/**
 * @file     RainbowEncoder.ino
 * @brief    Controls WS2812 LED color hue dynamically via rotary encoder rotation using HSV conversion
 * @version  1.0
 */

#include <Arduino.h>
#include <rp_help.h>

#define WS2812_PIN       16
#define ENCODER_PIN_A    10 
#define ENCODER_BTN_PIN  12

auto& sys = rp_help::System::instance();
int16_t current_hue = 0; 

void setup() {
    sys.initSerial(115200);
    sys.initPixel(WS2812_PIN);

    sys.setBrightnessPixel(50);
    sys.enableWatchdog(4000);
    
    // Encoder und Button initialisieren (Pullups explizit aus wie gewünscht)
    sys.initEncoder(ENCODER_PIN_A, 4, ENCODER_BTN_PIN, 50, 800); 

    LOG_INFO("[SYSTEM] RainbowEncoder bereit!");
    LOG_INFOF("[SYSTEM] Aktuelle Helligkeit: %u", sys.getBrightnessPixel());
}

void loop() {
    sys.update();

    // Encoder-Raster abfragen
    int32_t delta = sys.readEncoderDelta();
    if (delta != 0) {
        current_hue += (delta * 1); 

        if (current_hue >= 360) current_hue %= 360;
        while (current_hue < 0) current_hue += 360;

        uint32_t new_color = rp_help::System::hsvToGRB(current_hue, 1.0f, 1.0f);
        sys.setPixel(new_color, 0); 

        LOG_INFOF("Farbton: %d°", current_hue);
    }

    // Button-Ereignisse abfragen
    rp_help::ButtonEvent btn = sys.getButtonEvent();

    if (btn == rp_help::ButtonEvent::SHORT_PRESS) {
        LOG_INFO("Kurzer Druck -> Dauerhaft WEISS");
        sys.setPixel(rp_help::Color::WHITE, 200); 
        
    } else if (btn == rp_help::ButtonEvent::LONG_PRESS) {
        LOG_INFO(" Langer Druck -> Blaues Signal");
        sys.setPixel(rp_help::Color::BLUE, 500); 
    }
}