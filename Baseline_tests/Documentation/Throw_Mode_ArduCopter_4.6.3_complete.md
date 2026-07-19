# Throw Mode in ArduCopter 4.6.3 — Complete Line-by-Line Reference

This document walks through **everything** that happens from the moment you arm the
vehicle in Throw mode until it hands control to the next mode. Every line of
`ArduCopter/mode_throw.cpp`, every parameter, every hidden constant, and every
controller call is explained. Source is the `Copter-4.6.3` tag.

Files involved:
- `ArduCopter/mode_throw.cpp` — the whole mode (the only file that is *Throw-specific*)
- `ArduCopter/mode.h` — the `ModeThrow` class, the `stage` enum, the two throw enums
- `ArduCopter/Parameters.cpp` — the 5 `THROW_*` parameters
- `ArduCopter/config.h` — the compile-time constants (`THROW_HIGH_SPEED`, `THROW_VERTICAL_SPEED`, `BRAKE_MODE_SPEED_Z`, `BRAKE_MODE_DECEL_RATE`)
- The shared attitude controller (`AC_AttitudeControl`) and position controller (`AC_PosControl`) libraries, which Throw *calls into* — this is where the bouncing actually happens.

---

## 0. The mental model first

Throw mode is a **state machine**. It does not do one thing; it does a sequence of
distinct things, and it advances from one to the next only when a specific
condition becomes true. The stages in order are:

```
Throw_Disarmed
   → Throw_Detecting
      → Throw_Wait_Throttle_Unlimited
         → Throw_Uprighting
            → Throw_HgtStabilise      <-- YOUR BOUNCING HAPPENS HERE
               → Throw_PosHold
                  → (switch to THROW_NEXTMODE)
```

The code comment at the top of `run()` only lists 5 stages, but there are actually
**6** — `Throw_Wait_Throttle_Unlimited` is a real stage that sits between detection
and uprighting. Keep that in mind; it matters for timing.

`run()` is called by the main scheduler at the loop rate (**400 Hz** on most
modern boards, and the comment says "should be called at 100 Hz or more"). So
everything below runs every 2.5 ms.

Each call of `run()` does two things, in this order:
1. **Transition logic** — a long `if / else if` chain that *may* advance `stage`.
2. **State processing** — a `switch (stage)` that *acts* on whatever stage we are
   now in (motor spool state, attitude demand, throttle, position controller).

Because transition and action are separate, the first loop after a transition
already runs the new stage's action code. Good to know when reading logs.

---

## 1. `init()` — what happens the instant you select Throw mode

```cpp
bool ModeThrow::init(bool ignore_checks)
{
#if FRAME_CONFIG == HELI_FRAME
    return false;                     // helis may never use Throw
#endif

    if (motors->armed()) {
        return false;                 // refuse to ENTER Throw if already armed/flying
    }

    stage = Throw_Disarmed;           // always start in Disarmed
    nextmode_attempted = false;       // we have not yet tried to hand off

    // horizontal pos-controller limits
    pos_control->set_max_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);
    pos_control->set_correction_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);

    // vertical pos-controller limits
    pos_control->set_max_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);
    pos_control->set_correction_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);

    return true;
}
```

Line by line:

- **HELI guard.** Traditional helis cannot use Throw. Returns `false`, mode change is rejected.
- **`motors->armed()` guard.** This is *the* reason the workflow is "select Throw
  first, then arm." If you are already armed (already flying, or armed on the
  ground) you cannot switch into Throw. So Throw is normally entered while
  disarmed; then you arm; then you throw.
- **`stage = Throw_Disarmed`.** The state machine always begins here.
- **`nextmode_attempted = false`.** A latch so the hand-off to the next mode is
  attempted exactly once.
- **`set_max_speed_accel_xy(default_wpnav_speed, BRAKE_MODE_DECEL_RATE)`** — configures
  the horizontal position controller's *output* limits. Max horizontal speed = your
  `WPNAV_SPEED` (default 1000 cm/s); max horizontal decel = `BRAKE_MODE_DECEL_RATE`
  = **750 cm/s²**.
- **`set_correction_speed_accel_xy(...)`** — same numbers, but this sets the limits
  on the *correction* (feed-back) part of the controller, i.e. how aggressively it
  is allowed to chase a position error.
