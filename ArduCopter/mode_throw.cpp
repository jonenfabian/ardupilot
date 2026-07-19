#include "Copter.h"

#if MODE_THROW_ENABLED

// throw_init - initialise throw controller
bool ModeThrow::init(bool ignore_checks)
{
#if FRAME_CONFIG == HELI_FRAME
    // do not allow helis to use throw to start
    return false;
#endif

    // do not enter the mode when already armed or when flying
    if (motors->armed()) {
        return false;
    }

    // init state
    stage = Throw_Disarmed;
    nextmode_attempted = false;
    throw_reset_detection_state();

    // initialise pos controller speed and acceleration
    pos_control->set_max_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);
    pos_control->set_correction_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);

    // set vertical speed and acceleration limits
    pos_control->set_max_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);
    pos_control->set_correction_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);

    return true;
}

void ModeThrow::throw_reset_detection_state()
{
    free_fall_start_ms = 0;
    free_fall_start_velz = 0.0f;
    early_condition_start_ms = 0;
    impulse_seen = false;
    impulse_seen_ms = 0;
    detected_via_fallback = false;
    climb_start_ms = 0;
}

// initialise height stabilisation about the current height; entered either
// directly after uprighting or after the open-loop power climb stage
void ModeThrow::enter_hgt_stabilise()
{
    stage = Throw_HgtStabilise;

    // initialise the z controller
    pos_control->init_z_controller_no_descent();

    // initialise the demanded height to 3m above the throw height
    // we want to rapidly clear surrounding obstacles
    if (g2.throw_type == ThrowType::Drop) {
        pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() - 100);
    } else {
        pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() + 300);
    }

    // Set the auto_arm status to true to avoid a possible automatic disarm caused by selection of an auto mode with throttle at minimum
    copter.set_auto_armed(true);
}

