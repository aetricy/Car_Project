---
tags: [esp32, freertos, esp-now, rc-drift, logitech-g29, kicad, embedded]
aliases: [Sim-to-Reality RC Roadmap, ESP32 Drift Vehicle System]
date_created: 2026-08-26
---

# 🏎️ ESP32 Sim-to-Reality RC Drift Telemetry & Control System

This engineering roadmap outlines the transformation of the zero-latency, FreeRTOS-based ESP-NOW communication pipeline between the ESP32-S3 (Transmitter / G29 Host) and the ESP32-C3 (Receiver / Car) into physical hardware and autonomous control systems. Chassis target: 1/24 & 1/28 RWD (e.g. TG Super TT).

---

## 📍 PHASE 1: Physical Layer & Actuator Driving
The communication layer is complete. In this phase, wireless digital telemetry is converted into physical actuation (PWM).

> [!warning] Hardware Notice: 3.3V / 5V Logic Tolerance
> ESP32-C3 GPIO pins are **not** 5V tolerant. To prevent signal noise and inductive 5V back-EMF spikes from ESCs or high-torque servos, always insert a bidirectional Logic Level Converter or protective series resistor network.

- [x] **Hardware PWM Generation (LEDC / MCPWM)**
  - [x] Configured 330Hz / 50Hz period using ESP-IDF `ledc` peripheral.
  - [x] Calibrated duty cycle range to RC standard (1000µs - 2000µs, 1500µs neutral center).
- [x] **Mathematical Mapping & Signal Processing**
  - [x] G29 raw telemetry converted to calibrated microsecond PWM timings.
  - [x] Exponential curve algorithms ("Linear", "Expo Soft", "Expo Aggressive") implemented for steering and throttle.
  - [x] Dynamic deadzone compensation implemented to eliminate center jitter.

---

## 📍 PHASE 2: Bidirectional Time-Division Telemetry (TDM)
The system should not only receive commands, but also transmit physical vehicle telemetry back to the S3 / G29 wheel.

> [!danger] RF Packet Collision Prevention
> If the S3 transmits 50 packets per second while the C3 simultaneously broadcasts asynchronously, Wi-Fi collisions (`ESP_ERR_ESPNOW_NO_MEM`) degrade throughput. Uncoordinated transmissions must be avoided.

- [ ] **TDM (Time-Division Multiplexing) Architecture**
  - [ ] Synchronize C3 telemetry transmission exclusively in response to incoming S3 packets (via RX callback event flag) as an ACK reply.
- [ ] **On-Board Sensor Telemetry (C3 Side)**
  - [ ] ADC voltage sensing for 2S LiPo battery monitoring (using precision voltage divider network).
  - [ ] Gyro slip angle and yaw acceleration packaged into telemetry stream.
- [ ] **Driver Feedback (S3 Side)**
  - [ ] Alert driver via G29 RPM LEDs or buzzer when battery voltage falls below threshold.

---

## 📍 PHASE 3: G29 Force Feedback (FFB) Integration
The pinnacle of sim-to-reality immersion: translating physical chassis forces and yaw acceleration directly into G29 dual-motor force feedback.

- [x] **USB HID Output Report Management**
  - [x] Native G29 HID command packets for range, autocenter spring, mechanical friction, and constant force.
- [x] **Simulated Physics Engine (S3 Side)**
  - [x] Real-time vehicle speed estimation, caster self-aligning torque, high-speed understeer scrub, and throttle-sensitive friction softening.
- [ ] **Sensor-Driven Closed-Loop FFB**
  - [ ] Closed-loop counter-steer torque driven by on-car IMU yaw velocity with <15ms end-to-end response latency.