- **`set_max_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE)`**
  — vertical limits. Up-speed = **250 cm/s**, down-speed = **250 cm/s**,
  vertical accel = **750 cm/s²**. `BRAKE_MODE_SPEED_Z` is 250, `BRAKE_MODE_DECEL_RATE` is 750.
- **`set_correction_speed_accel_z(...)`** — same for the vertical correction term.

**These four lines are extremely relevant to your bouncing problem.** They cap how
fast and how hard the height controller is *allowed* to arrest the fall. 250 cm/s
of vertical speed authority and 750 cm/s² of accel is not huge for a drone that has
been thrown and is moving fast in Z. If those caps are too low relative to the
energy in the throw, the controller can overshoot the target height and oscillate —
which is one plausible source of your up/down bouncing. (More on this in §8.)

Note what `init()` does **not** do: it does not touch the Z or XY controllers'
internal integrators yet. Those are initialised later, at the moment of the
`Throw_Uprighting → Throw_HgtStabilise` and `Throw_HgtStabilise → Throw_PosHold`
transitions.

---

## 2. `run()` part 1 — the transition chain

This runs top to bottom every loop. Only the **first** matching branch fires
(`else if`), so ordering matters.

### 2.1 Fell-back-to-disarmed guard
```cpp
if (!motors->armed()) {
    stage = Throw_Disarmed;
}
```
If motors are not armed for any reason, force the machine back to the start. This is
the safety net that resets everything if you disarm.

### 2.2 Disarmed → Detecting (fires the instant you arm)
```cpp
} else if (stage == Throw_Disarmed && motors->armed()) {
    gcs().send_text(..., "waiting for throw");
    stage = Throw_Detecting;
}
```
The moment you arm, you get the **"waiting for throw"** GCS message and enter
detection. The motors are still not spinning (unless `THROW_MOT_START = 1`).

### 2.3 Detecting → Wait_Throttle_Unlimited (the throw is detected)
```cpp
} else if (stage == Throw_Detecting && throw_detected()){
    gcs().send_text(..., "throw detected - spooling motors");
    copter.set_land_complete(false);
    stage = Throw_Wait_Throttle_Unlimited;
    AP_Notify::flags.waiting_for_throw = false;   // stop the buzzer tune
}
```
`throw_detected()` (fully dissected in §4) returns true. We tell the landing
detector we are no longer landed, cancel the "waiting for throw" notify tone, and
move to waiting for the motors to fully spool up.

### 2.4 Wait_Throttle_Unlimited → Uprighting (motors have spooled)
```cpp
} else if (stage == Throw_Wait_Throttle_Unlimited &&
           motors->get_spool_state() == AP_Motors::SpoolState::THROTTLE_UNLIMITED) {
    gcs().send_text(..., "throttle is unlimited - uprighting");
    stage = Throw_Uprighting;
}
```
Motors don't jump to full authority instantly — they ramp through spool states
(`SHUT_DOWN → GROUND_IDLE → SPOOLING_UP → THROTTLE_UNLIMITED`) governed by
`MOT_SPOOL_TIME` (default **0.5 s**). We wait until the motor library reports
`THROTTLE_UNLIMITED` — i.e. the motors are allowed to use their full range — before
attempting to right the aircraft. **`MOT_SPOOL_TIME` therefore delays the whole
recovery** and is a parameter worth knowing about.

### 2.5 Uprighting → HgtStabilise (the copter is level)
```cpp
} else if (stage == Throw_Uprighting && throw_attitude_good()) {
    gcs().send_text(..., "uprighted - controlling height");
    stage = Throw_HgtStabilise;

    pos_control->init_z_controller_no_descent();

    if (g2.throw_type == ThrowType::Drop) {
        pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() - 100);
    } else {
        pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() + 300);
    }

    copter.set_auto_armed(true);
}
```
This is the pivotal transition for your problem. When the aircraft is upright
(`throw_attitude_good()`, §5):

- **`init_z_controller_no_descent()`** — initialises the vertical position controller
  seeded from the *current* vertical state, but deliberately not allowing a
  descent demand. This resets the Z integrators to a sane value so the controller
  starts clean.
