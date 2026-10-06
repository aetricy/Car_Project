#ifndef G29_DRIVER_HOST_H
#define G29_DRIVER_HOST_H

#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.h"

// Enum representing G29 connection states
typedef enum {
    G29_STATE_DISCONNECTED,
    G29_STATE_PS3_WAKING_UP,
    G29_STATE_NATIVE_READY
} g29_state_t;

// User callback types
typedef void (*g29_state_callback_t)(g29_state_t state);
typedef void (*g29_input_callback_t)(const uint8_t *data, int len);

// ---- API FUNCTIONS ----

/**
 * @brief Initializes the G29 USB host driver in the background.
 * @param state_cb Triggered on state changes (connected, ready, disconnected)
 * @param input_cb Triggered when raw input data arrives from wheel
 */
bool g29_init(g29_state_callback_t state_cb, g29_input_callback_t input_cb);

/**
 * @brief Returns true if device is ready in native G29 mode.
 */
bool g29_is_ready(void);

/**
 * @brief Sets RPM shift LEDs on steering wheel.
 * @param val LED bitmask (0 all off, 31 all on)
 */
void g29_set_leds(uint8_t val);

/**
 * @brief Sets maximum rotation range of wheel.
 * @param range Angle in degrees (e.g. 900 full lock, 360 for F1, 540 for RC drift)
 */
void g29_set_range(uint16_t range);

/**
 * @brief Applies constant force torque to wheel.
 * @param force Between -1.0 (full left) and 1.0 (full right)
 */
void g29_set_constant_force(float force);

/**
 * @brief Applies mechanical friction (steering weight) to wheel (non-blocking).
 * @param friction From 0.0 (no friction) to 1.0 (maximum resistance)
 */
void g29_set_friction(float friction);

/**
 * @brief Sets raw mechanical friction step (non-blocking).
 * @param f_val Friction level (0 to 7)
 */
void g29_set_friction_raw(uint8_t f_val);

/**
 * @brief Enables G29 hardware autocentering mode (0x14 command).
 */
void g29_enable_autocenter(void);

/**
 * @brief Disables G29 hardware autocenter spring (0xF5 command).
 */
void g29_disable_autocenter(void);

/**
 * @brief Stops all active motor torques and effects immediately (emergency stop).
 */
void g29_force_off(void);

/**
 * @brief Sets raw hardware autocenter spring strength and rate (non-blocking).
 * @param s_val Strength step (0 - 15)
 * @param r_val Rate/slope step (0 - 255)
 */
void g29_set_autocenter_raw(uint8_t s_val, uint8_t r_val);

/**
 * @brief Sets autocentering spring strength and rate.
 * @param strength Centering force (0.0 to 1.0)
 * @param rate Centering speed/rate (0.0 to 1.0)
 */
void g29_set_autocenter(float strength, float rate);

/**
 * @brief Template function for sending raw Force Feedback commands
 */
void g29_send_ffb_command(const uint8_t *command, size_t len);

#endif // G29_DRIVER_HOST_H