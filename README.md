# rp_help

A lightweight, non-blocking C++ helper library for RP2040 and RP2350 microcontrollers using the Arduino-Pico core. It provides robust peripheral abstractions including PIO-driven WS2812 status LEDs, PIO quadrature encoders, advanced button handling with configurable debouncing, hardware watchdog supervision, and clean logging utilities.

## Features

- **PIO WS2812 Driver**: Autonomous hardware-timed control for addressable LEDs with adjustable global brightness and non-blocking background flashing (`setFlash`).
- **PIO Quadrature Encoder**: Precise, low-overhead position tracking via Raspberry Pi PIO state machines with configurable scaling and hardware pullup management.
- **Button Debouncing**: Reliable short and long press event detection with active-low configuration.
- **Watchdog Supervision**: Simple API to enable and kick the hardware watchdog timer.
- **Convenience Utilities**: HSV-to-GRB color conversion and formatted logging macros (`LOG_INFO`, `LOG_INFOF`, etc.).

## Installation

1. Clone or download this repository into your Arduino sketchbook libraries folder (e.g., `Documents/Arduino/libraries/rp_help`).
2. Ensure you are using the official [Arduino-Pico core](https://github.com/earlephilhower/arduino-pico) for your RP2040/RP2350 board.

## Usage

Here is a quick example demonstrating system initialization, state machine status LEDs, and encoder button handling:

```cpp
#include <Arduino.h>
#include <rp_help.h>

constexpr uint8_t STATUS_LED_PIN = 16;  // Onboard WS2812 GPIO
constexpr uint8_t PIN_ENC_A      = 10;  // Encoder Phase A GPIO (Phase B is Pin A + 1)
constexpr uint8_t PIN_ENC_BTN    = 12;  // Button GPIO (active LOW)

auto& sys = rp_help::System::instance();

void setup() {
    sys.initSerial(115200);

    // Initialize peripherals
    sys.initPixel(STATUS_LED_PIN);
    sys.setBrightnessPixel(30);
    sys.enableWatchdog(2000);
    sys.initEncoder(PIN_ENC_A, 4, PIN_ENC_BTN, 50, 800);

    // Startup indicator
    sys.setPixel(rp_help::Color::WHITE, 500);

    LOG_INFO("System initialized successfully.");
}

void loop() {
    // Must be called regularly to service watchdog, background flashing, and buttons
    sys.update();

    // Check for button events
    rp_help::ButtonEvent btn = sys.getButtonEvent();
    if (btn == rp_help::ButtonEvent::SHORT_PRESS) {
        LOG_INFO("Button short press detected.");
        // Trigger a non-blocking background flash (e.g., Error state: Red/Blue)
        sys.setFlash(rp_help::Color::RED, rp_help::Color::BLUE, 60);
    }
}