// runs the throw to start controller
// should be called at 100hz or more
void ModeThrow::run()
{
    /* Throw State Machine
    Throw_Disarmed - motors are off
    Throw_Detecting -  motors are on and we are waiting for the throw
    Throw_Uprighting - the throw has been detected and the copter is being uprighted
    Throw_PowerClimb - fixed-throttle open-loop climb while the EKF recovers, then HgtStabilise
    Throw_HgtStabilise - the copter is kept level and  height is stabilised about the target height
    Throw_PosHold - the copter is kept at a constant position and height
    */

    if (!motors->armed()) {
        // state machine entry is always from a disarmed state
        stage = Throw_Disarmed;
        throw_reset_detection_state();

    } else if (stage == Throw_Disarmed && motors->armed()) {
        gcs().send_text(MAV_SEVERITY_INFO,"waiting for throw");
        stage = Throw_Detecting;
        throw_reset_detection_state();

    } else if (stage == Throw_Detecting && throw_detected()) {
        if (detected_via_fallback) {
            gcs().send_text(MAV_SEVERITY_INFO,"throw detected - spooling motors (fallback peak)");
        } else {
            gcs().send_text(MAV_SEVERITY_INFO,"throw detected - spooling motors (detect %u)",
                            (unsigned)g2.throw_detect.get());
        }
        copter.set_land_complete(false);
        stage = Throw_Wait_Throttle_Unlimited;

        // Cancel the waiting for throw tone sequence
        AP_Notify::flags.waiting_for_throw = false;

    } else if (stage == Throw_Wait_Throttle_Unlimited &&
               motors->get_spool_state() == AP_Motors::SpoolState::THROTTLE_UNLIMITED) {
        gcs().send_text(MAV_SEVERITY_INFO,"throttle is unlimited - uprighting");
        stage = Throw_Uprighting;
    } else if (stage == Throw_Uprighting && throw_attitude_good()) {
        if (g2.throw_climb_s > 0) {
            // open-loop climb while the EKF recovers from the launch
            climb_start_ms = AP_HAL::millis();
            stage = Throw_PowerClimb;
            gcs().send_text(MAV_SEVERITY_INFO,"uprighted - power climb %.1fs", (double)g2.throw_climb_s.get());
        } else {
            gcs().send_text(MAV_SEVERITY_INFO,"uprighted - controlling height");
            enter_hgt_stabilise();
        }

    } else if (stage == Throw_PowerClimb &&
               (AP_HAL::millis() - climb_start_ms) >= (uint32_t)(constrain_float(g2.throw_climb_s, 0.0f, 15.0f) * 1000.0f)) {
        gcs().send_text(MAV_SEVERITY_INFO,"power climb done - controlling height");
        enter_hgt_stabilise();

    } else if (stage == Throw_HgtStabilise && throw_height_good()) {
        gcs().send_text(MAV_SEVERITY_INFO,"height achieved - controlling position");
        stage = Throw_PosHold;

        // initialise position controller
        pos_control->init_xy_controller();

        // Set the auto_arm status to true to avoid a possible automatic disarm caused by selection of an auto mode with throttle at minimum
        copter.set_auto_armed(true);
    } else if (stage == Throw_PosHold && throw_position_good()) {
        if (!nextmode_attempted) {
            switch ((Mode::Number)g2.throw_nextmode.get()) {
                case Mode::Number::AUTO:
                case Mode::Number::GUIDED:
                case Mode::Number::RTL:
                case Mode::Number::LAND:
                case Mode::Number::BRAKE:
                case Mode::Number::LOITER:
                    set_mode((Mode::Number)g2.throw_nextmode.get(), ModeReason::THROW_COMPLETE);
                    break;
                default:
                    // do nothing
                    break;
            }
            nextmode_attempted = true;
        }
    }

    // Throw State Processing
    switch (stage) {

    case Throw_Disarmed:

        // prevent motors from rotating before the throw is detected unless enabled by the user
        if (g.throw_motor_start == PreThrowMotorState::RUNNING) {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        } else {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::SHUT_DOWN);
        }

        // demand zero throttle (motors will be stopped anyway) and continually reset the attitude controller
        attitude_control->reset_yaw_target_and_rate();
        attitude_control->reset_rate_controller_I_terms();
        attitude_control->set_throttle_out(0,true,g.throttle_filt);
        break;

    case Throw_Detecting:

        // prevent motors from rotating before the throw is detected unless enabled by the user
        if (g.throw_motor_start == PreThrowMotorState::RUNNING) {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        } else {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::SHUT_DOWN);
        }

        // Hold throttle at zero during the throw and continually reset the attitude controller
        attitude_control->reset_yaw_target_and_rate();
        attitude_control->reset_rate_controller_I_terms();
        attitude_control->set_throttle_out(0,true,g.throttle_filt);

        // Play the waiting for throw tone sequence to alert the user
        AP_Notify::flags.waiting_for_throw = true;

        break;

    case Throw_Wait_Throttle_Unlimited:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        break;

    case Throw_Uprighting:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // demand a level roll/pitch attitude with zero yaw rate
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);

        // fixed uprighting throttle (parameter, default 0.5) and turn off angle boost to maximise righting moment
        attitude_control->set_throttle_out(constrain_float(g2.throw_upright_thr, 0.1f, 1.0f), false, g.throttle_filt);

        break;

    case Throw_PowerClimb:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // demand a level roll/pitch attitude with zero yaw rate
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);

        // fixed open-loop climb throttle: no EKF height/velocity feedback while the
        // estimator recovers from the launch. Angle boost on (vehicle is already
        // within 30 deg of level) so the vertical thrust component stays constant.
        attitude_control->set_throttle_out(constrain_float(g2.throw_climb_thr, 0.1f, 1.0f), true, g.throttle_filt);

        break;

    case Throw_HgtStabilise:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // call attitude controller
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);

        // call height controller
        pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
        pos_control->update_z_controller();

        break;

    case Throw_PosHold:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // use position controller to stop
        Vector2f vel;
        Vector2f accel;
        pos_control->input_vel_accel_xy(vel, accel);
        pos_control->update_xy_controller();

        // call attitude controller
        attitude_control->input_thrust_vector_rate_heading(pos_control->get_thrust_vector(), 0.0f);

        // call height controller
        pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
        pos_control->update_z_controller();

        break;
    }

#if HAL_LOGGING_ENABLED
    // log at 10hz or if stage changes
    uint32_t now = AP_HAL::millis();
    if ((stage != prev_stage) || (now - last_log_ms) > 100) {
        prev_stage = stage;
        last_log_ms = now;
        const float velocity = inertial_nav.get_velocity_neu_cms().length();
        const float velocity_z = inertial_nav.get_velocity_z_up_cms();
        const float accel = copter.ins.get_accel().length();
        const float ef_accel_z = ahrs.get_accel_ef().z;
        const bool throw_detect = (stage > Throw_Detecting) || throw_detected();
        const bool attitude_ok = (stage > Throw_Uprighting) || throw_attitude_good();
        const bool height_ok = (stage > Throw_HgtStabilise) || throw_height_good();
        const bool pos_ok = (stage > Throw_PosHold) || throw_position_good();

// @LoggerMessage: THRO
// @Description: Throw Mode messages
// @URL: https://ardupilot.org/copter/docs/throw-mode.html
// @Field: TimeUS: Time since system startup
// @Field: Stage: Current stage of the Throw Mode
// @Field: Vel: Magnitude of the velocity vector
// @Field: VelZ: Vertical Velocity
// @Field: Acc: Magnitude of the vector of the current acceleration
// @Field: AccEfZ: Vertical earth frame accelerometer value
// @Field: Throw: True if a throw has been detected since entering this mode
// @Field: AttOk: True if the vehicle is upright 
// @Field: HgtOk: True if the vehicle is within 50cm of the demanded height
// @Field: PosOk: True if the vehicle is within 50cm of the demanded horizontal position

        AP::logger().WriteStreaming(
            "THRO",
            "TimeUS,Stage,Vel,VelZ,Acc,AccEfZ,Throw,AttOk,HgtOk,PosOk",
            "s-nnoo----",
            "F-0000----",
            "QBffffbbbb",
            AP_HAL::micros64(),
            (uint8_t)stage,
            (double)velocity,
            (double)velocity_z,
            (double)accel,
            (double)ef_accel_z,
            throw_detect,
            attitude_ok,
            height_ok,
            pos_ok);
    }