- **The target height is set relative to where the copter is right now:**
  - Upward throw (`THROW_TYPE = 0`): target = current altitude **+ 300 cm (3 m)**.
    The comment says: *"we want to rapidly clear surrounding obstacles."* So after
    an upward throw the drone is commanded to climb 3 m above the point where it
    became upright.
  - Drop (`THROW_TYPE = 1`): target = current altitude **− 100 cm (1 m)**.
- **`set_auto_armed(true)`** — prevents an automatic disarm if the next mode is an
  auto-type mode entered with the throttle stick down.

**Why this matters for bouncing:** at the instant of this transition the aircraft
still has a lot of residual vertical (and horizontal) velocity from the throw. The
Z controller is now told "go to current + 3 m" while the vehicle is still moving.
If the vehicle is moving *up* fast, it will sail past the +3 m target and the
controller pulls it back down; if it was still moving *down*, it has to arrest a
downward velocity and then climb. Combine that with the 250 cm/s / 750 cm/s²
authority caps and the standard PSC gains, and you get exactly the
overshoot-and-return **up/down bouncing** you describe. See §8.

### 2.6 HgtStabilise → PosHold (height achieved)
```cpp
} else if (stage == Throw_HgtStabilise && throw_height_good()) {
    gcs().send_text(..., "height achieved - controlling position");
    stage = Throw_PosHold;

    pos_control->init_xy_controller();
    copter.set_auto_armed(true);
}
```
Once the height error drops under 50 cm (`throw_height_good()`, §5), we initialise
the *horizontal* position controller and move to holding position. Note that height
"achieved" only requires being within 50 cm of the target **once** — the machine
advances even if the vehicle is still oscillating through that band. This is one
reason bouncing can continue into PosHold.

### 2.7 PosHold → next mode (fully stabilised)
```cpp
} else if (stage == Throw_PosHold && throw_position_good()) {
    if (!nextmode_attempted) {
        switch ((Mode::Number)g2.throw_nextmode.get()) {
            case AUTO: case GUIDED: case RTL:
            case LAND: case BRAKE: case LOITER:
                set_mode(g2.throw_nextmode, ModeReason::THROW_COMPLETE);
                break;
            default:
                break;      // any other value → stay in Throw
        }
        nextmode_attempted = true;
    }
}
```
When horizontal position error is under 50 cm (`throw_position_good()`), and only if
we have not already tried, we switch to `THROW_NEXTMODE` — but **only** if it is one
of the six whitelisted modes (Auto, Guided, RTL, Land, Brake, Loiter). Any other
value (including the default 18 = Throw) means the vehicle stays in Throw mode
holding position. `nextmode_attempted` guarantees this switch is tried exactly once,
so if the target mode refuses (e.g. Auto with no mission) it won't spam retries.

---

## 3. `run()` part 2 — the per-stage action `switch`

After possibly advancing `stage`, the same `run()` call acts on the current stage.

### 3.1 `Throw_Disarmed`
```cpp
if (g.throw_motor_start == PreThrowMotorState::RUNNING) {
    motors->set_desired_spool_state(GROUND_IDLE);
} else {
    motors->set_desired_spool_state(SHUT_DOWN);
}
attitude_control->reset_yaw_target_and_rate();
attitude_control->reset_rate_controller_I_terms();
attitude_control->set_throttle_out(0, true, g.throttle_filt);
```
- Motors either idle (`THROW_MOT_START = 1`, props turn slowly for confidence) or are
  shut down (`THROW_MOT_START = 0`, default — props dead).
- The attitude controller is **continuously reset** every loop: yaw target snapped to
  current heading, rate-loop integrators zeroed. This is important — it means while
  you are carrying the disarmed drone around, no attitude error is being accumulated.
- **`set_throttle_out(0, true, g.throttle_filt)`** — commands zero throttle. The
  second arg `true` = *apply angle boost*; third = `THR_FILT` filter constant. Motors
  are off anyway, so this mostly keeps the throttle filter state fresh.

