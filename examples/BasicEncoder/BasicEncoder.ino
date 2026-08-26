#include <Arduino.h>
#include <rp2350_uni.h>

constexpr uint8_t PIN_ENC_A   = 10; // Enc B = 11
constexpr uint8_t PIN_ENC_BTN = 12;

RP2350Uni::HardwarePIOEncoder encoder(PIN_ENC_A, PIN_ENC_BTN);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    RP2350Uni::System::begin();
    RP2350Uni::System::enableWatchdog(2000);

    encoder.begin();
    Serial.println(F("[RP2350_UNI] System Initialized."));
}

void loop() {
    RP2350Uni::System::feedWatchdog();

    int32_t delta = encoder.readDelta();
    if (delta != 0) {
        Serial.printf("Encoder Delta: %d\n", delta);
    }

    RP2350Uni::HardwarePIOEncoder::ButtonEvent btn = encoder.updateButton();
    if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::SHORT_PRESS) {
        Serial.println(F("Button: SHORT PRESS"));
    } else if (btn == RP2350Uni::HardwarePIOEncoder::ButtonEvent::LONG_PRESS) {
        Serial.println(F("Button: LONG PRESS"));
    }

    static uint32_t lastTempPrint = 0;
    if (millis() - lastTempPrint >= 2000) {
        lastTempPrint = millis();
        Serial.printf("MCU Temp: %.2f °C\n", RP2350Uni::System::readMcuTemperature());
    }
}