#endif  // HAL_LOGGING_ENABLED
}

bool ModeThrow::throw_ahrs_healthy() const
{
    // Check that we have a valid navigation solution
    nav_filter_status filt_status = inertial_nav.get_filter_status();
    return filt_status.flags.attitude && filt_status.flags.horiz_pos_abs && filt_status.flags.vert_pos;
}

bool ModeThrow::throw_height_within_params() const
{
    // fetch the altitude above home
    float altitude_above_home;  // Use altitude above home if it is set, otherwise relative to EKF origin
    if (ahrs.home_is_set()) {
        ahrs.get_relative_position_D_home(altitude_above_home);
        altitude_above_home = -altitude_above_home; // altitude above home is returned as negative
    } else {
        altitude_above_home = inertial_nav.get_position_z_up_cm() * 0.01f; // centimeters to meters
    }

    return (g.throw_altitude_min == 0 || altitude_above_home > g.throw_altitude_min)
           && (g.throw_altitude_max == 0 || (altitude_above_home < g.throw_altitude_max));
}

bool ModeThrow::throw_changing_height() const
{
    const float velz_min_cms = MAX(g2.throw_velz_min_ms.get(), 0.0f) * 100.0f;
    if (g2.throw_type == ThrowType::Drop) {
        return inertial_nav.get_velocity_z_up_cms() < -velz_min_cms;
    }
    return inertial_nav.get_velocity_z_up_cms() > velz_min_cms;
}

bool ModeThrow::throw_high_speed() const
{
    const float speed_min_cms = MAX(g2.throw_speed_min_ms.get(), 0.0f) * 100.0f;
    return inertial_nav.get_velocity_neu_cms().length_squared() > sq(speed_min_cms);
}

float ModeThrow::throw_accel_g() const
{
    return copter.ins.get_accel().length() / GRAVITY_MSS;
}

bool ModeThrow::throw_coast_condition(float accel_max_g) const
{
    // launch force over: specific force magnitude below threshold
    // (accel_max_g <= 0 disables the accelerometer gate, purely kinematic detection)
    if (accel_max_g > 0.0f && throw_accel_g() >= accel_max_g) {
        return false;
    }
    if (!throw_changing_height()) {
        return false;
    }
    if (!throw_high_speed()) {
        return false;
    }
    if (!throw_height_within_params()) {
        return false;
    }
    return true;
}

bool ModeThrow::throw_debounce(bool condition, uint32_t debounce_ms)
{
    const uint32_t now_ms = AP_HAL::millis();
    if (!condition) {
        early_condition_start_ms = 0;
        return false;
    }
    if (early_condition_start_ms == 0) {
        early_condition_start_ms = now_ms;
        return false;
    }
    return (now_ms - early_condition_start_ms) >= debounce_ms;
}

