#include "src/rp2350_uni.h"

// --- Pin Configuration ---
#define WS2812_PIN       16
#define ENCODER_PIN_A    10 // Pin B is automatically GPIO 11
#define ENCODER_BTN_PIN  12

// Get Singleton instance
auto& sys = UNI::System::instance();
int16_t current_hue = 0; 

// HSV to GRB conversion for rainbow colors
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

    return ((uint32_t)(uint8_t)((g + m) * 255.0f) << 16) | 
           ((uint32_t)(uint8_t)((r + m) * 255.0f) << 8)  | 
            (uint8_t)((b + m) * 255.0f);
}

void setup() {
    // 1. Initialize System & WS2812 (Baud rate, Pin)
    sys.begin(115200, WS2812_PIN);
    while (!Serial && millis() < 2000);

    // 2. Setup Hardware Modules
    sys.setBrightness(30);                   // Brightness (0-255)
    sys.initADC(26, true);                   // ADC + Temp sensor
    sys.enableWatchdog(4000);                // Watchdog 4 sec
    sys.initEncoder(ENCODER_PIN_A, 4);       // Encoder (Base pin A, Divisor)
    sys.initButton(ENCODER_BTN_PIN, true);   // Button (Pullup active-low)

    // Initial State: Solid White (duration_ms = 0)
    sys.setPixel(UNI::Color::WHITE, 0);

    LOG_INFO(F("[SYSTEM] Successfully started!"));
}

void loop() {
    // Crucial: Executes watchdog kick & button/LED timers
    sys.update();

    // 1. Read Encoder (Color selection via rotation)
    int32_t delta = sys.readEncoderDelta();
    if (delta != 0) {
        current_hue = (current_hue + (delta * 5)) % 360;
        if (current_hue < 0) current_hue += 360;

        // Set color (0 = remains on continuously)
        sys.setPixel(hsvToGRB(current_hue, 1.0f, 1.0f), 0);
        LOG_INFOF("Hue: %d°", current_hue);
    }

    // 2. Evaluate Button Press
    UNI::ButtonEvent btn = sys.getButtonEvent();

    if (btn == UNI::ButtonEvent::SHORT_PRESS) {
        LOG_INFO(F("Button short press -> Reset to White"));
        sys.setPixel(UNI::Color::WHITE, 0); // Solid White
        
    } else if (btn == UNI::ButtonEvent::LONG_PRESS) {
        LOG_WARNF("Core Temperature: %.1f °C", sys.readCoreTemp());
        sys.setPixel(UNI::Color::BLUE, 500); // Flash Blue for 500ms as feedback
    }
}
