/**
 * @file    rp2350_uni.h
 * @brief   Universal Hardware Helper Library for RP2350 Microcontrollers
 * @author  OpenSourceDevelop
 * @version 1.0.0
 * @date    2026
 *
 * @note Designed for the Earle Philhower RP2040/RP2350 Arduino Core.
 */

#ifndef RP2350_UNI_H
#define RP2350_UNI_H

#include <Arduino.h>
#include <hardware/pio.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include <hardware/adc.h>
#include <hardware/watchdog.h>

namespace RP2350Uni {

/**
 * @brief System-wide configuration constants.
 */
namespace Config {
    constexpr float ADC_CONVERSION_FACTOR = 3.3f / 4095.0f;
    constexpr float TEMP_BASE_VOLTAGE     = 0.706f;
    constexpr float TEMP_SLOPE            = 0.001721f;
    constexpr float TEMP_BASE_C           = 27.0f;
}

/**
 * @brief System & System-Health Utility Helper Class
 */
class System {
public:
    /**
     * @brief Initializes internal hardware ADCs (e.g. MCU Temperature Sensor).
     */
    static void begin();

    /**
     * @brief Reads the internal RP2350 die temperature in °C.
     * @return Temperature in Degrees Celsius.
     */
    static float readMcuTemperature();

    /**
     * @brief Checks if the last system reset was triggered by the hardware watchdog.
     * @return True if watchdog reset occurred.
     */
    static bool wasWatchdogReset();

    /**
     * @brief Enables system hardware watchdog.
     * @param timeoutMs Reset timeout in milliseconds (max ~8300ms).
     */
    static void enableWatchdog(uint32_t timeoutMs = 2000);

    /**
     * @brief Feeds the hardware watchdog timer.
     */
    static void feedWatchdog();
};

/**
 * @brief Hardware PIO Quadrature Encoder Driver with dynamic acceleration and debouncing.
 */
class HardwarePIOEncoder {
public:
    enum class ButtonEvent : uint8_t {
        NONE = 0,
        SHORT_PRESS,
        LONG_PRESS
    };

private:
    PIO      _pio;
    uint     _sm;
    uint     _offset;
    uint8_t  _pinA;
    uint8_t  _pinBtn;

    int32_t  _currentCount;
    int32_t  _lastReportedCount;
    uint8_t  _lastState;
    uint32_t _lastStepTime;

    uint8_t  _quadratureDivisor;
    uint32_t _accelThresholdMs;
    int32_t  _accelMultiplierMax;
    uint32_t _debounceMs;
    uint32_t _longPressMs;

    enum class ButtonState : uint8_t {
        IDLE,
        DEBOUNCE_PRESS,
        PRESSED,
        DEBOUNCE_RELEASE
    };

    ButtonState _btnState;
    uint32_t    _btnStateTime;
    bool        _longPressReported;

    int32_t getPioRawCount();

public:
    /**
     * @brief Constructor for the PIO Encoder.
     * @param basePinA Base GPIO for Phase A (Phase B must be basePinA + 1).
     * @param btnPin   GPIO for Push Button.
     * @param pioInst  PIO Hardware Instance (pio0 or pio1).
     * @param divisor  Quadrature step divisor (typically 4 for full detent cycles).
     */
    HardwarePIOEncoder(uint8_t basePinA, uint8_t btnPin, PIO pioInst = pio0, uint8_t divisor = 4);

    /**
     * @brief Initializes PIO state machine, pins and hardware clocks.
     */
    void begin();

    /**
     * @brief Reads step delta since last call, applying dynamic acceleration.
     * @return Signed delta value.
     */
    int32_t readDelta();

    /**
     * @brief Polls button state machine.
     * @return ButtonEvent enum state.
     */
    ButtonEvent updateButton();
};

} // namespace RP2350Uni

#endif // RP2350_UNI_H