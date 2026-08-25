#pragma once

#include <Arduino.h>
#include "hardware/watchdog.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

namespace UNI {

enum class LogLevel {
    INFO,
    WARN,
    ERROR,
    DEBUG,
    SYSTEM
};

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS
};

namespace Color {
    constexpr uint32_t OFF        = 0x00000000;
    constexpr uint32_t GREEN      = 0x00FF0000; // G, R, B
    constexpr uint32_t RED        = 0x0000FF00;
    constexpr uint32_t BLUE       = 0x000000FF;
    constexpr uint32_t YELLOW     = 0x00FFFF00;
    constexpr uint32_t CYAN       = 0x00FF00FF;
    constexpr uint32_t MAGENTA    = 0x0000FFFF;
    constexpr uint32_t WHITE      = 0x00FFFFFF;
    constexpr uint32_t WARM_WHITE = 0x0080FF20;
    constexpr uint32_t COLD_WHITE = 0x00FFFFE0;
}

class System {
public:
    static System& instance() {
        static System inst;
        return inst;
    }

    // --- Main Loop Task ---
    void update();

    // --- System Init ---
    void begin(uint32_t baud_rate, uint ws2812_pin, PIO pio_block = pio0, uint sm = 0);

    // --- Watchdog ---
    void enableWatchdog(uint32_t delay_ms = 4000, bool pause_on_debug = true);
    void kickWatchdog();

    // --- WS2812 Status LED & Globale Helligkeit ---
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return global_brightness_; }

    // HIER WAR DER FEHLER: Standardwert duration_ms = 0 im Header definieren!
    void setPixel(uint32_t grb_color, uint32_t duration_ms = 0);

    // --- ADC & MCU Temp ---
    void initADC(uint gpio_pin = 0, bool enable_temp_sensor = true);
    uint16_t readADC(uint input_channel);
    float readADCVolts(uint input_channel);
    float readCoreTemp();

    // --- PIO Direct-Stream Quadrature Encoder ---
    void initEncoder(uint pin_a, uint8_t divisor = 4, PIO pio_block = pio0, uint sm = 1);
    int32_t getEncoderCount();
    int32_t readEncoderDelta();
    void resetEncoderCount(int32_t val = 0);

    // --- Push Button ---
    void initButton(uint gpio_pin, bool active_low = true, uint32_t long_press_ms = 800);
    ButtonEvent getButtonEvent();

    // --- Logging ---
    void log(LogLevel level, const String& msg);
    void logf(LogLevel level, const char* format, ...);

private:
    System() = default;

    // WS2812 State
    PIO pio_inst_ = pio0;
    uint sm_ = 0;
    uint offset_ = 0;
    bool ws2812_initialized_ = false;
    uint32_t led_off_time_ = 0;
    uint8_t global_brightness_ = 25;

    // Direct Stream Encoder State
    PIO enc_pio_inst_ = pio0;
    uint enc_sm_ = 1;
    uint enc_pin_a_ = 0;
    int32_t current_count_ = 0;
    int32_t last_reported_count_ = 0;
    uint8_t last_enc_state_ = 0;
    uint8_t quad_divisor_ = 4;
    unsigned long last_step_time_ = 0;
    uint32_t accel_threshold_ms_ = 70;
    int32_t accel_multiplier_max_ = 40;

    // Button State
    uint btn_pin_ = 0;
    bool btn_initialized_ = false;
    bool btn_active_low_ = true;
    uint32_t btn_long_press_ms_ = 800;
    bool btn_last_state_ = false;
    uint32_t btn_press_time_ = 0;
    uint32_t btn_last_debounce_time_ = 0;
    ButtonEvent btn_pending_event_ = ButtonEvent::NONE;

    // Watchdog State
    bool watchdog_enabled_ = false;

    void initWS2812PIO(uint ws2812_pin);
    void updateButton();
    int32_t getPioRawCount();
    uint32_t scaleColor(uint32_t grb_color, uint8_t brightness);
};

} // namespace UNI

#define LOG_INFO(msg)    UNI::System::instance().log(UNI::LogLevel::INFO, msg)
#define LOG_WARN(msg)    UNI::System::instance().log(UNI::LogLevel::WARN, msg)
#define LOG_ERROR(msg)   UNI::System::instance().log(UNI::LogLevel::ERROR, msg)
#define LOG_DEBUG(msg)   UNI::System::instance().log(UNI::LogLevel::DEBUG, msg)
#define LOG_SYS(msg)     UNI::System::instance().log(UNI::LogLevel::SYSTEM, msg)

#define LOG_INFOF(fmt, ...)  UNI::System::instance().logf(UNI::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNF(fmt, ...)  UNI::System::instance().logf(UNI::LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_ERRORF(fmt, ...) UNI::System::instance().logf(UNI::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_DEBUGF(fmt, ...) UNI::System::instance().logf(UNI::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_SYSF(fmt, ...)   UNI::System::instance().logf(UNI::LogLevel::SYSTEM, fmt, ##__VA_ARGS__)