### 3.2 `Throw_Detecting`
```cpp
// identical motor spool logic as Disarmed (idle or shutdown)
attitude_control->reset_yaw_target_and_rate();
attitude_control->reset_rate_controller_I_terms();
attitude_control->set_throttle_out(0, true, g.throttle_filt);
AP_Notify::flags.waiting_for_throw = true;   // buzzer/LED "waiting" tune
```
Same as Disarmed — throttle held at zero, attitude controller continually reset —
**plus** the "waiting for throw" notify flag that drives the buzzer/LED pattern.
Crucially, **the motors do not respond to the throw during this stage**; they stay
at idle/off. The mode is purely watching the EKF for the throw signature.

### 3.3 `Throw_Wait_Throttle_Unlimited`
```cpp
motors->set_desired_spool_state(THROTTLE_UNLIMITED);
```
The *only* thing this stage does is request full motor authority. It does **not**
command any attitude or throttle yet. The machine sits here (usually a fraction of a
second, bounded by `MOT_SPOOL_TIME`) until the motor library confirms
`THROTTLE_UNLIMITED`, then §2.4 advances it. During this brief window the aircraft is
essentially still in free-fall/ballistic flight — no active control yet.

### 3.4 `Throw_Uprighting`
```cpp
motors->set_desired_spool_state(THROTTLE_UNLIMITED);
attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);
attitude_control->set_throttle_out(0.5f, false, g.throttle_filt);
```
- Demands **level attitude**: roll = 0, pitch = 0, yaw rate = 0. The attitude
  controller now drives the aircraft flat regardless of how it was tumbling.
- **`set_throttle_out(0.5f, false, ...)`** — a fixed **50 % throttle**, with angle
  boost **off** (`false`). Angle boost is disabled deliberately so that all of the
  motor thrust goes into producing the righting torque rather than being scaled up to
  hold altitude. The comment: *"turn off angle boost to maximise righting moment."*
- This 50 % is open-loop — it is not trying to hold height yet, just to give the
  attitude loop authority to flip the aircraft upright. This is a source of a *first*
  vertical disturbance: 50 % fixed throttle may be more or less than hover, so the
  vehicle can gain or lose height during uprighting before the height controller
  takes over.

### 3.5 `Throw_HgtStabilise` — the core of your bouncing
```cpp
motors->set_desired_spool_state(THROTTLE_UNLIMITED);
attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);
pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
pos_control->update_z_controller();
```
- **Attitude:** still commanding dead-level (roll 0, pitch 0, yaw-rate 0). No
  horizontal position control yet, so the vehicle is allowed to drift sideways while
  the Z axis is brought under control.
- **`set_pos_target_z_from_climb_rate_cm(0.0f)`** — feeds a **zero climb-rate**
  demand into the Z controller. Combined with the target altitude set at the
  transition (current + 3 m for an upward throw), the controller is being told:
  "climb to the target, arriving with zero vertical speed, and hold." Internally
  this uses the square-root controller shaping bounded by the `set_max_speed_accel_z`
  limits from `init()` (250 cm/s, 750 cm/s²).
- **`update_z_controller()`** — runs the full PSC vertical cascade:
  position error → target climb rate → target accel → throttle, using `PSC_POSZ_P`,
  `PSC_VELZ_P/I/D/FF`, `PSC_ACCZ_P/I/D`. **This cascade, seeded with a large initial
  vertical velocity from the throw and a 3 m step target, is exactly what overshoots
  and produces the up/down oscillation.** The vehicle rises, overshoots +3 m, the
  controller commands descent, undershoots, climbs again — 3–6 cycles until the
  energy bleeds off, matching your report.

### 3.6 `Throw_PosHold`
```cpp
motors->set_desired_spool_state(THROTTLE_UNLIMITED);

Vector2f vel;                       // both zero-initialised
Vector2f accel;
pos_control->input_vel_accel_xy(vel, accel);   // demand zero horizontal vel & accel = "stop"
pos_control->update_xy_controller();

attitude_control->input_thrust_vector_rate_heading(pos_control->get_thrust_vector(), 0.0f);

pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
pos_control->update_z_controller();
```
- **Horizontal:** demand zero velocity and zero acceleration → the XY controller
  actively brakes any drift and holds the captured position. `update_xy_controller()`
  runs the horizontal PSC cascade (`PSC_POSXY_P`, `PSC_VELXY_*`), bounded by the XY
  speed/accel limits set in `init()`.
