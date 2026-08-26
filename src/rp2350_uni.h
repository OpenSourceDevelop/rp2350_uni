/**
 * @file    rp2350_uni.h
 * @brief   Universal C++ Embedded Library for RP2350 / RP2040 Microcontrollers
 * @author  OpenSourceDevelop
 * @date    2026
 */

#ifndef RP2350_UNI_H
#define RP2350_UNI_H

#include <Arduino.h>
#include <hardware/pio.h>

namespace RP2350Uni {

/**
 * @brief Pre-defined color constants in packed 32-bit GRB format (APA-104 / WS2812 compatible).
 */
namespace Color {
    constexpr uint32_t OFF    = 0x00000000;
    constexpr uint32_t RED    = 0x0000FF00;
    constexpr uint32_t GREEN  = 0x00FF0000;
    constexpr uint32_t BLUE   = 0x000000FF;
    constexpr uint32_t WHITE  = 0x00FFFFFF;
    constexpr uint32_t AMBER  = 0x008CFF00;
    constexpr uint32_t ORANGE = 0x0045FF00;
}

/**
 * @brief System level control, watchdog, MCU metrics, and native PIO APA-104 / WS2812 LED driver.
 */
class System {
public:
    /**
     * @brief Initializes internal peripherals, ADC temperature sensor, and native PIO LED state machine.
     * @param ledPin GPIO pin for the APA-104 / WS2812 status LED (default: 16).
     */
    static void begin(uint8_t ledPin = 16);

    /**
     * @brief Non-blocking state update engine. Must be called periodically in loop().
     */
    static void update();

    /**
     * @brief Reads internal RP2350 die temperature in degrees Celsius.
     * @return Temperature in °C.
     */
    static float readMcuTemperature();

    /**
     * @brief Checks if the last system reset was triggered by the hardware watchdog.
     * @return True if watchdog reset occurred.
     */
    static bool wasWatchdogReset();

    /**
     * @brief Enables the hardware watchdog timer.
     * @param timeoutMs Timeout period in milliseconds.
     */
    static void enableWatchdog(uint32_t timeoutMs);

    /**
     * @brief Resets (feeds) the hardware watchdog timer.
     */
    static void feedWatchdog();

    /**
     * @brief Sets status LED color with optional non-blocking duration/blink interval.
     * @param grbColor Packed 32-bit color in GRB format.
     * @param durationMs Optional duration in ms for temporary flash or blink toggle interval.
     */
    static void setPixel(uint32_t grbColor, uint32_t durationMs = 0);

    /**
     * @brief Sets global brightness scale for status LED.
     * @param brightness Scale from 0 (off) to 255 (full brightness).
     */
    static void setBrightness(uint8_t brightness);

private:
    static uint8_t  s_ledPin;
    static uint8_t  s_brightness;
    static uint32_t s_currentColor;
    static uint32_t s_ledTimer;
    static uint32_t s_blinkInterval;
    static bool     s_blinkState;

    // Hardware PIO Instance & State Machine for APA-104 / WS2812
    static PIO      s_ledPio;
    static uint     s_ledSm;
    static uint     s_ledOffset;

    static void pushPixel(uint32_t grbColor);
};

/**
 * @brief Hardware PIO Quadrature Encoder Driver with integrated non-blocking button logic.
 */
class HardwarePIOEncoder {
public:
    enum class ButtonEvent : uint8_t {
        NONE = 0,
        SHORT_PRESS,
        LONG_PRESS
    };

    /**
     * @brief Constructor for PIO Encoder.
     * @param basePinA GPIO pin for Encoder Phase A (Phase B must be basePinA + 1).
     * @param btnPin GPIO pin for the encoder push button.
     * @param pio Target PIO peripheral instance (default: pio0).
     * @param stepDivisor Quadrature step division factor (default: 4 for standard detents).
     */
    HardwarePIOEncoder(uint8_t basePinA, uint8_t btnPin, PIO pio = pio0, uint8_t stepDivisor = 4);

    /**
     * @brief Claims a PIO state machine, loads quadrature program, and configures GPIOs.
     */
    void begin();

    /**
     * @brief Reads step count delta since the last call, applying dynamic acceleration.
     * @return Relative movement delta (+/- steps).
     */
    int32_t readDelta();

    /**
     * @brief Non-blocking update loop for button state debouncing and event classification.
     * @return ButtonEvent (NONE, SHORT_PRESS, LONG_PRESS).
     */
    ButtonEvent updateButton();

private:
    uint8_t  m_pinA;
    uint8_t  m_pinB;
    uint8_t  m_btnPin;
    PIO      m_pio;
    uint8_t  m_sm;
    uint8_t  m_stepDivisor;

    int32_t  m_lastPosition;
    uint32_t m_lastReadTime;

    // Button FSM State
    uint32_t m_btnPressTime;
    bool     m_btnWasPressed;
};

} // namespace RP2350Uni

#endif // RP2350_UNI_H