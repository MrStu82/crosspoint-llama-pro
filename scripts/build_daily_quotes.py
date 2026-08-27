#!/usr/bin/env python3
"""Build the Home daily-quote pack from English Wikiquote.

Three stages, run in order:

  fetch       docs/daily-quotes-works.tsv -> one cached revision per work
  candidates  cached wikitext -> filtered candidate quotes (deterministic)
  emit        a selection of candidate ids -> DailyQuoteData.inc + sources TSV
              + a plain-text review list

The verbatim rail is the point of this script. Quote text is only ever
*extracted* from a fetched revision, never composed: every candidate, and again
every emitted record, must be a byte-for-byte substring of that revision's
wikitext after markup stripping. A candidate that fails is dropped, never
repaired. Ranking (which candidates are memorable) happens outside this script
and comes back as a list of ids, so no model ever touches quote text.

Usage:
  build_daily_quotes.py fetch [--refresh]
  build_daily_quotes.py candidates
  build_daily_quotes.py emit --selection <file with one candidate id per line>
"""

from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import re
import subprocess
import sys
import time
import unicodedata
import urllib.parse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORKS_TSV = ROOT / "docs" / "daily-quotes-works.tsv"
CACHE_DIR = ROOT / "docs" / ".quote-cache"
CANDIDATES_JSON = CACHE_DIR / "candidates.json"
OUT_INC = ROOT / "src" / "util" / "DailyQuoteData.inc"
OUT_TSV = ROOT / "docs" / "daily-quotes-sources.tsv"
OUT_REVIEW = ROOT / "docs" / "daily-quotes-review.txt"

API = "https://en.wikiquote.org/w/api.php"
RECORD_COUNT = 366
MIN_LEN = 40
MAX_LEN = 115
MAX_PER_WORK = 4
MAX_PER_AUTHOR = 6

# ---------------------------------------------------------------------------
# fetch
# ---------------------------------------------------------------------------