- **Attitude is now driven differently:** instead of forcing level, it uses
  `input_thrust_vector_rate_heading(pos_control->get_thrust_vector(), 0.0f)`. The
  position controller produces a desired *thrust vector* (the tilt needed to move/stop
  horizontally); the attitude controller leans the aircraft to match it, with zero
  yaw rate. So in PosHold the lean angle is whatever is needed to hold XY, not
  forced to zero.
- **Vertical:** still holding the target height with a zero climb-rate demand.
- Once horizontal error < 50 cm, §2.7 hands off to the next mode.

---

## 4. `throw_detected()` — how the throw itself is recognised (every line)

This function is called every loop while in `Throw_Detecting`. It returns `true`
only when a genuine throw/drop is confirmed.

```cpp
nav_filter_status filt_status = inertial_nav.get_filter_status();
if (!filt_status.flags.attitude || !filt_status.flags.horiz_pos_abs || !filt_status.flags.vert_pos) {
    return false;
}
```
**Gate 1 — EKF health.** The mode refuses to detect anything unless the EKF has a
valid attitude solution, an absolute horizontal position, and a vertical position.
No GPS/position estimate → no throw detection. (This is why Throw needs a good
position estimate before you throw it.)

```cpp
bool high_speed = inertial_nav.get_velocity_neu_cms().length_squared()
                  > (THROW_HIGH_SPEED * THROW_HIGH_SPEED);
```
**Condition A — high speed.** True if the total 3-D speed exceeds
**`THROW_HIGH_SPEED` = 500 cm/s (5 m/s)**. Uses squared magnitude to avoid a sqrt.
Your 25–30 g launch easily blows past this.

```cpp
bool changing_height;
if (g2.throw_type == ThrowType::Drop) {
    changing_height = inertial_nav.get_velocity_z_up_cms() < -THROW_VERTICAL_SPEED;
} else {
    changing_height = inertial_nav.get_velocity_z_up_cms() > THROW_VERTICAL_SPEED;
}
```
**Condition B — height is changing the right way.**
`THROW_VERTICAL_SPEED = 50 cm/s`.
- Upward throw: vehicle must be moving **up** faster than +50 cm/s.
- Drop: vehicle must be moving **down** faster than −50 cm/s.

```cpp
bool free_falling = ahrs.get_accel_ef().z > -0.25 * GRAVITY_MSS;
```
**Condition C — free fall.** Earth-frame vertical accelerometer reading is greater
than −0.25 g. In this sign convention that means the aircraft is close to free-fall
(little supporting force). `GRAVITY_MSS` = 9.80665.

```cpp
bool no_throw_action = copter.ins.get_accel().length() < 1.0f * GRAVITY_MSS;
```
**Condition D — you have let go.** The *total* measured acceleration magnitude has
dropped below 1 g, meaning your hand is no longer accelerating the vehicle — the
throw action itself is over and the copter is released and ballistic.

