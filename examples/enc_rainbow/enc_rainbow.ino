#include "src/rp2350_uni.h"

#define WS2812_PIN       16
#define ENCODER_PIN_A    10 // Pin B ist GPIO 11
#define ENCODER_BTN_PIN  12

auto& sys = UNI::System::instance();
int16_t current_hue = 0; 

// Wandelt HSV in 32-Bit GRB um
uint32_t hsvToGRB(uint16_t h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0, g = 0, b = 0;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    uint8_t r_byte = static_cast<uint8_t>((r + m) * 255.0f);
    uint8_t g_byte = static_cast<uint8_t>((g + m) * 255.0f);
    uint8_t b_byte = static_cast<uint8_t>((b + m) * 255.0f);

    return ((uint32_t)g_byte << 16) | ((uint32_t)r_byte << 8) | b_byte;
}

void setup() {
    sys.begin(115200, WS2812_PIN);
    while (!Serial && millis() < 2000);

    sys.setBrightness(30);
    sys.initADC(26, true);
    sys.enableWatchdog(4000);
    sys.initEncoder(ENCODER_PIN_A, 4); 
    sys.initButton(ENCODER_BTN_PIN, true, 800);

    // Dauerhaft Weiß
    sys.setPixel(UNI::Color::WHITE, 500); // 500ms

    LOG_INFO(F("[SYSTEM] Bereit!"));
}

void loop() {
    sys.update();

    // 1. Encoder Auswertung
    int32_t delta = sys.readEncoderDelta();
    if (delta != 0) {
        current_hue += (delta * 5); 

        if (current_hue >= 360) current_hue %= 360;
        while (current_hue < 0) current_hue += 360;

        uint32_t new_color = hsvToGRB(current_hue, 1.0f, 1.0f);
        sys.setPixel(new_color, 0); // dauerhaft

        LOG_INFOF("Farbton: %d°", current_hue);
    }

    // 2. Button Auswertung
    UNI::ButtonEvent btn = sys.getButtonEvent();

    if (btn == UNI::ButtonEvent::SHORT_PRESS) {
        LOG_INFO(F("Reset -> Dauerhaft WEISS"));
        sys.setPixel(UNI::Color::WHITE, 200); // 200ms
        
    } else if (btn == UNI::ButtonEvent::LONG_PRESS) {
        LOG_WARNF("Core Temp: %.1f °C", sys.readCoreTemp());
        sys.setPixel(UNI::Color::BLUE, 500); // 500ms
    }
}