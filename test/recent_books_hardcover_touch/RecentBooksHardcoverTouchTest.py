#!/usr/bin/env python3
"""Deterministic guard for the suggested-match touch contract."""
from pathlib import Path
import re
source = (Path(__file__).resolve().parents[2] / 'src/activities/home/RecentBooksActivity.cpp').read_text()
choice = re.search(r'if \(syncState == SyncState::Choosing\) \{(.*?)\n  \}\n  runOneMetadataSync', source, re.S)
assert choice, 'suggested-match input branch missing'
body = choice.group(1)
for required in [
    'handleListTouch(touchChoice, static_cast<int>(choices), listTop, listHeight, true)',
    'ListTouchResult::Activated) finishCandidateChoice(candidateIndex < syncCandidates.size())',
    'SwipeDir::Up', 'SwipeDir::Down', 'nextPageIndex(candidateIndex, choices, pageItems)',
    'previousPageIndex(candidateIndex, choices, pageItems)',
    'Button::Back)) { finishCandidateChoice(false)',
]:
    assert required in body, f'missing touch/scroll/cancel behavior: {required}'
render = re.search(r'if \(syncState == SyncState::Choosing\) \{(.*?)\n  \}\n\n  // The explicit command', source, re.S)
assert render, 'suggested-match render branch missing'
for required in ['"Suggested matches"', '"Skip / Cancel"', 'contentTop + suggestedMatchesHeadingHeight']:
    assert required in render.group(1), f'missing visible UI affordance: {required}'
print('RecentBooks suggested-match touch contract: PASS')
