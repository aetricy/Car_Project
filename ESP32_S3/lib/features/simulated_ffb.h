#ifndef SIMULATED_FFB_H
#define SIMULATED_FFB_H

#include <stdbool.h>
#include <stdint.h>
#include "CONFIG.h"
#include "g29_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the simulated Force Feedback (FFB) subsystem.
 *        Applies parked mode profile (subtle friction and gentle autocentering) to the steering wheel.
 */
void simulated_ffb_init(void);

/**
 * @brief Updates dynamic vehicle physics simulation from steering and pedal telemetry.
 *        Computes estimated speed, cornering caster self-aligning torque, understeer scrub, and braking load transfer,
 *        non-blockingly transmitting parameters to G29.
 *        Must be invoked periodically inside Logic_Task loop.
 * @param telemetry Current telemetry packet from G29
 */
void simulated_ffb_update(const g29_telemetry_t *telemetry);

/**
 * @brief Enables or disables the simulated FFB subsystem.
 *        Pass false during sleep mode or emergency stops to cut motor current.
 * @param enabled true: active simulation, false: motors free
 */
void simulated_ffb_set_enabled(bool enabled);

/**
 * @brief Returns current state of FFB simulation.
 */
bool simulated_ffb_is_enabled(void);

/**
 * @brief Toggles FFB simulation on/off.
 * @return New state (true: enabled, false: disabled/free)
 */
bool simulated_ffb_toggle(void);

/**
 * @brief Resets estimated speed and simulation states.
 *        Reverts to parked state upon vehicle switch, wake-up, or failsafe.
 */
void simulated_ffb_reset(void);

/**
 * @brief Returns current estimated vehicle speed (0.0f = stopped, 1.0f = full speed).
 */
float simulated_ffb_get_estimated_speed(void);

#ifdef __cplusplus
}
#endif

#endif // SIMULATED_FFB_H

