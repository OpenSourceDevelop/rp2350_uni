#pragma once

#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/gpio.h"

namespace rp_help {

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
    constexpr uint32_t COLD_WHITE = 0x00E0E0FF; // G=0xE0, R=0xE0, B=0xFF -> Blau dominant
}

class System {
public:
    static System& instance() {
        static System inst;
        return inst;
    }

    void update();
    void initSerial(uint32_t baud_rate);
    void initPixel(uint ws2812_pin, PIO pio_block = pio0, uint sm = 0);
    void setBrightnessPixel(uint8_t brightness);
    uint8_t getBrightnessPixel() const { return global_brightness_; }

    void initEncoder(uint pin_a, uint8_t divisor = 4, int8_t btn_pin = -1, uint32_t short_press_ms = 50, uint32_t long_press_ms = 800, bool btn_active_low = true, PIO pio_block = pio0, uint sm = 1);

    void enableWatchdog(uint32_t delay_ms = 4000, bool pause_on_debug = true);
    void kickWatchdog();

    void setPixel(uint32_t grb_color, uint32_t duration_ms = 0);
    void setFlash(uint32_t color1, uint32_t color2, uint32_t interval_ms);

    int32_t getEncoderCount();
    int32_t readEncoderDelta();
    void resetEncoderCount(int32_t val = 0);

    ButtonEvent getButtonEvent();

    void log(LogLevel level, const String& msg);
    void logf(LogLevel level, const char* format, ...);

    static uint32_t hsvToGRB(uint16_t h, float s, float v);

private:
    System() = default;

    PIO pio_inst_ = pio0;
    uint sm_ = 0;
    uint offset_ = 0;
    bool ws2812_initialized_ = false;
    uint32_t led_off_time_ = 0;
    uint8_t global_brightness_ = 25;

    bool flash_active_ = false;
    uint32_t flash_color1_ = 0;
    uint32_t flash_color2_ = 0;
    uint32_t flash_interval_ = 0;
    uint32_t flash_timer_ = 0;
    bool flash_state_ = false;

    PIO enc_pio_inst_ = pio0;
    uint enc_sm_ = 1;
    uint enc_pin_a_ = 0;
    int32_t current_count_ = 0;
    int32_t last_reported_count_ = 0;
    uint8_t last_enc_state_ = 0;
    uint8_t quad_divisor_ = 4;

    int8_t btn_pin_ = -1;
    bool btn_initialized_ = false;
    bool btn_active_low_ = true;
    uint32_t btn_short_press_ms_ = 50;
    uint32_t btn_long_press_ms_ = 800;
    bool btn_last_state_ = false;
    uint32_t btn_press_time_ = 0;
    uint32_t btn_last_debounce_time_ = 0;
    ButtonEvent btn_pending_event_ = ButtonEvent::NONE;

    bool watchdog_enabled_ = false;

    void initWS2812PIO(uint ws2812_pin);
    void updateButton();
    int32_t getPioRawCount();
    uint32_t scaleColor(uint32_t grb_color, uint8_t brightness);
    static uint pioFunc(PIO pio_block);
};

} // namespace rp_help

// ── LOGGING MACROS ──
#define LOG_INFO(msg)    rp_help::System::instance().log(rp_help::LogLevel::INFO, msg)
#define LOG_WARN(msg)    rp_help::System::instance().log(rp_help::LogLevel::WARN, msg)
#define LOG_ERROR(msg)   rp_help::System::instance().log(rp_help::LogLevel::ERROR, msg)
#define LOG_DEBUG(msg)   rp_help::System::instance().log(rp_help::LogLevel::DEBUG, msg)
#define LOG_SYS(msg)     rp_help::System::instance().log(rp_help::LogLevel::SYSTEM, msg)

#define LOG_INFOF(fmt, ...)  rp_help::System::instance().logf(rp_help::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNF(fmt, ...)  rp_help::System::instance().logf(rp_help::LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_ERRORF(fmt, ...) rp_help::System::instance().logf(rp_help::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_DEBUGF(fmt, ...) rp_help::System::instance().logf(rp_help::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_SYSF(fmt, ...)   rp_help::System::instance().logf(rp_help::LogLevel::SYSTEM, fmt, ##__VA_ARGS__)