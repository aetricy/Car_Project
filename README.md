# Car Project: G29 ESP-NOW RC Controller 🚗🎮

This project is a system that allows you to control a remote-controlled (RC) car using a Logitech G29 steering wheel set via the **ESP-NOW** wireless communication protocol.

This branch (`new/config-control`) focuses on vehicle control configurations, G29 raw data parsing, and the optimization of ESP-NOW data packets.

## 🏗️ System Architecture

The system generally consists of two main components: the **Transmitter** and the **Receiver (Car)**. A detailed diagram of the system can be examined in the image named `Untitled-2026-09-24-1211.jpg` included in the project files.

### 1. Transmitter (Controller Side)

Raw data coming from the Logitech G29 (steering angle, pedals, and buttons) is read by a microcontroller.

* **G29 Raw Button Map:** The 8-byte data packet (from `[0]` to `[7]`) coming from the G29 is parsed (e.g., Throttle, Brake, Clutch, Shifter buttons).
* The data is gathered into the `G29_rawPkg_t` structure and then converted into the `control_packet_t` structure, which will be sent to the car.
* **Communication:** The data is transmitted to the receiver via the low-latency **ESP-NOW** protocol, which operates independently of a standard Wi-Fi network.

### 2. Receiver (Car Side)

An **ESP32-C3** microcontroller is mounted on the car. It drives the mechanical components by converting the incoming control packets (Throttle, Steering, etc.) into PWM signals.

* **Power:** The system draws its power from a 2S LiPo battery (Battery 2S) distributed through the necessary voltage regulators (5V/GND).
* **ESC & Motor:** The incoming throttle data is forwarded to the ESC to drive the DC Motor.
* **Gyro & Servo:** The steering data is routed through a Gyro module to the Servo motor to maintain the vehicle's stability and balance.

## 🔌 Hardware and Pin Connections (ESP32-C3 Receiver)

The channel and pin configurations for the ESP32-C3 on the receiver side are mapped as follows:

| Channel | Hardware | Function | 
| ----- | ----- | ----- | 
| **PWM - CH1** | ESC -> DC Motor | Throttle (Forward/Reverse) | 
| **PWM - CH2** | GYRO -> Servo Motor | Steering | 
| **PWM - CH3** | GYRO | Gyro Gain (Sensitivity) | 

*Note: Ensure that a common GND line is established across the system and that the 5V supply is routed correctly.*

## 📦 Data Structures (Packets)

The packets sent over ESP-NOW are based on specific `struct` definitions:

* **`G29_rawPkg_t`**: Contains the raw, unparsed steering, pedal, and button states read directly from the G29.
* **`control_packet_t`**: The optimized control packet sent to the receiver. It contains the packet type, command ID, and required parameters (throttle values, steering angles, etc.).

## 🚀 Setup and Usage

1. **Prerequisites:**
   * PlatformIO or Arduino IDE.
   * ESP32 Board Manager packages (with ESP32-C3 support).
   * Relevant ESP-NOW and USB Host (for G29) libraries.

2. **Installation:**
   * Upload the `Transmitter` code to the ESP32 on the controller side.
   * Upload the `Receiver` code to the ESP32-C3 on the car side.

3. **Pairing (MAC Address):**
   * Make sure to correctly enter the MAC address of the receiver ESP32-C3 into the transmitter code (either as a specific peer or broadcast). ESP-NOW pairing relies on this MAC address to establish the connection.