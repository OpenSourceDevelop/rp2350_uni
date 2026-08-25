#include "rp2350_uni.h"
#include <stdarg.h>

namespace UNI {

// --- WS2812-2020 PIO Code ---
static const uint16_t ws2812_instructions[] = {
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1123, //  1: jmp    !x, 3           side 1 [1]
    0x1400, //  2: jmp    0               side 1 [4]
    0x0400  //  3: jmp    0               side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_instructions,
    .length = 4,
    .origin = -1,
};

// --- Direct Stream PIO Encoder Program ---
static const uint16_t encoder_pio_instructions[] = {
    0x4002, // 0: in   pins, 2   ; Liest Pin A & B in ISR
    0x8020  // 1: push noblock   ; Pusht bedingungslos in den FIFO
};

static const struct pio_program encoder_pio_program = {
    .instructions = encoder_pio_instructions,
    .length = 2,
    .origin = -1,
};

// --- Main Loop Update ---
void System::update() {
    if (watchdog_enabled_) {
        kickWatchdog();
    }

    // Timer-Prüfung für automatisches Ausschalten
    if (led_off_time_ > 0 && millis() >= led_off_time_) {
        setPixel(Color::OFF, 0);
        led_off_time_ = 0;
    }

    if (btn_initialized_) {
        updateButton();
    }
}

void System::begin(uint32_t baud_rate, uint ws2812_pin, PIO pio_block, uint sm) {
    Serial.begin(baud_rate);
    pio_inst_ = pio_block;
    sm_ = sm;
    initWS2812PIO(ws2812_pin);
}

void System::initWS2812PIO(uint ws2812_pin) {
    offset_ = pio_add_program(pio_inst_, &ws2812_program);
    pio_gpio_init(pio_inst_, ws2812_pin);
    pio_sm_set_consecutive_pindirs(pio_inst_, sm_, ws2812_pin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset_, offset_ + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, ws2812_pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = (float)clock_get_hz(clk_sys) / 8000000.0f;
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio_inst_, sm_, offset_, &c);
    pio_sm_set_enabled(pio_inst_, sm_, true);

    ws2812_initialized_ = true;
    setPixel(Color::OFF);
}

// --- Globale Helligkeit & Farben ---
void System::setBrightness(uint8_t brightness) {
    global_brightness_ = brightness;
}

uint32_t System::scaleColor(uint32_t grb_color, uint8_t brightness) {
    if (brightness == 255) return grb_color;
    if (brightness == 0) return 0;

    uint8_t g = (grb_color >> 16) & 0xFF;
    uint8_t r = (grb_color >> 8) & 0xFF;
    uint8_t b = grb_color & 0xFF;

    g = (g * brightness) >> 8;
    r = (r * brightness) >> 8;
    b = (b * brightness) >> 8;

    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

void System::setPixel(uint32_t grb_color, uint32_t duration_ms) {
    if (!ws2812_initialized_) return;
    
    if (!pio_sm_is_tx_fifo_full(pio_inst_, sm_)) {
        uint32_t scaled_color = scaleColor(grb_color, global_brightness_);
        pio_sm_put(pio_inst_, sm_, scaled_color << 8u);
    }

    if (duration_ms > 0) {
        led_off_time_ = millis() + duration_ms;
    } else {
        led_off_time_ = 0; // Dauerhaftes Leuchten
    }
}

// --- Watchdog ---
void System::enableWatchdog(uint32_t delay_ms, bool pause_on_debug) {
    watchdog_enable(delay_ms, pause_on_debug);
    watchdog_enabled_ = true;
}

void System::kickWatchdog() {
    watchdog_update();
}

// --- ADC & MCU Temp ---
void System::initADC(uint gpio_pin, bool enable_temp_sensor) {
    adc_init();
    if (gpio_pin >= 26 && gpio_pin <= 29) {
        adc_gpio_init(gpio_pin);
    }
    if (enable_temp_sensor) {
        adc_set_temp_sensor_enabled(true);
    }
}

uint16_t System::readADC(uint input_channel) {
    adc_select_input(input_channel);
    return adc_read();
}

float System::readADCVolts(uint input_channel) {
    constexpr float conversion_factor = 3.3f / (1 << 12);
    return readADC(input_channel) * conversion_factor;
}

float System::readCoreTemp() {
    float raw_volts = readADCVolts(4);
    return 27.0f - ((raw_volts - 0.706f) / 0.001721f);
}

// --- Direct Hardware Register PIO Encoder ---
void System::initEncoder(uint pin_a, uint8_t divisor, PIO pio_block, uint sm) {
    enc_pio_inst_ = pio_block;
    enc_sm_ = sm;
    enc_pin_a_ = pin_a;
    quad_divisor_ = divisor;

    uint pin_b = pin_a + 1;

    uint offset = pio_add_program(enc_pio_inst_, &encoder_pio_program);

    if (enc_pio_inst_ == pio0) {
        gpio_set_function(pin_a, GPIO_FUNC_PIO0);
        gpio_set_function(pin_b, GPIO_FUNC_PIO0);
    } else {
        gpio_set_function(pin_a, GPIO_FUNC_PIO1);
        gpio_set_function(pin_b, GPIO_FUNC_PIO1);
    }

    gpio_set_dir(pin_a, GPIO_IN);
    gpio_set_dir(pin_b, GPIO_IN);
    gpio_disable_pulls(pin_a);
    gpio_disable_pulls(pin_b);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin_a);
    sm_config_set_in_shift(&c, false, false, 0);

    sm_config_set_wrap(&c, offset, offset + 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    float div = static_cast<float>(clock_get_hz(clk_sys)) / 2000.0f;
    sm_config_set_clkdiv(&c, div);

    pio_sm_clear_fifos(enc_pio_inst_, enc_sm_);
    pio_sm_init(enc_pio_inst_, enc_sm_, offset, &c);
    pio_sm_set_enabled(enc_pio_inst_, enc_sm_, true);

    last_enc_state_ = (gpio_get(pin_a) ? 1 : 0) | ((gpio_get(pin_b) ? 1 : 0) << 1);
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
    unsigned long now = millis();
    unsigned long dt = now - last_step_time_;
    last_step_time_ = now;

    int32_t multiplier = 1;
    if (dt < accel_threshold_ms_) {
        uint32_t diffMs = accel_threshold_ms_ - dt;
        multiplier = 1 + static_cast<int32_t>((diffMs * diffMs) / 250);
        if (multiplier > accel_multiplier_max_) {
            multiplier = accel_multiplier_max_;
        }
    }

    return rawDelta * multiplier;
}

int32_t System::getEncoderCount() {
    getPioRawCount();
    return current_count_ / static_cast<int32_t>(quad_divisor_);
}

void System::resetEncoderCount(int32_t val) {
    current_count_ = val * quad_divisor_;
    last_reported_count_ = current_count_;
}

// --- Push Button ---
void System::initButton(uint gpio_pin, bool active_low, uint32_t long_press_ms) {
    btn_pin_ = gpio_pin;
    btn_active_low_ = active_low;
    btn_long_press_ms_ = long_press_ms;

    gpio_init(btn_pin_);
    gpio_set_dir(btn_pin_, GPIO_IN);
    gpio_disable_pulls(btn_pin_);

    btn_initialized_ = true;
    btn_last_state_ = false;
    btn_pending_event_ = ButtonEvent::NONE;
}

void System::updateButton() {
    bool raw_read = gpio_get(btn_pin_);
    bool is_pressed = btn_active_low_ ? !raw_read : raw_read;
    uint32_t now = millis();

    if (is_pressed != btn_last_state_) {
        if (now - btn_last_debounce_time_ > 30) {
            btn_last_state_ = is_pressed;

            if (is_pressed) {
                btn_press_time_ = now;
            } else {
                uint32_t duration = now - btn_press_time_;
                if (duration >= btn_long_press_ms_) {
                    btn_pending_event_ = ButtonEvent::LONG_PRESS;
                } else if (duration >= 30) {
                    btn_pending_event_ = ButtonEvent::SHORT_PRESS;
                }
            }
        }
        btn_last_debounce_time_ = now;
    }
}

ButtonEvent System::getButtonEvent() {
    ButtonEvent evt = btn_pending_event_;
    btn_pending_event_ = ButtonEvent::NONE;
    return evt;
}

// --- Logging ---
void System::log(LogLevel level, const String& msg) {
    const char* prefix = "";

    switch (level) {
        case LogLevel::INFO:   prefix = "[INFO] "; break;
        case LogLevel::WARN:   prefix = "[WARN] "; break;
        case LogLevel::ERROR:  prefix = "[ERROR]"; break;
        case LogLevel::DEBUG:  prefix = "[DEBUG]"; break;
        case LogLevel::SYSTEM: prefix = "[SYS]  "; break;
    }

    Serial.print(prefix);
    Serial.print(" ");
    Serial.println(msg);
}

void System::logf(LogLevel level, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(level, String(buffer));
}

} // namespace UNI