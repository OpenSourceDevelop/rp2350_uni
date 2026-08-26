/**
 * @file    StatusLED.ino
 * @brief   Non-blocking Status LED example for APA-104 using RP2350Uni static System interface
 *          Configured for boards with external hardware pull-up resistors.
 * @author  OpenSourceDevelop
 */

#include <Arduino.h>
#include <rp2350_uni.h>

// ── HARDWARE DEFINITIONS ──
constexpr uint8_t STATUS_LED_PIN = 16;  // Onboard APA-104 / WS2812 GPIO
constexpr uint8_t PIN_ENC_A      = 10;  // Phase B = GPIO 11 (uses HW Pull-ups)
constexpr uint8_t PIN_ENC_BTN    = 12;  // Button GPIO (uses HW Pull-up to 3.3V, active LOW)

// ── SYSTEM STATE ENUM ──
enum class SystemStatus : uint8_t {
    IDLE = 0,
    HEATING,
    READY,
    ERROR
};

RP2350Uni::HardwarePIOEncoder encoder(PIN_ENC_A, PIN_ENC_BTN);
SystemStatus g_status = SystemStatus::IDLE;

/**
 * @brief Updates status LED color based on system state
 */
void updateStatusLED(SystemStatus status) {
    switch (status) {
        case SystemStatus::IDLE:
            // Static Dim Blue
            RP2350Uni::System::setPixel(RP2350Uni::Color::BLUE);
            break;

        case SystemStatus::HEATING:
            // Pulsing / Blinking Amber (500ms pulse interval)
            RP2350Uni::System::setPixel(RP2350Uni::Color::AMBER, 500);
            break;

        case SystemStatus::READY:
            // Static Solid Green
            RP2350Uni::System::setPixel(RP2350Uni::Color::GREEN);
            break;

        case SystemStatus::ERROR:
            // Fast Flashing Red (150ms interval)
            RP2350Uni::System::setPixel(RP2350Uni::Color::RED, 150);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    // Initialize System with internal APA-104 LED pin
    RP2350Uni::System::begin(STATUS_LED_PIN);
    RP2350Uni::System::setBrightness(30); // Brightness scale 0-255
    RP2350Uni::System::enableWatchdog(2000);

    // Initialize Hardware PIO Encoder & Button (Floating INPUT for HW Pull-ups)
    encoder.begin();

    // Initial LED State (Temporary White flash for 500ms on startup)
    RP2350Uni::System::setPixel(RP2350Uni::Color::WHITE, 500);

    Serial.println(F("[RP2350_UNI] Native APA-104 Status LED Example Started."));
    Serial.println(F("Press Button to cycle Status: IDLE -> HEATING -> READY -> ERROR"));
}

void loop() {
    // Non-blocking internal system update (LED animation FSM & Watchdog feed)
    RP2350Uni::System::update();

    // Debouncing and event handler for hardware button
    RP2350Uni::HardwarePIOEncoder::ButtonEvent btn = encoder.updateButton();
    
    if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::SHORT_PRESS) {
        uint8_t nextState = (static_cast<uint8_t>(g_status) + 1) % 4;
        g_status = static_cast<SystemStatus>(nextState);
        
        Serial.printf("New State: %d\n", static_cast<uint8_t>(g_status));
        updateStatusLED(g_status);
    } 
    else if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::LONG_PRESS) {
        Serial.printf("Core Temp: %.2f °C\n", RP2350Uni::System::readMcuTemperature());
    }
}
