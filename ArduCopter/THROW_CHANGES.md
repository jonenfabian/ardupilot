# What This Branch Changes — Plain-Language Summary

Branch `fabian/throw-detect-variants-4.6`, based on exactly **ArduCopter 4.6.3**
(`92b0cd78`) — the same firmware version our July baseline flights used. Everything
described here lives inside **one flight mode**: Throw mode (catapult/hand-launch
start). The rest of the firmware is untouched.

For how to *operate* the test cases, see `FELDTEST.md`. This document explains
*what was changed and why*, for readers who don't work with the code.

---

## What the original does

In stock ArduCopter, Throw mode works like this:

```text
wait → detect the throw → spool up motors → level the drone → hold height → hold position → hand over to the next mode
```

The drone sits armed with motors off, watches its sensors for the "I have been
thrown" signature, and only then starts the motors and recovers into a hover.
Afterwards it hands control to a configurable follow-up mode (Loiter in our
setup) — from that point on, normal flight logic and any external guidance
system take over, exactly as before this branch.

## Change 1 — The throw detection is now selectable (was: one fixed method)

The original recognises a throw by a single pattern: "the drone is fast or in
free fall AND its climb rate is collapsing measurably". That fires relatively
late. We added five alternative detection methods that fire earlier — for
example "the catapult jolt is over and the drone is still climbing", or "first a
hard jolt of at least 8 g must be seen, then start early". One parameter
(`THROW_DETECT`, 0–5) selects the method. The default (0) is the unchanged
original behaviour.

## Change 2 — A safety net behind every new method

Whichever new detection is selected, the original detection keeps running in
parallel. If the new method misses the throw, the motors still start — at the
latest when the original would have fired. The radio message tells you which of
the two paths triggered (`detect N` vs `fallback peak`), so a missed early
detection is visible in the field and in the logs. This closes the one genuinely
dangerous gap of the early methods: without it, a missed detection would have
meant a ballistic crash with motors off.

## Change 3 — Hard-coded numbers became tunable parameters

Several values were baked into the original code — for example the throttle used
while the drone rights itself (fixed at 50 %). These are now Mission Planner
parameters, so field tuning needs no new firmware. We also corrected thresholds
using our July flight data: the analysis showed the drone *massively*
underestimates its own speed right after a catapult launch (the accelerometer
saturates at the ~25–30 g jolt, which corrupts the position/velocity estimator
for a few seconds). With the original speed threshold, no early detection would
ever have fired; the field guide now carries values derived from the real logs.

## Change 4 — The new "power climb" phase (the current test topic)

Between "level the drone" and "hold height" there is now an optional new phase:
the drone climbs for a configurable time (e.g. 5 s) at a configurable fixed
thrust (e.g. 60 %), and during this phase it deliberately **ignores its own
height and speed estimates** — precisely because those are unreliable for the
first seconds after a launch. Only afterwards does the normal height controller
take over. With duration = 0 (the default) the phase is completely off and the
mode behaves exactly as before. Setting the duration to 0 from the ground
station mid-climb aborts the phase instantly — a second abort lever besides the
transmitter mode switch.

## Change 5 — Automated tests

Every feature has a simulator test that flies a complete throw (simulated
launch, detection, climb, capture, return, landing): `ThrowMode` (stock
behaviour), `ThrowModeEarlyDetect` (all five early methods), 
`ThrowModeDetectFallback` (safety net takes over when the early method cannot
fire), `ThrowModePowerClimb` (both field test cases with the climb phase). They
run after every change; among other things they prove that the default settings
preserve the original behaviour.

## Change 6 — Documentation

`FELDTEST.md`: the field manual with step-by-step Mission Planner navigation,
copy-paste parameter tables for the two test cases, tuning tables, an
altitude-gain estimate for the climb phase, and safety notes.

## What was deliberately NOT changed

- The uprighting, height-hold and position-hold logic itself.
- The hand-over to the follow-up mode (`THROW_NEXTMODE`) — external systems
  take over after the recovery exactly as before.
- All failsafes (EKF, battery, fence). Note: Throw mode is *not* exempt from
  them — a severe EKF failsafe can still abort a climb; `FELDTEST.md` explains
  the trial-only mitigation.
- Everything outside Throw mode.

Total footprint: six files, roughly 700 changed lines, all contained in Throw
mode, implemented identically on this branch and on the master-based development
branch (`fabian/throw-detect-variants`) with matching parameter IDs, so
parameter values survive switching firmware between the two.

## Commits on this branch (newest last)

| Commit | Content |
|--------|---------|
| `ff0e141e98` | Backport of the selectable detection methods + safety-net fallback onto 4.6.3 |
| `dede4edf73` | New power-climb stage + its two parameters |
| `b22ab07b8c` | Simulator test for the power climb (both field test cases) |
| `a127507b03` | Field guide reworked into `FELDTEST.md` (two test cases, Mission Planner navigation) |
| `595bc7e262` | `FELDTEST.md` translated to English |
