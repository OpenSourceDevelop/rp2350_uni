# rp2350_uni

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![Target: RP2350 / RP2040](https://img.shields.io/badge/Target-RP2350%2FRP2040-C60537.svg)](https://www.raspberrypi.com/)

A universal, highly optimized C++ embedded helper library for Raspberry Pi **RP2350** (and RP2040) microcontrollers within the Arduino ecosystem.

Built for maximum performance and reliability using **hardware PIO encapsulation**, **fully non-blocking finite state machines (FSM)**, and a **type-safe API**.

---

## 🛠 Features

* **Hardware PIO Quadrature Encoder:**
  * Leverages RP2350 PIO State Machines to decode incremental rotary encoders.
  * Zero CPU overhead for interrupt handling at high rotational speeds.
  * Dynamic velocity acceleration curve.
  * Integrated non-blocking button debouncing.
  * Button event classification (`SHORT_PRESS`, `LONG_PRESS`).
* **System & MCU Health:**
  * Direct acquisition of the internal RP2350 die temperature in °C via internal ADC.
  * Query system watchdog reboot causes.
  * Watchdog activation and re-feeding helper utilities.
* **Modular & Clean C++ Architecture:**
  * Strict type safety using `constexpr` and `enum class`.
  * Designed for the [Earle Philhower RP2040/RP2350 Core](https://github.com/earlephilhower/arduino-pico).

---

## 📁 Repository Structure

```text
rp2350_uni/
├── library.properties      # Arduino Library Specification
├── README.md               # Repository Documentation
├── src/
│   ├── rp2350_uni.h        # Doxygen-documented header file
│   └── rp2350_uni.cpp      # Driver & FSM implementation
└── examples/
    ├── BasicEncoder/       # Example: Hardware PIO Encoder & Temp Sensor
    └── StatusLED/          # Example: Non-blocking WS2812 FSM State Machine