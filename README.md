# rp2350_uni Library 🚀

A high-performance C++ header/CPP library for the **Raspberry Pi RP2040** and **RP2350**, optimized for the Arduino IDE and the Earle Philhower Core Framework.

This library bundles essential hardware features such as **PIO Encoder Processing with Dynamic Acceleration**, **WS2812 Status LEDs**, **Watchdog Handling**, **ADC Measurements**, and **Button Debouncing** into a clean Singleton class.

---

## 🛠️ Features

* **WS2812 PIO Driver:** Direct bitbanging via PIO State Machine including global brightness control and an optional auto-off timer (`duration_ms`).
* **Hardware PIO Quadrature Encoder:** High-precision sampling via PIO-FIFO with built-in dynamic rotation acceleration.
* **Button Debouncer:** Event-based evaluation for short and long button presses (debounced).
* **ADC & On-Core Temp:** Easy reading of analog pins and the MCU's internal core temperature.
* **Watchdog Support:** Convenient setup and kicking directly inside the loop.
* **Unified Logging System:** Prefix-based formatted logs (`LOG_INFO`, `LOG_WARNF`, etc.).

---

## 📁 Folder Structure

To ensure the library links properly, place the files in your project directory as follows:

```text
YourProject/
├── YourProject.ino
└── src/
    ├── rp2350_uni.h
    └── rp2350_uni.cpp