def slug(page: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", page).strip("_")


def read_works() -> list[dict]:
    rows = []
    lines = WORKS_TSV.read_text(encoding="utf-8").splitlines()
    header = lines[0].split("\t")
    for line in lines[1:]:
        if not line.strip():
            continue
        rows.append(dict(zip(header, line.split("\t"))))
    return rows


# Wikimedia asks for a descriptive User-Agent and throttles anonymous bursts;
# one batched query per 20 titles with a pause between them stays well inside it.
UA = "CrossPointReaderQuotePack/1.0 (e-reader firmware daily quotes; contact via github.com/crosspoint-reader)"
BATCH = 20
BATCH_PAUSE_S = 3.0


def curl_json(url: str, params: dict) -> dict:
    """POST a form-encoded API query. Raises with the body when it is not JSON,
    so a rate-limit notice reads as one instead of a JSON parse error."""
    out = subprocess.run(
        [
            "curl", "-sS", "--max-time", "90",
            "-H", f"User-Agent: {UA}",
            "-H", "Content-Type: application/x-www-form-urlencoded",
            "--data-binary", "@-",
            url,
        ],
        input=urllib.parse.urlencode(params),
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    try:
        return json.loads(out)
    except json.JSONDecodeError:
        raise RuntimeError(" ".join(out.split())[:200]) from None


def cmd_fetch(args) -> int:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    works = read_works()
    todo = [w for w in works if args.refresh or not (CACHE_DIR / f"{slug(w['wikiquote_page'])}.json").exists()]
    cached = len(works) - len(todo)
    ok = 0
    missing: list[str] = []

    for start in range(0, len(todo), BATCH):
        chunk = todo[start : start + BATCH]
        params = {
            "action": "query",
            "prop": "revisions",
            "rvprop": "content|ids",
            "rvslots": "main",
            "format": "json",
            "formatversion": "2",
            "redirects": "1",
            "titles": "|".join(w["wikiquote_page"] for w in chunk),
        }
        data = None
        for attempt in range(5):  # throttling is the expected failure: back off, do not skip
            try:
                data = curl_json(API, params)
                break
            except Exception as e:  # network/proxy hiccup: report, never invent
                err = str(e)
                if attempt == 4:
                    missing.extend(f"{w['wikiquote_page']}: {err}" for w in chunk)
                else:
                    wait = 15 * (attempt + 1)
                    print(f"RETRY in {wait}s after: {err[:100]}", file=sys.stderr, flush=True)
                    time.sleep(wait)
        if data is None:
            continue
        q = data.get("query")
        if not q:
            missing.extend(f"{w['wikiquote_page']}: {str(data)[:120]}" for w in chunk)
            continue

        # requested title -> final title, following normalisation then redirects
        alias: dict[str, str] = {}
        for kind in ("normalized", "redirects"):
            for m in q.get(kind, []):
                alias[m["from"]] = m["to"]

        def resolve(name: str) -> str:
            seen = set()
            while name in alias and name not in seen:
                seen.add(name)
                name = alias[name]
            return name

        pages = {p["title"]: p for p in q.get("pages", [])}
        for w in chunk:
            page = w["wikiquote_page"]
            p = pages.get(resolve(page))
            if p is None or p.get("missing") or not p.get("revisions"):
                missing.append(f"{page}: no page")
                continue
            rev = p["revisions"][0]
            wikitext = rev["slots"]["main"]["content"]
            if rejected_source_page(p["title"], wikitext):
                missing.append(f"{page}: rejected navigation/disambiguation/non-literary page")
                continue
            record = {
                "requested_page": page,
                "canonical_title": p["title"],
                "revision_id": rev["revid"],
                "source_url": (
                    "https://en.wikiquote.org/w/index.php?title="
                    f"{p['title'].replace(' ', '_')}&oldid={rev['revid']}"
                ),
                "retrieved_at": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat(),
                "title": w["title"],
                "author": w["author"],
                "era": w["era"],
                "region": w["region"],
                "wikitext": wikitext,
            }
            (CACHE_DIR / f"{slug(page)}.json").write_text(
                json.dumps(record, ensure_ascii=False), encoding="utf-8"
            )
            ok += 1
        time.sleep(BATCH_PAUSE_S)

    for m in missing:
        print(f"MISS  {m}", file=sys.stderr)
    print(f"fetched={ok} cached={cached} missing={len(missing)} of {len(works)}")
    return 0


# ---------------------------------------------------------------------------
# markup stripping (single definition, used for both extraction and the rail)
# ---------------------------------------------------------------------------

TEMPLATE_LITERALS = {
    "{{'}}": "'",
    "{{'s}}": "'s",
    "{{\"}}": '"',
    "{{'\"}}": "'\"",
    "{{spaces}}": " ",
    "{{nbsp}}": " ",
    "{{ndash}}": "–",
    "{{mdash}}": "—",
}


def strip_markup(text: str) -> str:
    s = text
    s = re.sub(r"<!--.*?-->", "", s, flags=re.S)
    s = re.sub(r"<ref[^>/]*/>", "", s)
    s = re.sub(r"<ref.*?</ref>", "", s, flags=re.S)
    # A line-break tag is word separation in a quotation, never deletion.
    # Removing it concatenates words (e.g. "admiredis") and corrupts text.
    s = re.sub(r"<br\s*/?>", " ", s, flags=re.I)
    s = re.sub(r"<[^>]+>", "", s)
    for lit, rep in TEMPLATE_LITERALS.items():
        s = s.replace(lit, rep)
    # Innermost-first template removal; some templates carry no readable text.
    for _ in range(8):
        new = re.sub(r"\{\{[^{}]*\}\}", "", s)
        if new == s:
            break
        s = new
    s = re.sub(r"\[\[[^\]|]*\|([^\]|]*)\]\]", r"\1", s)
    s = re.sub(r"\[\[([^\]|]*)\]\]", r"\1", s)
    s = re.sub(r"\[(?:https?|//)\S+\s+([^\]]*)\]", r"\1", s)
    s = re.sub(r"\[(?:https?|//)\S+\]", "", s)
    s = s.replace("'''''", "").replace("'''", "").replace("''", "")
    s = html.unescape(s)
    s = s.replace(" ", " ")
    s = re.sub(r"[ \t]+", " ", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def page_plaintext(wikitext: str) -> str:
    """The whole revision, stripped. The rail checks membership in this."""
    return strip_markup(wikitext)


def rejected_source_page(canonical_title: str, wikitext: str) -> bool:
    """Reject disambiguation/navigation and screen-adaptation sources.

    The corpus is literary text. Wikiquote hub pages can look like valid quote
    pages to a line extractor while their bullets are merely adaptation links.
    Keep this test at both fetch and candidate time so stale cache never leaks
    back into the pack.
    """
    low = wikitext.lower()
    if "{{disambig" in low or canonical_title.lower().endswith("(film)"):
        return True
    bullets = [line.lstrip("* ").strip() for line in wikitext.splitlines()
               if line.lstrip().startswith("*")]
    nav = [b for b in bullets if re.fullmatch(r"\[\[[^\]]+\]\]", b)]
    return len(bullets) >= 6 and len(nav) * 100 >= len(bullets) * 70


# ---------------------------------------------------------------------------
# candidates
# ---------------------------------------------------------------------------

SPEAKER_RE = re.compile(r"^'''([A-Z][A-Za-z .'’-]{1,28})''':\s*(.+)$")
FILE_RE = re.compile(r"^\[\[(?:File|Image):[^|]*\|(.*)\]\]\s*$")
SENTENCE_SPLIT = re.compile(r"(?<=[.!?”’])\s+(?=[\"“‘A-Z])")

BAD_CHARS = set("[]{}|=<>*#•")
TRAILING_CONJUNCTIONS = {
    "and", "but", "or", "nor", "for", "yet", "so", "because", "although", "though",
    "while", "whereas", "if", "unless", "until", "when", "which", "that", "than",
    "the", "a", "an", "of", "to", "in", "on", "with", "as", "at", "by", "from",
    "is", "was", "were", "be", "been", "his", "her", "their", "its", "my", "our",
}
LEADING_BAD = {"and", "but", "or", "nor", "yet", "so", "then", "which", "that", "because"}
META_MARKERS = (
    "quoted in", "as quoted", "translat", "variant:", "ch. ", "chapter ", "p. ",
    "pp. ", "act ", "scene ", "vol. ", "book i", "introduction", "preface",
    "wikipedia", "wikisource", "isbn", "http",
)


def balanced(s: str) -> bool:
    if s.count('"') % 2:
        return False
    if s.count("“") != s.count("”"):
        return False
    if "‘" in s and s.count("‘") != s.count("’"):
        return False
    for open_c, close_c in (("(", ")"), ("[", "]")):
        if s.count(open_c) != s.count(close_c):
            return False
    return True


def acceptable(s: str) -> bool:
    if not (MIN_LEN <= len(s) <= MAX_LEN):
        return False
    if any(c in BAD_CHARS for c in s):
        return False
    # Adjacent quote marks mean two dialogue turns were run together by the
    # source's own formatting; on a one-line home screen that reads as a glitch.
    if "''" in s or '""' in s or "  " in s:
        return False
    if any(unicodedata.category(c) in ("Cc", "Cf") for c in s):
        return False
    low = s.lower()
    if any(m in low for m in META_MARKERS):
        return False
    if not balanced(s):
        return False
    if not (s[0].isupper() or s[0] in "\"“‘"):
        return False
    if s[-1] not in ".!?”’\"":
        return False
    words = s.split()
    if len(words) < 6:
        return False
    if words[0].lower().strip("\"“‘") in LEADING_BAD:
        return False
    last = re.sub(r"[^A-Za-z]", "", words[-1]).lower()
    if last in TRAILING_CONJUNCTIONS:
        return False
    # A complete thought needs a verb-ish middle; a bare noun phrase rarely ends
    # in a full stop, so the terminal-punctuation rule above carries most of it.
    return True


def candidates_from_line(line: str) -> list[tuple[str, str]]:
    """Return (speaker, text) pairs extractable from one wikitext line."""
    out: list[tuple[str, str]] = []
    body = line
    speaker = ""

    m = FILE_RE.match(line.strip())
    if m:
        body = m.group(1)
        # caption may itself be pipe-separated params; take the longest field
        body = max(body.split("|"), key=len)
    elif line.startswith("*"):
        body = line.lstrip("*").strip()
    else:
        return out

    sm = SPEAKER_RE.match(body)
    if sm:
        speaker = sm.group(1).strip()
        body = sm.group(2)

    whole = strip_markup(body)
    if whole:
        out.append((speaker, whole))
    for bold in re.findall(r"'''(.+?)'''", body):
        t = strip_markup(bold)
        if t:
            out.append((speaker, t))
    if len(whole) > MAX_LEN:
        for sentence in SENTENCE_SPLIT.split(whole):
            sentence = sentence.strip()
            if sentence:
                out.append((speaker, sentence))
    return out


def cmd_candidates(_args) -> int:
    works = {slug(w["wikiquote_page"]): w for w in read_works()}
    results = []
    for path in sorted(CACHE_DIR.glob("*.json")):
        if path.name == "candidates.json":
            continue
        if path.stem not in works:  # do not revive stale cache after a page remap
            continue
        rec = json.loads(path.read_text(encoding="utf-8"))
        if rejected_source_page(rec["canonical_title"], rec["wikitext"]):
            continue
        plain = page_plaintext(rec["wikitext"])
        seen: set[str] = set()
        for line in rec["wikitext"].splitlines():
            for speaker, text in candidates_from_line(line):
                if text in seen or not acceptable(text):
                    continue
                if text not in plain:  # the rail, applied at extraction time
                    continue
                seen.add(text)
                results.append(
                    {
                        "id": f"{path.stem}:{len(seen):03d}",
                        "work_slug": path.stem,
                        "title": rec["title"],
                        "author": rec["author"],
                        "era": rec["era"],
                        "region": rec["region"],
                        "character": speaker,
                        "quote": text,
                    }
                )
        _ = works  # works list is the fetch input; kept for provenance only
    CANDIDATES_JSON.write_text(json.dumps(results, ensure_ascii=False, indent=1), encoding="utf-8")
    by_work: dict[str, int] = {}
    for r in results:
        by_work[r["work_slug"]] = by_work.get(r["work_slug"], 0) + 1
    print(f"candidates={len(results)} works={len(by_work)}")
    return 0


# ---------------------------------------------------------------------------
# emit
# ---------------------------------------------------------------------------


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def cmd_emit(args) -> int:
    cands = {c["id"]: c for c in json.loads(CANDIDATES_JSON.read_text(encoding="utf-8"))}
    wanted = [l.strip() for l in Path(args.selection).read_text(encoding="utf-8").splitlines() if l.strip()]

    caches: dict[str, dict] = {}
    plains: dict[str, str] = {}

    chosen: list[dict] = []
    per_work: dict[str, int] = {}
    per_author: dict[str, int] = {}
    seen_quotes: set[str] = set()
    dropped: list[str] = []

    for cid in wanted:
        c = cands.get(cid)
        if c is None:
            dropped.append(f"{cid}: unknown candidate id")
            continue
        ws = c["work_slug"]
        if ws not in caches:
            caches[ws] = json.loads((CACHE_DIR / f"{ws}.json").read_text(encoding="utf-8"))
            plains[ws] = page_plaintext(caches[ws]["wikitext"])
        # The rail, applied again at emit against a freshly stripped revision.
        if c["quote"] not in plains[ws]:
            dropped.append(f"{cid}: FAILED VERBATIM RAIL")
            continue
        if not acceptable(c["quote"]):
            dropped.append(f"{cid}: failed shape check")
            continue
        if c["quote"] in seen_quotes:
            dropped.append(f"{cid}: duplicate text")
            continue
        if per_work.get(c["title"], 0) >= MAX_PER_WORK:
            dropped.append(f"{cid}: work cap")
            continue
        if per_author.get(c["author"], 0) >= MAX_PER_AUTHOR:
            dropped.append(f"{cid}: author cap")
            continue
        seen_quotes.add(c["quote"])
        per_work[c["title"]] = per_work.get(c["title"], 0) + 1
        per_author[c["author"]] = per_author.get(c["author"], 0) + 1
        chosen.append(c)
        # The selection is a ranked list, deliberately longer than the pack so
        # the caps have something to fall back on. Take the best 366 that pass.
        if len(chosen) == RECORD_COUNT:
            break

    for d in dropped:
        print(f"DROP {d}", file=sys.stderr)
    print(f"selected={len(chosen)} works={len(per_work)} authors={len(per_author)}")
    if len(chosen) != RECORD_COUNT:
        print(f"ERROR: need exactly {RECORD_COUNT} records, have {len(chosen)}", file=sys.stderr)
        return 1

    inc = ["// Generated by scripts/build_daily_quotes.py from English Wikiquote (CC BY-SA).",
           "// Provenance for every record - page, revision id, retrieval time - is in",
           "// docs/daily-quotes-sources.tsv. Do not hand-edit either file.",
           "// Initialiser rows only - DailyQuote.cpp owns the array declaration."]
    tsv = ["record\tquote\tcharacter\ttitle\tauthor\tsource_url\trevision_id\tretrieved_at"]
    review = []
    for i, c in enumerate(chosen):
        cache = caches[c["work_slug"]]
        inc.append(
            f'    {{"{c_escape(c["quote"])}", "{c_escape(c["character"])}", '
            f'"{c_escape(c["title"])}", "{c_escape(c["author"])}"}},'
        )
        tsv.append(
            "\t".join(
                [
                    str(i),
                    c["quote"],
                    c["character"],
                    c["title"],
                    c["author"],
                    cache["source_url"],
                    str(cache["revision_id"]),
                    cache["retrieved_at"],
                ]
            )
        )
        attribution = ", ".join([x for x in (c["character"], c["title"], c["author"]) if x])
        review.append(f"{i + 1:3d}. {c['quote']}\n     -- {attribution}")

    OUT_INC.write_text("\n".join(inc) + "\n", encoding="utf-8")
    OUT_TSV.write_text("\n".join(tsv) + "\n", encoding="utf-8")
    OUT_REVIEW.write_text(
        f"CrossPoint daily quote pack v3 - {len(chosen)} records, "
        f"{len(per_work)} works, {len(per_author)} authors\n"
        "Source: English Wikiquote (CC BY-SA). Every line verified verbatim against the\n"
        "cited revision; per-record source_url/revision_id/retrieved_at in "
        "docs/daily-quotes-sources.tsv.\n\n" + "\n".join(review) + "\n",
        encoding="utf-8",
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--refresh", action="store_true")
    f.set_defaults(func=cmd_fetch)
    sub.add_parser("candidates").set_defaults(func=cmd_candidates)
    e = sub.add_parser("emit")
    e.add_argument("--selection", required=True)
    e.set_defaults(func=cmd_emit)
    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
