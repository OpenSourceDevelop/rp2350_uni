/**
 * @file    RainbowEncoder.ino
 * @brief   Non-blocking HSV Rainbow Hue Selector using RP2350Uni PIO Encoder & APA-104 LED.
 *          Configured for hardware setups with external pull-up resistors.
 *          Includes Watchdog integration during fast encoder updates.
 * @author  OpenSourceDevelop
 */

#include <Arduino.h>
#include <rp2350_uni.h>

// ── HARDWARE DEFINITIONS ──
constexpr uint8_t STATUS_LED_PIN = 16;  // Onboard APA-104 / WS2812 GPIO
constexpr uint8_t PIN_ENC_A      = 10;  // Phase B = GPIO 11 (uses HW Pull-ups)
constexpr uint8_t PIN_ENC_BTN    = 12;  // Button GPIO (uses HW Pull-up to 3.3V, active LOW)

RP2350Uni::HardwarePIOEncoder encoder(PIN_ENC_A, PIN_ENC_BTN);

// Hue-Wert für den Farbkreis (0 - 359 Grad)
static uint16_t g_hue = 0;

/**
 * @brief Converts HSV (Hue, Saturation, Value) to packed 32-bit GRB Color Format.
 * @param h Hue angle [0 - 359]
 * @param s Saturation [0.0 - 1.0]
 * @param v Value/Brightness [0.0 - 1.0]
 * @return Packed 32-bit GRB color for APA-104 / WS2812
 */
uint32_t hsvToGrb(uint16_t h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0, g = 0, b = 0;

    if (h < 60)        { r = c; g = x; b = 0; }
    else if (h < 120)  { r = x; g = c; b = 0; }
    else if (h < 180)  { r = 0; g = c; b = x; }
    else if (h < 240)  { r = 0; g = x; b = c; }
    else if (h < 300)  { r = x; g = 0; b = c; }
    else               { r = c; g = 0; b = x; }

    uint8_t r8 = static_cast<uint8_t>((r + m) * 255.0f);
    uint8_t g8 = static_cast<uint8_t>((g + m) * 255.0f);
    uint8_t b8 = static_cast<uint8_t>((b + m) * 255.0f);

    // Pack into GRB Byte-Order (Green, Red, Blue)
    return (static_cast<uint32_t>(g8) << 16) | 
           (static_cast<uint32_t>(r8) << 8)  | 
            static_cast<uint32_t>(b8);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    // Watchdog Reboot-Check
    if (RP2350Uni::System::wasWatchdogReset()) {
        Serial.println(F("[WARNING] System rebooted by Hardware Watchdog!"));
    }

    // System & PIO LED Driver Initialisierung
    RP2350Uni::System::begin(STATUS_LED_PIN);
    RP2350Uni::System::setBrightness(40); // 0-255 Brightness Scale

    // Watchdog mit 2000ms Timeout aktivieren
    RP2350Uni::System::enableWatchdog(2000);

    // Initialisierung des Encoders (deaktiviert interne Pull-ups für externe HW-Pull-ups)
    encoder.begin();

    // Startfarbe setzen (Rot bei Hue = 0°)
    RP2350Uni::System::setPixel(hsvToGrb(g_hue, 1.0f, 1.0f));

    Serial.println(F("[RP2350_UNI] Rainbow Encoder Example Started."));
    Serial.println(F("Rotate Encoder to adjust Hue (0-359°). Press Button to reset to Red."));
}

void loop() {
    // Non-blocking FSM & Watchdog Feed
    RP2350Uni::System::update();

    // 1. Encoder-Drehung auslesen & Hue anpassen
    int32_t delta = encoder.readDelta();
    if (delta != 0) {
        // Watchdog explizit auch bei schnellen Encoder-Events nachfüttern
        RP2350Uni::System::feedWatchdog();

        // Hue im Bereich 0-359 Grad halten (5 Grad pro Raste)
        int32_t newHue = static_cast<int32_t>(g_hue) + (delta * 5);
        
        if (newHue < 0) {
            newHue = 360 + (newHue % 360);
        }
        g_hue = static_cast<uint16_t>(newHue % 360);

        Serial.printf("Hue: %d° | Delta: %d\n", g_hue, delta);
        
        // APA-104 LED-Farbe direkt live aktualisieren
        RP2350Uni::System::setPixel(hsvToGrb(g_hue, 1.0f, 1.0f));
    }

    // 2. Taster-Event auswerten
    RP2350Uni::HardwarePIOEncoder::ButtonEvent btn = encoder.updateButton();
    if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::SHORT_PRESS) {
        g_hue = 0; // Reset auf Rot
        Serial.println(F("Button Pressed -> Reset Hue to 0° (Red)"));
        RP2350Uni::System::setPixel(hsvToGrb(g_hue, 1.0f, 1.0f));
    }
    else if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::LONG_PRESS) {
        // Flash White zum Bestätigen des Long-Press & MCU-Temp anzeigen (Blinkt für 300ms)
        RP2350Uni::System::setPixel(RP2350Uni::Color::WHITE, 300);
        Serial.printf("Long Press -> MCU Temp: %.2f °C\n", RP2350Uni::System::readMcuTemperature());
    }
}