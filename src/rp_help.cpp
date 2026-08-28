#include "rp_help.h"
#include <hardware/watchdog.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>

namespace rp_help {

// ── WS2812 PIO Program Assembly ──
static const uint16_t ws2812_program_instructions[] = {
    0x6221, // 0: out x, 1             side 0 [2]
    0x1123, // 1: jmp !x, 3            side 1 [1]
    0x1400, // 2: jmp 0                side 1 [4]
    0xa442, // 3: nop                  side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

void System::initSerial(uint32_t baud_rate) {
    Serial.begin(baud_rate);
    while (!Serial && millis() < 2000) {
        // Wait for serial connection with timeout
    }
}

void System::initPixel(uint ws2812_pin, PIO pio_block, uint sm) {
    pio_inst_ = pio_block;
    sm_ = sm;
    
    initWS2812PIO(ws2812_pin);
    ws2812_initialized_ = true;
    
    setPixel(Color::OFF, 0);
}

void System::initWS2812PIO(uint ws2812_pin) {
    uint offset = pio_add_program(pio_inst_, &ws2812_program);
    offset_ = offset;
    
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, ws2812_pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    int cycles_per_bit = 3 + 3 + 4; 
    float div = (float)clock_get_hz(clk_sys) / (800000.f * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);

    gpio_init(ws2812_pin);
    gpio_set_function(ws2812_pin, (pio_inst_ == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    pio_gpio_init(pio_inst_, ws2812_pin);
    pio_sm_set_consecutive_pindirs(pio_inst_, sm_, ws2812_pin, 1, true);

    pio_sm_init(pio_inst_, sm_, offset, &c);
    pio_sm_set_enabled(pio_inst_, sm_, true);
}

void System::setBrightnessPixel(uint8_t brightness) {
    global_brightness_ = brightness;
}

uint32_t System::scaleColor(uint32_t grb_color, uint8_t brightness) {
    if (brightness == 0) return 0;
    if (brightness >= 255) return grb_color;

    uint8_t g = (grb_color >> 16) & 0xFF;
    uint8_t r = (grb_color >> 8) & 0xFF;
    uint8_t b = grb_color & 0xFF;

    g = (g * brightness) / 255;
    r = (r * brightness) / 255;
    b = (b * brightness) / 255;

    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

void System::setPixel(uint32_t grb_color, uint32_t duration_ms) {
    if (!ws2812_initialized_) return;
    
    flash_active_ = false;

    if (!pio_sm_is_tx_fifo_full(pio_inst_, sm_)) {
        uint32_t scaled_color = scaleColor(grb_color, global_brightness_);
        pio_sm_put(pio_inst_, sm_, scaled_color << 8u);
    }

    if (duration_ms > 0) {
        led_off_time_ = millis() + duration_ms;
    } else {
        led_off_time_ = 0;
    }
}

void System::setFlash(uint32_t color1, uint32_t color2, uint32_t interval_ms) {
    flash_color1_ = color1;
    flash_color2_ = color2;
    flash_interval_ = interval_ms;
    flash_timer_ = millis();
    flash_state_ = true;
    flash_active_ = true;
    
    if (!pio_sm_is_tx_fifo_full(pio_inst_, sm_)) {
        pio_sm_put(pio_inst_, sm_, scaleColor(color1, global_brightness_) << 8u);
    }
}

// ── ENCODER (PIO-based - Korrigiertes Programm mit Autopush) ──
static const uint16_t encoder_program_instructions[] = {
    0x4002, // 0: in pins, 2
};

static const struct pio_program encoder_program = {
    .instructions = encoder_program_instructions,
    .length = 1,
    .origin = -1,
};

void System::initEncoder(uint pin_a, uint8_t divisor, int8_t btn_pin, uint32_t short_press_ms, uint32_t long_press_ms, PIO pio_block, uint sm) {
    enc_pin_a_ = pin_a;
    quad_divisor_ = divisor;
    enc_pio_inst_ = pio_block;
    enc_sm_ = sm;

    gpio_init(pin_a);
    gpio_init(pin_a + 1);
    
    // Interne Pullups explizit deaktiviert (da in HW auf Platine ausgeführt)
    gpio_disable_pulls(pin_a);
    gpio_disable_pulls(pin_a + 1);
    
    gpio_set_function(pin_a, (enc_pio_inst_ == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    gpio_set_function(pin_a + 1, (enc_pio_inst_ == pio0) ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
    pio_gpio_init(enc_pio_inst_, pin_a);
    pio_gpio_init(enc_pio_inst_, pin_a + 1);

    uint offset = pio_add_program(enc_pio_inst_, &encoder_program);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin_a);
    sm_config_set_wrap(&c, offset, offset);
    
    // Autopush nach 2 Bits aktivieren, damit der FIFO sauber mit den Pin-Zuständen gefüttert wird
    sm_config_set_in_shift(&c, false, true, 2);

    // Taktteiler für sanftes, sauberes Abfragen (verhindert FIFO-Überflutung)
    float div = static_cast<float>(clock_get_hz(clk_sys)) / 100000.0f;
    sm_config_set_clkdiv(&c, div);

    pio_sm_clear_fifos(enc_pio_inst_, enc_sm_);
    pio_sm_init(enc_pio_inst_, enc_sm_, offset, &c);
    pio_sm_set_enabled(enc_pio_inst_, enc_sm_, true);

    last_enc_state_ = (gpio_get(pin_a) ? 1 : 0) | ((gpio_get(pin_a + 1) ? 1 : 0) << 1);
    current_count_ = 0;
    last_reported_count_ = 0;

    if (btn_pin >= 0) {
        btn_pin_ = btn_pin;
        btn_short_press_ms_ = short_press_ms;
        btn_long_press_ms_ = long_press_ms;
        pinMode(btn_pin_, INPUT);
        gpio_disable_pulls(btn_pin_);
        btn_initialized_ = true;
    }
}

int32_t System::getPioRawCount() {
    while (!pio_sm_is_rx_fifo_empty(enc_pio_inst_, enc_sm_)) {
        uint32_t sample = pio_sm_get(enc_pio_inst_, enc_sm_);
        uint8_t currState = sample & 0x03;

        if (currState != last_enc_state_) {
            static const int8_t stateTable[16] = {
                 0, -1,  1,  0,
                 1,  0,  0, -1,
                -1,  0,  0,  1,
                 0,  1, -1,  0
            };
            uint8_t idx = ((last_enc_state_ << 2) | currState) & 0x0F;
            current_count_ += stateTable[idx];
            last_enc_state_ = currState;
        }
    }
    return current_count_;
}

int32_t System::readEncoderDelta() {
    int32_t currentRaw = getPioRawCount();
    int32_t diff = currentRaw - last_reported_count_;
    int32_t rawDelta = diff / static_cast<int32_t>(quad_divisor_);

    if (rawDelta == 0) {
        return 0;
    }

    last_reported_count_ += rawDelta * quad_divisor_;
    return rawDelta;
}

int32_t System::getEncoderCount() {
    getPioRawCount();
    return current_count_ / static_cast<int32_t>(quad_divisor_);
}

void System::resetEncoderCount(int32_t val) {
    current_count_ = val * quad_divisor_;
    last_reported_count_ = current_count_;
}

// ── BUTTON HANDLING ──
void System::updateButton() {
    if (!btn_initialized_) return;

    bool raw_state = (digitalRead(btn_pin_) == LOW); 
    unsigned long now = millis();

    if (raw_state != btn_last_state_) {
        if (now - btn_last_debounce_time_ > 20) {
            btn_last_state_ = raw_state;
            if (raw_state) {
                btn_press_time_ = now;
            } else {
                uint32_t duration = now - btn_press_time_;
                if (duration >= btn_long_press_ms_) {
                    btn_pending_event_ = ButtonEvent::LONG_PRESS;
                } else if (duration >= btn_short_press_ms_) {
                    btn_pending_event_ = ButtonEvent::SHORT_PRESS;
                }
            }
            btn_last_debounce_time_ = now;
        }
    }
}

ButtonEvent System::getButtonEvent() {
    ButtonEvent ev = btn_pending_event_;
    btn_pending_event_ = ButtonEvent::NONE;
    return ev;
}

// ── WATCHDOG ──
void System::enableWatchdog(uint32_t delay_ms, bool pause_on_debug) {
    watchdog_enable(delay_ms, pause_on_debug);
    watchdog_enabled_ = true;
}

void System::kickWatchdog() {
    if (watchdog_enabled_) {
        watchdog_update();
    }
}

// ── SYSTEM UPDATE LOOP ──
void System::update() {
    if (watchdog_enabled_) {
        kickWatchdog();
    }

    if (flash_active_) {
        if (millis() - flash_timer_ >= flash_interval_) {
            flash_timer_ = millis();
            flash_state_ = !flash_state_;
            if (!pio_sm_is_tx_fifo_full(pio_inst_, sm_)) {
                uint32_t col = flash_state_ ? flash_color1_ : flash_color2_;
                pio_sm_put(pio_inst_, sm_, scaleColor(col, global_brightness_) << 8u);
            }
        }
    } else if (led_off_time_ > 0 && millis() >= led_off_time_) {
        setPixel(Color::OFF, 0);
        led_off_time_ = 0;
    }

    if (btn_initialized_) {
        updateButton();
    }
}

// ── LOGGING ──
void System::log(LogLevel level, const String& msg) {
    const char* prefix = "[INFO]";
    switch (level) {
        case LogLevel::INFO:   prefix = "[INFO]";   break;
        case LogLevel::WARN:   prefix = "[WARN]";   break;
        case LogLevel::ERROR:  prefix = "[ERROR]";  break;
        case LogLevel::DEBUG:  prefix = "[DEBUG]";  break;
        case LogLevel::SYSTEM: prefix = "[SYS]";    break;
    }
    Serial.print(prefix);
    Serial.print(" ");
    Serial.println(msg);
}

void System::logf(LogLevel level, const char* format, ...) {
    char loc_buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(loc_buf, sizeof(loc_buf), format, args);
    va_end(args);
    log(level, String(loc_buf));
}

// ── HSV TO GRB CONVERSION ──
uint32_t System::hsvToGRB(uint16_t h, float s, float v) {
    float r = 0, g = 0, b = 0;
    int i = h / 60;
    float f = h / 60.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }

    uint8_t red   = (uint8_t)(r * 255.0f);
    uint8_t green = (uint8_t)(g * 255.0f);
    uint8_t blue  = (uint8_t)(b * 255.0f);

    return ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
}

} // namespace rp_help