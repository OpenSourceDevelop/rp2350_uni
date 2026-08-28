/**
 * @file    BasicEncoder.ino
 * @brief   Basic example for PIO quadrature encoder and button handling using rp_help
 */

#include <Arduino.h>
#include <rp_help.h>

// ── HARDWARE DEFINITIONS ──
constexpr uint8_t STATUS_LED_PIN = 16;  // Onboard WS2812 GPIO
constexpr uint8_t PIN_ENC_A      = 10;  // Encoder Phase A GPIO (Phase B is Pin A + 1)
constexpr uint8_t PIN_ENC_BTN    = 12;  // Button GPIO (active LOW)

auto& sys = rp_help::System::instance();

void setup() {
    sys.initSerial(115200);

    // Peripherie initialisieren
    sys.initPixel(STATUS_LED_PIN);
    sys.setBrightnessPixel(20);
    sys.enableWatchdog(2000);

    // Encoder initialisieren (Viertelschritt-Teiler = 4, Button auf Pin 12)
    // Interne Pullups bleiben laut Vorgabe deaktiviert (externe Beschaltung auf Platine)
    sys.initEncoder(PIN_ENC_A, 4, PIN_ENC_BTN, 50, 800);

    // Start-Indikator (Blau für 500ms)
    sys.setPixel(rp_help::Color::BLUE, 500);

    LOG_INFO("BasicEncoder Example Started.");
    LOG_INFO("Turn encoder to change count. Press button to reset count to 0.");
}

void loop() {
    // Muss regelmäßig aufgerufen werden (Watchdog, Button, Blinker)
    sys.update();

    // Encoder-Delta auslesen (gibt Schritte seit dem letzten Loop-Durchlauf zurück)
    int32_t delta = sys.readEncoderDelta();
    if (delta != 0) {
        int32_t total = sys.RegelReadEncoderCount(); // Gesamten Zählerstand holen
        LOG_INFOF("Encoder Delta: %d | Total: %d", delta, sys.getEncoderCount());
        
        // Optional: Kurzes optisches Feedback am LED
        sys.setPixel(rp_help::Color::CYAN, 50);
    }

    // Button-Ereignisse abfragen
    rp_help::ButtonEvent btn = sys.getButtonEvent();
    if (btn == rp_help::ButtonEvent::SHORT_PRESS) {
        LOG_INFO("Button SHORT PRESS: Resetting encoder count to 0.");
        sys.resetEncoderCount(0);
        sys.setPixel(rp_help::Color::GREEN, 200);
    } 
    else if (btn == rp_help::ButtonEvent::LONG_PRESS) {
        LOG_INFO("Button LONG PRESS detected.");
        sys.setFlash(rp_help::Color::MAGENTA, rp_help::Color::OFF, 100);
    }
}