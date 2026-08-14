# Phase 6 Gap Analysis: Authentic Tamagotchi Care Loop

**Written:** 2026-08-11
**Basis:** Full read of `TamagotchiActivity.h`/`.cpp` (795 lines) at origin HEAD `2b78b00`, against parent's checklist (msg 3304): hunger/happiness decay rates, sleep cycle, discipline, sickness, death and evolution branching.

## What's already real (confirmed by source, not assumed)

- **Hunger/happiness/energy decay**: genuine RTC-time-driven meters (`kHungerDecaySeconds=5min`, `kHappinessDecaySeconds=8min`, `kEnergyDecaySeconds=10min` per point lost), computed proportionally from elapsed wall-clock time in `tick()`, not a per-frame/per-tap fake.
- **Poop cycle**: accumulates on a real timer (`kPoopIntervalSeconds=12min`), caps at `kMaxPoop=4`, cleared by the Clean action.
- **Sickness**: poop pinned at max for `kSicknessGraceSeconds` (20min) continuously → `sick=1`. Real, timer-gated, not random.
- **Death**: any of hunger/happiness/energy pinned at 0 continuously for `kNeglectDeathSeconds` (6h) → Dead. Real neglect mechanic.
- **Attention-call system**: a call starts when a meter crosses `kCallThreshold`, throttled by `kMinCallGapSeconds`; matching icon resolves free, Discipline resolves any call but always costs a care mistake (explicit anti-exploit fix from `a8f0793`); unanswered calls auto-expire and cost a mistake too.
- **Evolution gating**: age-gated (30min Baby→Child, 60min Child→Adult) AND care-gated (`careMistakes <= kMaxMistakesToEvolve`); a failed check resets both the stage clock and the mistake count (the `5737a5b` fix) rather than permanently freezing the pet.
- **Persistence**: versioned flash-backed state, survives reboot, same idiom as `StatsManager`.

This is a genuinely working care loop, not window dressing — the mechanics-first bar parent set ("needs to BE a Tamagotchi, not merely look like one") is substantially met for the meters/sickness/death/evolution axis.

## What's missing, ranked by how far it is from "authentic"

### 1. Sleep cycle — not implemented at all
The `kLightGain` constant's own comment claims the Light icon is "energy, via the Light icon (put pet to sleep/wake)", but `toggleLight()`'s actual body is just:
```cpp
state.energy = clampToByte(... + kLightGain);
resolveCall(CallKind::Energy);
```
No `isAsleep` flag, no day/night schedule, no dimmed/dark screen while asleep, no "waking the pet early is a care mistake" penalty (a signature Tamagotchi/Tamagotchi Uni mechanic — the room light toggle is supposed to put the pet to bed for the night and staying up is what drains its energy in the first place, not a redundant top-up button). Currently energy decay and the Light action have no real relationship to a sleep state; Light is just a second Play-like refill button under a misleading name.

**This is the single clearest gap.** A real fix needs: an `isAsleep` bool (or a derived day/night window off `nowEpoch()`), energy decay rate that differs (or halts) while asleep, a screen state for "sleeping" (dim/eyes-closed sprite), and Light toggling that state rather than instantly adding a stat.

### 2. Discipline — action only, not a stat
`dispatchIconAction()` maps `Icon::Discipline` straight to `discipline()` → `resolveAnyCall()` — it's a call-resolver with an intentional cost (per `a8f0793`'s anti-exploit fix), never a persisted, accumulating value. Real Tamagotchi discipline is a tracked meter that rises with consistent scolding/consistent care and typically influences which adult form the pet evolves into. Right now there's no `disciplineLevel` field in `State` at all — the Discipline icon does something real (it's not a no-op), but it doesn't feed anything downstream.

### 3. Evolution branching — confirmed strictly linear
`Stage` enum is `Egg → Baby → Child → Adult → Dead`, one path, no forks. `evolveIfReady()` only ever asks "is care good enough to progress," never "which adult form does this care history produce." Authentic Tamagotchi (and Tamagotchi Uni specifically) branches the adult form based on accumulated care-quality signals (typically discipline level + how well-fed/neglected the pet was across its life). This is directly downstream of gap #2 — there's no discipline stat to branch on yet, and no second/third adult sprite to branch to even if there were.

### 4. Visual style (TAMA-02, tracked separately in REQUIREMENTS.md)
Not a mechanics gap but worth naming alongside these since parent's PROJECT.md explicitly separates "mechanics correctness first, Tamagotchi Uni visual style second." Current Care Menu (`drawCareMenu()`) is a generic 4-column icon grid with rounded-rect selection highlight; stats screen uses heart-pip bars (`drawHeartPips()`). Functional and clean, but not shaped like Tamagotchi Uni's specific screen layout (typically a full-screen pet view with an icon ring/dock, not a grid page). TAMA-02 in REQUIREMENTS.md already tracks this as not-yet-started; no new finding here, just confirming it's real and separate from the three mechanics gaps above.

## What this means as the Phase 6 forward plan

In priority order (biggest authenticity gap vs. smallest/cheapest to build first is a real tradeoff — flagging both angles rather than picking for parent):

1. **Sleep cycle** — biggest single gap, self-contained (new `isAsleep` state + a light schedule + Light icon rewritten to toggle it), doesn't depend on anything else being built first.
2. **Discipline stat** — small state addition (`disciplineLevel: uint8_t` in `State`, versioned-blob bump to v3), `discipline()` increments it instead of/alongside resolving calls.
3. **Evolution branching** — depends on #2 existing first (need a real signal to branch on); needs new adult-stage sprites (Stuart would need to supply art, same as the existing pet/icon sprites) and a second `Stage` value or an adult-variant field separate from `Stage`.

None of these are researched-but-unwritten anymore — this doc *is* the Phase 6 plan going forward, per parent's framing ("that gap is the Phase 6 plan, and it's writable now from what exists"). Build order and whether to tackle all three vs. ship sleep-cycle alone first is parent's call, not decided here.