```cpp
float altitude_above_home;
if (ahrs.home_is_set()) {
    ahrs.get_relative_position_D_home(altitude_above_home);
    altitude_above_home = -altitude_above_home;
} else {
    altitude_above_home = inertial_nav.get_position_z_up_cm() * 0.01f;
}
const bool height_within_params =
    (g.throw_altitude_min == 0 || altitude_above_home > g.throw_altitude_min) &&
    (g.throw_altitude_max == 0 || altitude_above_home < g.throw_altitude_max);
```
**Condition E — altitude window.** Reads altitude above home (or above EKF origin if
home isn't set). Then checks it against **`THROW_ALT_MIN`** and **`THROW_ALT_MAX`**.
`0` disables each check. So if you set `THROW_ALT_MIN = 3`, the vehicle will not
"detect" the throw until it is more than 3 m up — useful to guarantee the props only
spin once the vehicle is clear of you.

```cpp
bool possible_throw_detected = (free_falling || high_speed)
                               && changing_height && no_throw_action && height_within_params;
```
**Combine into a *candidate*.** A possible throw is: (free-falling **OR** fast) **AND**
moving the right way in Z **AND** you've released it **AND** within the altitude
window. Note free-fall and high-speed are OR'd — either alone can qualify — but the
other three are all required.

```cpp
if (possible_throw_detected && ((AP_HAL::millis() - free_fall_start_ms) > 500)) {
    free_fall_start_ms = AP_HAL::millis();
    free_fall_start_velz = inertial_nav.get_velocity_z_up_cms();
}
```
**Latch the start of the candidate.** When a candidate appears (and at least 500 ms
have passed since the last latch), record the time and the current vertical velocity.
This is the reference point for the confirmation test.

```cpp
bool throw_condition_confirmed =
    ((AP_HAL::millis() - free_fall_start_ms < 500) &&
     ((inertial_nav.get_velocity_z_up_cms() - free_fall_start_velz) < -250.0f));

return throw_condition_confirmed;
```
**Confirmation.** Within 500 ms of the latch, the vertical velocity must have
*dropped by more than 2.5 m/s* (250 cm/s) relative to the latched value. In plain
terms: the code confirms a throw by seeing the characteristic **downward change in
vertical velocity of gravity acting on a released body** — a body in free-fall loses
2.5 m/s of upward velocity in about a quarter second. Only when this is seen does
`throw_detected()` return true and the machine advances.

**Consequence for your setup:** because confirmation requires a 2.5 m/s *downward
change* in Z velocity within 0.5 s, detection happens near the **apex** of your
throw, after the vehicle has already coasted up and started to slow / come back down.
With an 8–10 m throw the vehicle is high and already decelerating hard when the motors
finally spool — this is part of why there's so much residual energy for the height
controller to fight, and why bouncing is pronounced.

---

## 5. The three "goodness" checks (transition gates)

```cpp
bool ModeThrow::throw_attitude_good() const
{
    const Matrix3f &rotMat = ahrs.get_rotation_body_to_ned();
    return (rotMat.c.z > 0.866f);   // upright
}
```
**Attitude good** = the body-Z axis is within ~30° of vertical (`cos 30° ≈ 0.866`).
`rotMat.c.z` is the vertical component of the body-up axis; > 0.866 means the
aircraft is upright enough to start controlling height.

```cpp
bool ModeThrow::throw_height_good() const
{
    return (pos_control->get_pos_error_z_cm() < 50.0f);
}
```
**Height good** = vertical position error is under **50 cm**. As noted, this can be
satisfied momentarily while still oscillating, and the machine will advance anyway.

```cpp
bool ModeThrow::throw_position_good() const
{
    return (pos_control->get_pos_error_xy_cm() < 50.0f);
}
```
**Position good** = horizontal position error under **50 cm**. Gate for the final
hand-off to `THROW_NEXTMODE`.

---

## 6. The logging block (`THRO` message)

At 10 Hz (or immediately on any stage change) Throw writes a `THRO` dataflash
message. This is your single most useful debugging tool for the bouncing problem.
Fields:

| Field | Meaning |
|-------|---------|
| `TimeUS` | timestamp |
| `Stage` | current stage number (0=Disarmed … 5=PosHold) |
| `Vel` | magnitude of the 3-D velocity vector (cm/s) |
| `VelZ` | vertical velocity, up positive (cm/s) |
| `Acc` | magnitude of measured acceleration (m/s²) |
| `AccEfZ` | earth-frame vertical accel (m/s²) |
| `Throw` | true once a throw has been detected this session |
| `AttOk` | `throw_attitude_good()` |
| `HgtOk` | `throw_height_good()` — within 50 cm of target height |
| `PosOk` | `throw_position_good()` — within 50 cm of target position |

To diagnose the bounce, plot `THRO.Stage`, `THRO.VelZ`, and `CTUN.Alt` /
`CTUN.DAlt` (desired altitude) together. You will see the vehicle enter Stage 4
(HgtStabilise), the desired altitude step up 3 m, and the actual altitude overshoot
and oscillate around it — that oscillation *is* the bounce.

---

## 7. Every parameter that touches Throw mode

### 7.1 The 5 dedicated `THROW_*` parameters

| Parameter | Default | Units | What it does |
|-----------|---------|-------|--------------|
| **`THROW_MOT_START`** | 0 (Stopped) | — | `0` = motors OFF while waiting for throw; `1` = motors spin at `MOT_SPIN_MIN` (ground idle) while waiting. Purely the pre-throw motor state. |
| **`THROW_ALT_MIN`** | 0 (disabled) | m | Throw is only *detected* above this altitude above home. Use to stop props spinning until the vehicle is clear of you. |
| **`THROW_ALT_MAX`** | 0 (disabled) | m | Throw is only detected *below* this altitude. |
| **`THROW_NEXTMODE`** | 18 (Throw) | — | Mode to switch to after position is held. Only 3(Auto), 4(Guided), 5(Loiter), 6(RTL), 9(Land), 17(Brake) are honoured; anything else (incl. default 18) → stay in Throw. |
| **`THROW_TYPE`** | 0 (Upward) | — | `0` = Upward throw (detect upward motion, climb +3 m); `1` = Drop (detect downward motion, target −1 m). |

Source note: in `Parameters.cpp` the first three are `GSCALAR`s (top-level params)
and the last two (`THROW_NEXTMODE`, `THROW_TYPE`) live in the G2 group. All are gated
behind `#if MODE_THROW_ENABLED`.

### 7.2 Compile-time constants (not user parameters, but they define the behaviour)

| Constant | Value | Where | Meaning |
|----------|-------|-------|---------|
| `THROW_HIGH_SPEED` | 500 cm/s | `config.h` | 3-D speed threshold for "high speed" detection |
| `THROW_VERTICAL_SPEED` | 50 cm/s | `config.h` | vertical speed threshold for "changing height" |
| `BRAKE_MODE_SPEED_Z` | 250 cm/s | `config.h` | max vertical speed the Z controller may use in Throw |
| `BRAKE_MODE_DECEL_RATE` | 750 cm/s² | `config.h` | max horizontal/vertical decel the controllers may use |
| `−0.25 · GRAVITY_MSS` | −2.45 m/s² | inline | free-fall accel threshold |
| `1.0 · GRAVITY_MSS` | 9.81 m/s² | inline | "you let go" total-accel threshold |
| `−250 cm/s in <500 ms` | — | inline | throw confirmation velocity change |
| `0.866` | cos 30° | inline | upright threshold |
| `50 cm` | — | inline | height-good and position-good error thresholds |
| `+300 cm / −100 cm` | — | inline | target height step for Upward / Drop |
| `0.5f` | 50 % | inline | fixed throttle during uprighting |

**Important:** the four thresholds `−250 cm/s`, `0.866`, `50 cm`, and the `+300 cm`
climb target are **hard-coded**, not parameters. You cannot tune them without
recompiling. That is a key constraint on any fix — you cannot, for example, reduce
the 3 m climb step or widen the height-good band from parameters alone.

### 7.3 The controller parameters Throw *relies on* (this is where you actually tune the bounce)

Throw doesn't own its low-level gains — it calls the shared attitude and position
controllers. These parameters shape the bounce:

**Vertical position control (the bounce axis):**
- `PSC_POSZ_P` — altitude error → target climb rate. Too high → aggressive, overshoot-prone.
- `PSC_VELZ_P`, `PSC_VELZ_I`, `PSC_VELZ_D`, `PSC_VELZ_FF` — climb-rate → accel loop.
- `PSC_ACCZ_P`, `PSC_ACCZ_I`, `PSC_ACCZ_D` — accel → throttle loop. On many frames
  the accel-Z gains are mistuned and are the direct cause of Z oscillation.
- `PSC_JERK_Z` — jerk limit shaping the Z profile.

**Horizontal position control (matters in PosHold stage):**
- `PSC_POSXY_P`, `PSC_VELXY_P/I/D/FF`, `PSC_ACCXY_*`, `PSC_JERK_XY`.

**Attitude control (governs how fast/hard it levels and how much it can lean):**
- `ANGLE_MAX` — max lean angle (centidegrees, default 3000 = 30°). Caps how hard
  PosHold can lean to arrest horizontal drift.
- `ATC_ANG_RLL_P`, `ATC_ANG_PIT_P`, `ATC_ANG_YAW_P` — angle P gains.
- `ATC_RAT_RLL_*`, `ATC_RAT_PIT_*`, `ATC_RAT_YAW_*` — rate PIDs (the core tune).
- `ATC_ACCEL_R_MAX`, `ATC_ACCEL_P_MAX`, `ATC_ACCEL_Y_MAX` — angular accel limits.
- `ATC_INPUT_TC` — smoothing time constant of attitude input shaping.

**Motor spool / thrust:**
- `MOT_SPOOL_TIME` (default 0.5 s) — how long from throw-detect to full authority.
  A long spool time means more free-fall before control → more energy to fight.
- `MOT_SPIN_MIN`, `MOT_SPIN_ARM`, `MOT_THST_HOVER`, `MOT_THST_EXPO` — hover thrust
  learning and thrust curve. A badly-learned `MOT_THST_HOVER` makes the 50 % righting
  throttle and the Z controller feed-forward wrong, amplifying the initial vertical
  disturbance.
- `MOT_HOVER_LEARN` — whether hover thrust is being learned.

**Estimator (throw is very sensitive to this):**
- `INS_*` accel/gyro filtering, `EK3_*` velocity/height fusion. Detection and the
  entire Z response depend on clean EKF velocity — a noisy Z-velocity estimate during
  the violent throw feeds garbage into the controller and worsens bounce.

---

## 8. Tying it back to your bouncing problem

Reading the code, the up/down bounce is **not a bug in Throw mode logic** — it is the
predictable result of handing a high-energy ballistic vehicle to a position
controller with:

1. **A large step target.** At the `Uprighting → HgtStabilise` transition the target
   is set to *current altitude + 3 m* (hard-coded, upward throw). The vehicle receives
   a 3 m step demand while still moving in Z — a classic overshoot setup.
2. **Residual velocity at capture.** Detection completes near the throw apex, so when
   the controller finally engages there is significant Z velocity to arrest. With a
   25–30 g launch to 8–10 m, that energy is large.
3. **Authority caps** of 250 cm/s and 750 cm/s² (`BRAKE_MODE_SPEED_Z` /
   `BRAKE_MODE_DECEL_RATE`) that may be too low to arrest the throw cleanly, forcing
   the controller to over/under-shoot.
4. **PSC_* / ATC_* tune** that, if not tuned for this frame, oscillates around the
   target — the 3–6 cycles you see.
5. **Spool delay** (`MOT_SPOOL_TIME`) adding free-fall time before control begins.
6. **The height-good gate advancing at first 50 cm crossing**, so the machine moves
   on while still oscillating rather than waiting for settle.

Levers you actually have (parameter-only, no recompile):
- Tune the **vertical controller** (`PSC_POSZ_P` down, verify `PSC_ACCZ_*` and
  `PSC_VELZ_*`) so the Z axis doesn't overshoot.
- Verify **`MOT_THST_HOVER`** is correctly learned so the feed-forward is right.
- Reduce **`MOT_SPOOL_TIME`** so control engages sooner (less free-fall energy).
- Clean up the **EKF/INS** so Z-velocity estimate is trustworthy during the throw.
- Consider **`THROW_TYPE`/throw technique** — a gentler, more vertical throw leaves
  less horizontal and excess vertical energy to dissipate.
- Set **`THROW_ALT_MIN`** so detection/props only engage at a sensible height.

Levers that would require a **firmware change** (hard-coded today):
- The +3 m climb step, the 50 cm height-good band, the 2.5 m/s confirmation delta,
  and the fixed 50 % uprighting throttle.

---

## 9. One-glance sequence summary

```
ARM (disarmed→detecting)         "waiting for throw"      motors off/idle, throttle 0
   │  throw_detected() true
DETECT (→wait_throttle_unlimited) "throw detected"         request full spool
   │  spool == THROTTLE_UNLIMITED  (MOT_SPOOL_TIME)
WAIT (→uprighting)                "throttle is unlimited"  no control yet (ballistic)
   │  throw_attitude_good()  (body-z > 0.866, i.e. <30° from level)
UPRIGHT (→hgtstabilise)           "uprighted"              level demand + fixed 50% throttle
   │  set target = current alt + 3 m; init Z controller
HGTSTAB (→poshold)                "height achieved"        level + Z controller to target   ← BOUNCE
   │  throw_height_good()  (|Zerr| < 50 cm, first crossing)
POSHOLD (→nextmode)                                        XY brake+hold, thrust-vector attitude, Z hold
   │  throw_position_good()  (|XYerr| < 50 cm)
SWITCH to THROW_NEXTMODE (if whitelisted; else stay)
```
```
