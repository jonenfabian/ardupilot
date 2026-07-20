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

---

## 1. Building and flashing the firmware

### 1.1 Build (`.apj`)

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

### Test case 2 — early detection + power climb

| Parameter | Value | What it does |
|-----------|-------|--------------|
| `THROW_DETECT` | `3` | early detection: first the ≥ 8 g launch jolt must be seen, then motors start right after the jolt ends — well before the standard confirmation |
| `THROW_CLIMB_S` | `5` | power climb: same as test case 1 |
| `THROW_CLIMB_THR` | `0.6` | climb thrust: same as test case 1 |

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
- If a trial aborts with an "EKF Failsafe" message (drone switches to LAND in the
  middle of the climb): for this trial campaign `FS_EKF_THRESH` may be raised
  from `0.8` to `1.0` (chapter 2 procedure). **Restore it after the tests.**
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
  `ThrowModePowerClimb`.

Sources:

- https://ardupilot.org/planner/docs/mission-planner-configuration-and-tuning.html
- https://ardupilot.org/copter/docs/common-loading-firmware-onto-pixhawk.html
