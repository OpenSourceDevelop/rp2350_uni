/**
 * @file    rp2350_uni.cpp
 * @brief   Implementation of RP2350 Hardware Helper Library
 * @author  OpenSourceDevelop
 * @version 1.0.0
 */

#include "rp2350_uni.h"

namespace RP2350Uni {

// ── PIO ASSEMBLED INSTRUCTION BLOCK ──
static const uint16_t encoder_pio_instructions[] = {
    0x4002, // 0: in  pins, 2
    0x8020  // 1: push noblock
};

static const struct pio_program encoder_pio_program = {
    .instructions = encoder_pio_instructions,
    .length = 2,
    .origin = -1,
};

// ─────────────────────────────────────────────
//  System Class Implementation
// ─────────────────────────────────────────────
void System::begin() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

float System::readMcuTemperature() {
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * Config::ADC_CONVERSION_FACTOR;
    return Config::TEMP_BASE_C - (voltage - Config::TEMP_BASE_VOLTAGE) / Config::TEMP_SLOPE;
}

bool System::wasWatchdogReset() {
    return watchdog_caused_reboot();
}

void System::enableWatchdog(uint32_t timeoutMs) {
    watchdog_enable(timeoutMs, 1);
}

void System::feedWatchdog() {
    watchdog_update();
}

// ─────────────────────────────────────────────
//  HardwarePIOEncoder Implementation
// ─────────────────────────────────────────────
HardwarePIOEncoder::HardwarePIOEncoder(uint8_t basePinA, uint8_t btnPin, PIO pioInst, uint8_t divisor)
    : _pio(pioInst), _sm(0), _offset(0), _pinA(basePinA), _pinBtn(btnPin),
      _currentCount(0), _lastReportedCount(0), _lastState(0), _lastStepTime(0),
      _quadratureDivisor(divisor), _accelThresholdMs(70), _accelMultiplierMax(40),
      _debounceMs(15), _longPressMs(600),
      _btnState(ButtonState::IDLE), _btnStateTime(0), _longPressReported(false) {}

void HardwarePIOEncoder::begin() {
    pinMode(_pinBtn, INPUT_PULLUP);

    _sm = pio_claim_unused_sm(_pio, true);
    _offset = pio_add_program(_pio, &encoder_pio_program);

    gpio_set_function(_pinA, (_pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    gpio_set_function(_pinA + 1, (_pio == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    gpio_set_dir(_pinA, GPIO_IN);
    gpio_set_dir(_pinA + 1, GPIO_IN);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, _pinA);
    sm_config_set_in_shift(&c, false, false, 0);
    sm_config_set_wrap(&c, _offset, _offset + 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    float div = static_cast<float>(clock_get_hz(clk_sys)) / 2000.0f;
    sm_config_set_clkdiv(&c, div);

    pio_sm_clear_fifos(_pio, _sm);
    pio_sm_init(_pio, _sm, _offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);

    _lastState = (gpio_get(_pinA) ? 1 : 0) | ((gpio_get(_pinA + 1) ? 1 : 0) << 1);
}

int32_t HardwarePIOEncoder::getPioRawCount() {
    while (!pio_sm_is_rx_fifo_empty(_pio, _sm)) {
        uint32_t sample = pio_sm_get(_pio, _sm);
        uint8_t currState = sample & 0x03;

        if (currState != _lastState) {
            static const int8_t stateTable[16] = {
                 0, -1,  1,  0,
                 1,  0,  0, -1,
                -1,  0,  0,  1,
                 0,  1, -1,  0
            };
            uint8_t idx = ((_lastState << 2) | currState) & 0x0F;
            _currentCount += stateTable[idx];
            _lastState = currState;
        }
    }
    return _currentCount;
}

int32_t HardwarePIOEncoder::readDelta() {
    int32_t currentRaw = getPioRawCount();
    int32_t diff = currentRaw - _lastReportedCount;
    int32_t rawDelta = diff / static_cast<int32_t>(_quadratureDivisor);

    if (rawDelta == 0) return 0;

    _lastReportedCount += rawDelta * _quadratureDivisor;
    uint32_t now = millis();
    uint32_t dt = now - _lastStepTime;
    _lastStepTime = now;

    int32_t multiplier = 1;
    if (dt < _accelThresholdMs) {
        uint32_t diffMs = _accelThresholdMs - dt;
        multiplier = 1 + static_cast<int32_t>((diffMs * diffMs) / 250);
        if (multiplier > _accelMultiplierMax) {
            multiplier = _accelMultiplierMax;
        }
    }
    return rawDelta * multiplier;
}

HardwarePIOEncoder::ButtonEvent HardwarePIOEncoder::updateButton() {
    bool btnPressed = (digitalRead(_pinBtn) == LOW);
    uint32_t now = millis();
    ButtonEvent event = ButtonEvent::NONE;

    switch (_btnState) {
        case ButtonState::IDLE:
            if (btnPressed) { _btnState = ButtonState::DEBOUNCE_PRESS; _btnStateTime = now; }
            break;
        case ButtonState::DEBOUNCE_PRESS:
            if (now - _btnStateTime >= _debounceMs) {
                if (btnPressed) { _btnState = ButtonState::PRESSED; _longPressReported = false; }
                else _btnState = ButtonState::IDLE;
            }
            break;
        case ButtonState::PRESSED:
            if (!btnPressed) { _btnState = ButtonState::DEBOUNCE_RELEASE; _btnStateTime = now; }
            else if (!_longPressReported && (now - _btnStateTime >= _longPressMs)) {
                _longPressReported = true;
                event = ButtonEvent::LONG_PRESS;
            }
            break;
        case ButtonState::DEBOUNCE_RELEASE:
            if (now - _btnStateTime >= _debounceMs) {
                if (!btnPressed) {
                    _btnState = ButtonState::IDLE;
                    if (!_longPressReported) event = ButtonEvent::SHORT_PRESS;
                } else _btnState = ButtonState::PRESSED;
            }
            break;
    }
    return event;
}

} // namespace RP2350Uni