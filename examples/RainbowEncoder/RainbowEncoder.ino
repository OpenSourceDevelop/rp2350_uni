/**
 * @file    RainbowEncoder.ino
 * @brief   Interactive Rainbow Hue Rotator using PIO Encoder & Status LED
 * @author  OpenSourceDevelop
 */

#include <Arduino.h>
#include <rp2350_uni.h>
#include <Adafruit_NeoPixel.h>
#include <cmath>

// ── HARDWARE DEFINITIONS ──
constexpr uint8_t  PIN_LED        = 16;  // Onboard / WS2812 GPIO
constexpr uint16_t NUM_LEDS       = 1;
constexpr uint8_t  PIN_ENC_A      = 10;  // Phase B = GPIO 11
constexpr uint8_t  PIN_ENC_BTN    = 12;

// ── COLOR & HSV CONFIGURATION ──
constexpr uint8_t  LED_BRIGHTNESS = 40;  // Max brightness (0 - 255)
constexpr int16_t  HUE_STEP_DEG   = 5;   // Hue change per encoder tick

// ── OBJECTS ──
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);
RP2350Uni::HardwarePIOEncoder encoder(PIN_ENC_A, PIN_ENC_BTN);

static int16_t g_current_hue = 0; // Current hue in degrees (0 - 359)

/**
 * @brief Converts HSV parameters to 32-bit GRB NeoPixel color format.
 * @param h Hue angle in degrees [0, 360)
 * @param s Saturation [0.0, 1.0]
 * @param v Value / Brightness [0.0, 1.0]
 * @return Packed 32-bit uint32_t color in GRB format.
 */
uint32_t hsvToGRB(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (h < 60.0f)        { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    uint8_t r_byte = static_cast<uint8_t>((r + m) * 255.0f);
    uint8_t g_byte = static_cast<uint8_t>((g + m) * 255.0f);
    uint8_t b_byte = static_cast<uint8_t>((b + m) * 255.0f);

    return (static_cast<uint32_t>(g_byte) << 16) | 
           (static_cast<uint32_t>(r_byte) << 8)  | 
            static_cast<uint32_t>(b_byte);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    // Initialize RP2350 peripherals & Watchdog
    RP2350Uni::System::begin();
    RP2350Uni::System::enableWatchdog(4000);

    // Start hardware PIO quadrature encoder
    encoder.begin();

    // Initialize NeoPixel Strip
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    
    // Set initial color (Solid White indicator on boot)
    strip.setPixelColor(0, strip.Color(255, 255, 255));
    strip.show();

    Serial.println(F("[RP2350_UNI] Rainbow Encoder Example Ready!"));
}

void loop() {
    // Feed watchdog in main execution loop
    RP2350Uni::System::feedWatchdog();

    // 1. Process PIO Encoder Delta
    int32_t delta = encoder.readDelta();
    if (delta != 0) {
        g_current_hue += static_cast<int16_t>(delta * HUE_STEP_DEG);

        // Keep Hue wrapped within [0, 360) range
        if (g_current_hue >= 360) g_current_hue %= 360;
        while (g_current_hue < 0) g_current_hue += 360;

        uint32_t colorGRB = hsvToGRB(static_cast<float>(g_current_hue), 1.0f, 1.0f);
        strip.setPixelColor(0, colorGRB);
        strip.show();

        Serial.printf("[ENCODER] Hue: %d°\n", g_current_hue);
    }

    // 2. Process Button Events
    RP2350Uni::HardwarePIOEncoder::ButtonEvent btn = encoder.updateButton();

    if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::SHORT_PRESS) {
        g_current_hue = 0; // Reset back to Red (0°)
        strip.setPixelColor(0, strip.Color(255, 255, 255)); // Flash White
        strip.show();

        Serial.println(F("[BUTTON] Short Press -> Reset Hue (Flash WHITE)"));
    } 
    else if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::LONG_PRESS) {
        float mcuTemp = RP2350Uni::System::readMcuTemperature();
        strip.setPixelColor(0, strip.Color(0, 0, 255)); // Flash Blue
        strip.show();

        Serial.printf("[BUTTON] Long Press -> Core Temp: %.2f °C (Flash BLUE)\n", mcuTemp);
    }
}