bool ModeThrow::throw_detected_legacy()
{
    // Check for high speed (>500 cm/s) — compile-time threshold for legacy path
    bool high_speed = inertial_nav.get_velocity_neu_cms().length_squared() > (THROW_HIGH_SPEED * THROW_HIGH_SPEED);

    // check for upwards or downwards trajectory (airdrop) of 50cm/s
    bool changing_height;
    if (g2.throw_type == ThrowType::Drop) {
        changing_height = inertial_nav.get_velocity_z_up_cms() < -THROW_VERTICAL_SPEED;
    } else {
        changing_height = inertial_nav.get_velocity_z_up_cms() > THROW_VERTICAL_SPEED;
    }

    // Check the vertical acceleraton is greater than 0.25g
    bool free_falling = ahrs.get_accel_ef().z > -0.25 * GRAVITY_MSS;

    // Check if the accel length is < 1.0g indicating that any throw action is complete and the copter has been released
    bool no_throw_action = copter.ins.get_accel().length() < 1.0f * GRAVITY_MSS;

    // High velocity or free-fall combined with increasing height indicate a possible air-drop or throw release
    bool possible_throw_detected = (free_falling || high_speed) && changing_height && no_throw_action && throw_height_within_params();

    // Record time and vertical velocity when we detect the possible throw
    if (possible_throw_detected && ((AP_HAL::millis() - free_fall_start_ms) > 500)) {
        free_fall_start_ms = AP_HAL::millis();
        free_fall_start_velz = inertial_nav.get_velocity_z_up_cms();
    }

    // Once a possible throw condition has been detected, we check for 2.5 m/s of downwards velocity change in less than 0.5 seconds to confirm
    bool throw_condition_confirmed = ((AP_HAL::millis() - free_fall_start_ms < 500) && ((inertial_nav.get_velocity_z_up_cms() - free_fall_start_velz) < -250.0f));

    return throw_condition_confirmed;
}

bool ModeThrow::throw_detected_early_coast(float accel_max_g, uint32_t debounce_ms, bool require_impulse)
{
    // latch high-g launch pulse if required
    if (require_impulse) {
        constexpr uint32_t IMPULSE_LATCH_TIMEOUT_MS = 5000;
        const uint32_t now_ms = AP_HAL::millis();
        if (throw_accel_g() >= g2.throw_impulse_g) {
            impulse_seen = true;
            impulse_seen_ms = now_ms;
        }
        // expire the latch if detection never completed (e.g. misfired launch)
        if (impulse_seen && (now_ms - impulse_seen_ms) > IMPULSE_LATCH_TIMEOUT_MS) {
            impulse_seen = false;
        }
        if (!impulse_seen) {
            early_condition_start_ms = 0;
            return false;
        }
    }

    return throw_debounce(throw_coast_condition(accel_max_g), debounce_ms);
}

bool ModeThrow::throw_detected()
{
    if (!throw_ahrs_healthy()) {
        return false;
    }

    const DetectMethod method = g2.throw_detect;
    const uint32_t debounce_ms = (uint32_t)constrain_int16(g2.throw_detect_ms, 20, 2000);

    // legacy peak detection runs every loop for every method: it is the primary
    // detection for LegacyPeak and the safety fallback for the early methods, so
    // a launch whose early conditions are never satisfied still starts the motors
    // once the classic peak signature is seen
    const bool legacy_detected = throw_detected_legacy();

    bool early_detected = false;
    switch (method) {
    case DetectMethod::LegacyPeak:
        break;

    case DetectMethod::EarlyCoast:
        early_detected = throw_detected_early_coast(MAX(g2.throw_accel_max_g.get(), 0.1f), debounce_ms, false);
        break;

    case DetectMethod::EarlyCoast1g:
        // fixed 1.0 g coast threshold (legacy "launch complete"), no peak delta-v confirm
        early_detected = throw_detected_early_coast(1.0f, debounce_ms, false);
        break;

    case DetectMethod::PostImpulse:
        early_detected = throw_detected_early_coast(MAX(g2.throw_accel_max_g.get(), 0.1f), debounce_ms, true);
        break;

    case DetectMethod::PostImpulseFast: {
        // more aggressive: half debounce, minimum 20 ms
        const uint32_t fast_ms = MAX(debounce_ms / 2U, 20U);
        early_detected = throw_detected_early_coast(MAX(g2.throw_accel_max_g.get(), 0.1f), fast_ms, true);
        break;
    }

    case DetectMethod::ClimbingFast:
        // purely kinematic: speed + climb rate + height window, no accelerometer gate
        early_detected = throw_detected_early_coast(0.0f, debounce_ms, false);
        break;
    }

    if (early_detected) {
        detected_via_fallback = false;
        return true;
    }
    if (legacy_detected) {
        // an unknown THROW_DETECT value also ends up here: no early method ran,
        // so the legacy path is the only (and safe) detection
        detected_via_fallback = (method != DetectMethod::LegacyPeak);
        return true;
    }
    return false;
}

bool ModeThrow::throw_attitude_good() const
{
    // Check that we have uprighted the copter
    const Matrix3f &rotMat = ahrs.get_rotation_body_to_ned();
    return (rotMat.c.z > 0.866f); // is_upright
}

bool ModeThrow::throw_height_good() const
{
    // Check that we are within 0.5m of the demanded height
    return (pos_control->get_pos_error_z_cm() < 50.0f);
}

bool ModeThrow::throw_position_good() const
{
    // check that our horizontal position error is within 50cm
    return (pos_control->get_pos_error_xy_cm() < 50.0f);
}

#endif
