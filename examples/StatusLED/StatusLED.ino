/**
 * @file    StatusLED.ino
 * @brief   Clean Status LED example utilizing the rp_help library's built-in non-blocking setFlash()
 * @version 1.0
 */

#include <Arduino.h>
#include <rp_help.h>

// ── HARDWARE DEFINITIONS ──
constexpr uint8_t STATUS_LED_PIN = 16;  // Onboard WS2812 GPIO
constexpr uint8_t PIN_ENC_A      = 10;  // Phase A GPIO (Phase B is Pin A + 1)
constexpr uint8_t PIN_ENC_BTN    = 12;  // Button GPIO (active LOW)

auto& sys = rp_help::System::instance();

// ── SYSTEM STATE ENUM (nur für die Applikationslogik im Sketch) ──
enum class SystemStatus : uint8_t {
    IDLE = 0,
    HEATING,
    READY,
    ERROR
};

SystemStatus g_status = SystemStatus::IDLE;

/**
 * @brief Updates status LED color or triggers background animations via library
 */
void updateStatusLED(SystemStatus status) {
    switch (status) {
        case SystemStatus::IDLE:
            sys.setPixel(rp_help::Color::WHITE, 0);
            break;

        case SystemStatus::HEATING:
            sys.setPixel(rp_help::Color::YELLOW, 500);
            break;

        case SystemStatus::READY:
            sys.setPixel(rp_help::Color::GREEN, 0);
            break;

        case SystemStatus::ERROR:
            // Der Rot-Blau-Blitzer läuft komplett autonom im Hintergrund der Bibliothek!
            sys.setFlash(rp_help::Color::RED, rp_help::Color::BLUE, 60);
            break;
    }
}

void setup() {
    sys.initSerial(115200);

    sys.initPixel(STATUS_LED_PIN);
    sys.setBrightnessPixel(30);
    sys.enableWatchdog(2000);

    // Initialisierung des Encoders (interne Pullups bleiben laut Vorgabe deaktiviert)
    sys.initEncoder(PIN_ENC_A, 4, PIN_ENC_BTN, 50, 800);

    // Initialer Startblitz (Weiß für 500ms)
    sys.setPixel(rp_help::Color::WHITE, 500);

    LOG_INFO("[RP_HELP] Status LED Example Started.");
    LOG_INFO("Press Button to cycle Status: IDLE -> HEATING -> READY -> ERROR");
}

void loop() {
    // Hält Watchdog, Button-Abfrage und den Hintergrund-Blitzer am Laufen
    sys.update();

    // Button-Ereignisse abfragen
    rp_help::ButtonEvent btn = sys.getButtonEvent();
    
    if (btn == rp_help::ButtonEvent::SHORT_PRESS) {
        uint8_t nextState = (static_cast<uint8_t>(g_status) + 1) % 4;
        g_status = static_cast<SystemStatus>(nextState);
        
        LOG_INFOF("New State: %d", static_cast<uint8_t>(g_status));
        updateStatusLED(g_status);
    } 
    else if (btn == rp_help::ButtonEvent::LONG_PRESS) {
        LOG_INFO("Button: LONG PRESS");
    }
}