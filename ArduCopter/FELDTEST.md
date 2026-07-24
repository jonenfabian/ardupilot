# FIELD TEST — Throw Detection + Power Climb

**Goal:** Fly two test cases from the catapult. In both, after throw detection the
drone first keeps **climbing for several seconds at a fixed thrust** (power climb)
so the sensors can recover from the launch, and only then transitions into the
normal hover recovery.

- **Test case 1:** standard detection + power climb
- **Test case 2:** early detection (motors start earlier) + power climb
- *(Optional: baseline reference = standard detection without power climb, as in the July tests)*

**Firmware base:** exactly **ArduCopter 4.6.3 (`92b0cd78`)** — the same version as
the July baseline flights. Branch: `fabian/throw-detect-variants-4.6`
(https://github.com/jonenfabian/ardupilot/tree/fabian/throw-detect-variants-4.6)

**You need:** laptop with Mission Planner, USB cable, cleared test site with
**plenty of airspace above** (see chapter 5), emergency disarm within reach.

> **Flying test round 3 (after the July 22 flights)?** The concrete plan —
> firmware version to check, parameter values and what to record per throw —
> is in **chapter 9**.

---

## 1. Building and flashing the firmware

> **Use your own workflow if you have one.** Any build/flash/parameter method
> you normally use is fine (waf --upload, QGroundControl, other GCS, …) — the
> steps in this guide are just one known-good reference path. All that matters
> is the result: this branch's firmware on the board (check chapter 1.3) and
> the parameter values from chapters 3–4.

### 1.1 Build (`.apj`)

> **No build needed for CubeOrangePlus:** a prebuilt firmware of this branch
> is committed at `firmware-builds/arducopter-CubeOrangePlus.apj` — download
> that file and continue with chapter 1.2. Building it yourself is only
> required for other boards or after code changes.

```text
git fetch && git checkout fabian/throw-detect-variants-4.6
git submodule update --init --recursive
./waf configure --board CubeOrangePlus
./waf copter
```

Result: `build/CubeOrangePlus/bin/arducopter.apj`.

> **If the build hangs at "Processing dronecangen" (zero CPU output for
> minutes):** the latest `dronecan` Python package (1.0.27) has a parser
> regression with this branch's generator. Fix:
> `pip install 'dronecan==1.0.26' 'setuptools<81'` and rebuild.

### 1.2 Flashing in Mission Planner

1. Back up parameters first: connect → **CONFIG → Full Parameter List → Save to file**.
2. Disconnect. Plug the autopilot in via USB directly (no hub).
3. **SETUP → Install Firmware** → click **Load custom firmware** at the bottom.
   (Link not visible? Set **CONFIG → Planner → Layout: Advanced**.)
4. Select the built `arducopter.apj`, wait for the messages up to `Upload Done`.
5. Wait a few seconds, then **Connect**.

### 1.3 Verifying the right firmware is running

**CONFIG → Full Parameter List → Refresh Params** → type `THROW_CLIMB` into the
search box at the top right. If `THROW_CLIMB_S` and `THROW_CLIMB_THR` appear, the
correct firmware is installed. If they are missing → wrong firmware, back to 1.2.

The exact build is shown under **DATA → Messages** right after connecting, e.g.
`ArduCopter V4.6.3 (fdcaee40)`. The hash in parentheses identifies the commit
the firmware was built from — chapter 9 says which one the current test round
expects.

### 1.4 No hardware? Try it in the simulator first (SITL, no build needed)

Prebuilt simulator binaries of this branch live in `sitl-builds/` — no
compiling, no flashing, no drone:

- **Windows:** download `sitl-builds/ArduCopter-SITL-Windows.zip`, extract it
  **completely** (the `cyg*.dll` files must sit next to the exe), double-click
  `start-sitl.bat`. If SmartScreen complains: *More info → Run anyway*; accept
  the firewall prompt. Then in Mission Planner pick connection type **TCP**,
  host `127.0.0.1`, port `5760` and press Connect. All `THROW_*` parameters
  from chapters 3–4 are available exactly as on the real board.
- **Linux / WSL:** `sitl-builds/arducopter-sitl-linux-x86_64` is the same
  firmware built for Linux; run it with `--model +` and connect the same way.
- See `sitl-builds/README.md` for details.

> **If you rebuild the Windows SITL yourself** (GitHub-Actions workflow
> "Cygwin Build", or `Tools/scripts/cygwin_build.sh` in a local Cygwin):
> the Cygwin repo dropped the packages this workflow historically used —
> you need `python39`/`procps-ng` (not `python37`/`procps`), the current
> `gcc-g++`, and the `dronecan==1.0.26` pin from chapter 1.1 applies in
> Cygwin too. The workflow file on this branch already contains all of
> that, plus retrying downloads because cygwin.com is flaky from CI runners.

---

## 2. Setting parameters in Mission Planner — step by step

This is how you change **every** parameter in this guide:

1. Connect the drone: select the COM port at the top right (or AUTO), baud **115200**, **Connect**.
2. Click **CONFIG** in the top menu bar.
3. Select **Full Parameter List** in the left-hand menu.
4. Type the parameter name (e.g. `THROW`) into the **search box** at the top
   right — the list filters automatically.
5. Click into the **Value** column of the parameter's row and enter the new value.
6. Click **Write Params** on the right — only this stores the value on the drone.
7. To double-check, click **Refresh Params** and read the value back.

The drone's status messages (e.g. `throw detected`, `power climb`) appear under
**DATA** (top left) → **Messages** tab (bottom left).

---

## 3. Fixed values (set once before all trials)

Set all of these following the chapter 2 procedure, then **Write Params** once:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| `THROW_TYPE` | `0` | upward throw |
| `THROW_ALT_MIN` | `0` | no minimum height for detection |
| `THROW_ALT_MAX` | `0` | no maximum height for detection |
| `THROW_NEXTMODE` | `5` | after the catch: Loiter |
| `THROW_MOT_START` | `0` | props off before detection |
| `THROW_ACCEL_MAX` | `0.7` | "launch impulse over" threshold |
| `THROW_DET_MS` | `80` | early-detection hold time (test case 2 only) |
| `THROW_SPD_MIN` | `1.5` | minimum speed (derived from the July logs — do not raise!) |
| `THROW_VELZ_MIN` | `1` | minimum climb rate |
| `THROW_IMPULSE_G` | `8` | launch impulse proof (test case 2 only) |
| `THROW_UPR_THR` | `0.5` | throttle while uprighting |
| `MOT_SPOOL_TIME` | `0.3` | motor spool-up time (s) |

Note for 4.6.3: after the power climb the drone climbs another fixed **+3 m**
above its current height while capturing (hard-coded in this version) — that is
normal.

---

## 4. Setting up the test cases

Per test case only change the following parameters (chapter 2 procedure) →
**Write Params** → launch.

### Test case 1 — standard detection + power climb

| Parameter | Value | What it does |
|-----------|-------|--------------|
| `THROW_DETECT` | `0` | standard throw detection — motors start on the classic (late) confirmation, exactly as in the July flights |
| `THROW_CLIMB_S` | `5` | power climb: after uprighting, climb for 5 seconds before height control takes over (`0` = feature off) |
| `THROW_CLIMB_THR` | `0.6` | thrust during that climb: 0.6 = 60 % throttle (must stay above hover, ~33 %) |
| `THROW_CLIMB_XY` | `1` | **new since the July 22 tests:** as soon as the position estimate has recovered (typically 1–3 s into the climb), the drone brakes the launch/wind drift and holds its position over the ground for the rest of the climb. `0` = old behaviour (climbs level, drifts with the wind) |

### Test case 2 — early detection + power climb

| Parameter | Value | What it does |
|-----------|-------|--------------|
| `THROW_DETECT` | `3` | early detection: first the ≥ 8 g launch jolt must be seen, then motors start right after the jolt ends — well before the standard confirmation |
| `THROW_CLIMB_S` | `5` | power climb: same as test case 1 |
| `THROW_CLIMB_THR` | `0.6` | climb thrust: same as test case 1 |
| `THROW_CLIMB_XY` | `1` | hold position during the climb: same as test case 1 |

Motors start much earlier than in test case 1 (right after the launch impulse
instead of waiting for the standard confirmation). If the early detection does not
trigger, the standard detection takes over automatically (message `fallback peak`
instead of `detect 3`) — please note it down, that is a trial result. The start
timing can be shifted via `THROW_DET_MS` (80 = early; larger = later; above ~300
the fallback takes over).

### Baseline reference (only if needed, matches the July flights)

| Parameter | Value | What it does |
|-----------|-------|--------------|
| `THROW_DETECT` | `0` | standard throw detection |
| `THROW_CLIMB_S` | `0` | power climb **off** — the drone behaves exactly like the July baseline flights |

---

## 5. Power climb: what happens, and what you can tune

**Sequence:** detection → motors spool up → drone uprights itself → **climbs
vertically for `THROW_CLIMB_S` seconds at fixed throttle `THROW_CLIMB_THR`**
(ignoring the disturbed height/velocity estimates) → normal height control
(+3 m) → position hold → Loiter.

**Position hold during the climb (`THROW_CLIMB_XY`, new):** with `1` (default)
the drone watches its own estimator during the climb and, as soon as the
estimate is trustworthy again (typically 1–3 s after launch — message
`power climb - holding position`), it brakes the horizontal drift and holds its
position over the ground while continuing to climb. The July 22 logs showed
5–38 m of drift and up to 8 m/s ground speed at the hand-over; this closes that
gap. The climb thrust itself stays open-loop either way.

**The two tuning dials:**

| Parameter | Meaning | Values to test |
|-----------|---------|----------------|
| `THROW_CLIMB_S` | Climb duration in seconds. `0` = feature off. | `3` → `5` → `10` (increase only if needed) |
| `THROW_CLIMB_THR` | Thrust during the climb. `0.5` = 50 %, `0.6` = 60 %, `0.7` = 70 % throttle. | Start at `0.6`; **must stay above hover throttle (~0.33)**, otherwise it descends! |

**How high does it climb? (estimate — verify against the first logs!)**
At thrust 0.6 the drone initially accelerates upwards at roughly 8 m/s² (drag
reduces this in reality). Rough figures including the launch momentum:

| `THROW_CLIMB_S` | expected altitude gain |
|-----------------|------------------------|
| 3 s | ~40–70 m |
| 5 s | ~100–150 m |
| 10 s | 200 m+ — **only with generous, cleared airspace!** |

Add ~10–25 m of overshoot after the climb ends until height control has bled off
the momentum. `THROW_ALT_MAX` does **not** limit this (it only gates detection,
not the climb).

**Aborting in flight:** a mode change on the transmitter (e.g. to Loiter/AltHold)
ends everything immediately — thumb on the switch. Second option: set
`THROW_CLIMB_S` to `0` on the laptop + Write Params — the climb ends instantly.

---

## 6. Procedure per throw

1. Test case parameters set (chapter 4), **Write Params**.
2. Select flight mode **Throw** (mode 18), then arm. **Do not carry the armed
   drone** — arm only once it sits in the catapult.
3. Launch.
4. Check the messages under **DATA → Messages**. Expected sequence:
   - `waiting for throw`
   - `throw detected - spooling motors (detect 0)` or `(detect 3)`
     — in test case 2 possibly `(fallback peak)` = early detection missed, note it down!
   - `uprighted - power climb 5.0s`
   - `power climb - holding position` (only with `THROW_CLIMB_XY 1`, once the
     estimator has recovered — if it never appears during a climb, note it down)
   - `power climb done - controlling height`
   - `height achieved - controlling position`
5. Record: test case, parameter values, behaviour (tumbling? altitude? smooth catch?), save the log.

---

## 7. Safety

- Props, people, clear field; emergency disarm within reach. In test case 2 the
  props spin up only a few metres above the catapult — nobody above the exit path.
- **Airspace:** mind the altitude-gain table in chapter 5; start with 3 s.
  Keep line of sight.
- Never set `THROW_CLIMB_THR` below hover throttle (~0.33).
- After a misfire (launch without flight): **disarm, then re-arm.**
- **EKF failsafe (fixed since the July 22 tests):** flights 6 and 8 on July 22
  aborted into LAND ~2 s after launch because the launch shock trips the EKF
  variance failsafe — a coin toss at the default `FS_EKF_THRESH 0.8` (every
  launch exceeded the threshold; only the duration varied). The firmware now
  defers this failsafe during the open-loop phases (waiting, uprighting, power
  climb) where the estimate is not used for control anyway; it re-arms the
  moment height control begins. `FS_EKF_THRESH` can therefore stay at `0.8` —
  no more raising it for the campaign. All other failsafes stay fully active.
- The early-detection methods 1–5 and the fallback safety net remain in the
  firmware; the standard test plan however is only the two cases above.

---

## 8. Technical notes (for the analysis)

- Branch `fabian/throw-detect-variants-4.6`, based on tag `Copter-4.6.3` (`92b0cd78`).
- Code: `ArduCopter/mode_throw.cpp`; parameters in `Parameters.cpp` (indices
  identical to the master branch — values survive firmware swaps).
- Log analysis: THRO message, **stage numbers in this build:**
  0 Disarmed, 1 Detecting, 2 Spool, 3 Uprighting, **4 PowerClimb**,
  5 HgtStabilise, 6 PosHold.
- SITL autotests: `ThrowMode`, `ThrowModeEarlyDetect`, `ThrowModeDetectFallback`,
  `ThrowModePowerClimb`, `ThrowModePowerClimbEKFFailsafe` (failsafe deferred
  during the climb, fires again afterwards), `ThrowModePowerClimbXYHold`
  (drift braked once the estimator recovers).

## 9. Test round 3 — plan (after the July 22 flights)

**What changed since July 22:** the two failed climbs (flights 6 and 8) were
caused by the EKF variance failsafe forcing LAND ~2 s after launch — a timing
coin toss present in *every* launch. The firmware now defers that failsafe
during the open-loop phases and re-arms it once height control begins
(chapter 7). In addition the climb now brakes the launch/wind drift and holds
position over the ground as soon as the sensors have recovered
(`THROW_CLIMB_XY`, chapter 5).

**Firmware:** flash the prebuilt `firmware-builds/arducopter-CubeOrangePlus.apj`
from this branch. After connecting, **DATA → Messages** must show
**`ArduCopter V4.6.3 (fdcaee40)`** — if it shows `73872c44` you are still on the
July 22 build.

**Parameters (test case 1, chapter 2 procedure):**

| Parameter | Value | Note |
|-----------|-------|------|
| `THROW_DETECT` | `0` | unchanged |
| `THROW_CLIMB_S` | `7` | your July 22 value was fine — the failures were never caused by the duration |
| `THROW_CLIMB_THR` | `0.45` | suggestion: `0.38` is barely above hover, so the first seconds gain almost no height; flight 3 flew `0.45` cleanly. Keep `0.38` if airspace is tight |
| `THROW_CLIMB_XY` | `1` | new, default — position hold during the climb |
| `FS_EKF_THRESH` | `0.8` | back to/stays at default — the relaxation to 1.0 is obsolete |

**What to verify and record per throw:**

1. **No failsafe aborts:** every throw must complete the full climb now. A
   LAND drop mid-climb would be a *new* failure mode — flag it and save the log.
2. **Position hold engagement:** note the time from launch until the
   `power climb - holding position` message (expected 1–3 s). If it never
   appears during a climb, note that too — the climb still completes, just
   drifting like before.
3. **Drift:** compare hand-over point vs. launch point with the July 22
   flights (5–38 m back then). The drone should visibly brake and climb over a
   fixed ground point.
4. **Optional A/B:** if throws to spare, two launches with `THROW_CLIMB_XY 0`
   under the same wind to quantify the drift difference.

Test case 2 (early detection) is unchanged and stays optional.

Sources:

- https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html
- https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html
