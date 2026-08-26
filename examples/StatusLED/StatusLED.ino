/**
 * @file    StatusLED.ino
 * @brief   Non-blocking FSM Status LED example for RP2350
 * @author  OpenSourceDevelop
 */

#include <Arduino.h>
#include <rp2350_uni.h>
#include <Adafruit_NeoPixel.h>

// ── HARDWARE DEFINITIONEN ──
constexpr uint8_t  PIN_LED        = 16;  // Standard RP2350 NeoPixel GPIO (oder je nach Board anpassen)
constexpr uint16_t NUM_LEDS       = 1;
constexpr uint8_t  PIN_ENC_A      = 10;
constexpr uint8_t  PIN_ENC_BTN    = 12;

// ── ENUMS FÜR SYSTEMZUSTAND ──
enum class SystemStatus : uint8_t {
    IDLE = 0,
    HEATING,
    READY,
    ERROR
};

// ── OBJEKTE ──
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);
RP2350Uni::HardwarePIOEncoder encoder(PIN_ENC_A, PIN_ENC_BTN);

SystemStatus g_status = SystemStatus::IDLE;

// ─────────────────────────────────────────────
//  Non-Blocking LED FSM Engine
// ─────────────────────────────────────────────
void updateStatusLED(SystemStatus status) {
    static uint32_t lastBlinkTime = 0;
    static bool     blinkState    = false;
    uint32_t now = millis();

    switch (status) {
        case SystemStatus::IDLE:
            // Statisch Blau (Dimmed)
            strip.setPixelColor(0, strip.Color(0, 50, 150));
            break;

        case SystemStatus::HEATING:
            // Pulsieren / Blinken Gelb/Orange (500ms Intervall)
            if (now - lastBlinkTime >= 500) {
                lastBlinkTime = now;
                blinkState = !blinkState;
            }
            if (blinkState) {
                strip.setPixelColor(0, strip.Color(255, 140, 0));
            } else {
                strip.setPixelColor(0, strip.Color(40, 20, 0));
            }
            break;

        case SystemStatus::READY:
            // Statisch Grün
            strip.setPixelColor(0, strip.Color(0, 200, 40));
            break;

        case SystemStatus::ERROR:
            // Schnelles Rot-Blinken (150ms Intervall)
            if (now - lastBlinkTime >= 150) {
                lastBlinkTime = now;
                blinkState = !blinkState;
            }
            strip.setPixelColor(0, blinkState ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0));
            break;
    }

    strip.show();
}

// ─────────────────────────────────────────────
//  Setup & Loop
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    RP2350Uni::System::begin();
    RP2350Uni::System::enableWatchdog(2000);

    encoder.begin();

    strip.begin();
    strip.setBrightness(50); // 0 - 255
    strip.show();

    Serial.println(F("[RP2350_UNI] Status LED FSM Example Started."));
    Serial.println(F("Press Button to cycle Status: IDLE -> HEATING -> READY -> ERROR"));
}

void loop() {
    RP2350Uni::System::feedWatchdog();

    // Button steuert den Systemzustand um
    RP2350Uni::HardwarePIOEncoder::ButtonEvent btn = encoder.updateButton();
    if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::SHORT_PRESS) {
        uint8_t nextState = (static_cast<uint8_t>(g_status) + 1) % 4;
        g_status = static_cast<SystemStatus>(nextState);
        
        Serial.printf("New State: %d\n", static_cast<uint8_t>(g_status));
    }

    // LED State Machine im Haupt-Loop (Non-Blocking)
    updateStatusLED(g_status);
}