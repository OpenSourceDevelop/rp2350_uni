/**
 * @file    rp2350_uni.cpp
 * @brief   Implementation of RP2350Uni System and Hardware PIO Encoder classes.
 *          Optimized for RP2350 with external hardware pull-up resistors.
 * @author  OpenSourceDevelop
 * @date    2026
 */

#include "rp2350_uni.h"
#include <hardware/adc.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include <hardware/pio.h>
#include <hardware/gpio.h>

namespace RP2350Uni {

// ── SYSTEM STATIC MEMBER INITIALIZATION ──
uint8_t  System::s_ledPin        = 16;
uint8_t  System::s_brightness    = 255;
uint32_t System::s_currentColor  = Color::OFF;
uint32_t System::s_ledTimer      = 0;
uint32_t System::s_blinkInterval = 0;
bool     System::s_blinkState    = false;

PIO      System::s_ledPio        = pio1; // Reserve pio1 for LED (pio0 reserved for Encoder)
uint     System::s_ledSm         = 0;
uint     System::s_ledOffset     = 0;

// ── PIO PROGRAM FOR APA-104 / WS2812 (800 kHz NRZ Protocol) ──
static const uint16_t ws2812_pio_instructions[] = {
    // Side-set 1 bit: Bit 0 = Pin Level
    0x6221, // 0: out    x, 1           side 0 [2]
    0x1123, // 1: jmp    !x, 3          side 1 [1]
    0x1400, // 2: jmp    0              side 1 [4]
    0xa442  // 3: nop                   side 0 [4]
};

static const struct pio_program ws2812_pio_program = {
    .instructions = ws2812_pio_instructions,
    .length = 4,
    .origin = -1,
};

// ── PIO PROGRAM FOR QUADRATURE ENCODER DECODER ──
static const uint16_t encoder_pio_instructions[] = {
    0x4002, // 0: in     pins, 2
    0xa020, // 1: mov    x, status
    0x00a0, // 2: jmp    x--, 0
    0xa001, // 3: mov    pins, x
};

static const struct pio_program encoder_pio_program = {
    .instructions = encoder_pio_instructions,
    .length = 4,
    .origin = -1,
};

// ── SYSTEM IMPLEMENTATION ──

void System::begin(uint8_t ledPin) {
    s_ledPin = ledPin;

    // Initialize ADC for MCU Temperature Sensor
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Hardware PIO Setup for APA-104 / WS2812 on pio1
    s_ledSm = pio_claim_unused_sm(s_ledPio, true);
    s_ledOffset = pio_add_program(s_ledPio, &ws2812_pio_program);

    pio_gpio_init(s_ledPio, s_ledPin);
    pio_sm_set_consecutive_pindirs(s_ledPio, s_ledSm, s_ledPin, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, s_ledOffset, s_ledOffset + 3);
    sm_config_set_sideset_pins(&c, s_ledPin);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_out_shift(&c, false, true, 24); // MSB first, autopull 24 bit
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Set PIO clock divisor to hit exact 800 kHz bit timing
    float div = static_cast<float>(clock_get_hz(clk_sys)) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(s_ledPio, s_ledSm, s_ledOffset, &c);
    pio_sm_set_enabled(s_ledPio, s_ledSm, true);

    setPixel(Color::OFF);
}

void System::pushPixel(uint32_t grbColor) {
    uint8_t g = static_cast<uint8_t>((grbColor >> 16) & 0xFF);
    uint8_t r = static_cast<uint8_t>((grbColor >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>(grbColor & 0xFF);

    // Apply brightness scaling
    g = static_cast<uint8_t>((g * s_brightness) >> 8);
    r = static_cast<uint8_t>((r * s_brightness) >> 8);
    b = static_cast<uint8_t>((b * s_brightness) >> 8);

    uint32_t scaledGRB = (static_cast<uint32_t>(g) << 16) | 
                         (static_cast<uint32_t>(r) << 8)  | 
                          static_cast<uint32_t>(b);

    pio_sm_put_blocking(s_ledPio, s_ledSm, scaledGRB << 8);
}

void System::setPixel(uint32_t grbColor, uint32_t durationMs) {
    feedWatchdog(); // Füttert den Watchdog bei jedem LED-Update
    
    s_currentColor  = grbColor;
    s_blinkInterval = durationMs;
    s_ledTimer      = millis();
    s_blinkState    = true;

    pushPixel(s_currentColor);
}

void System::setBrightness(uint8_t brightness) {
    s_brightness = brightness;
    pushPixel(s_currentColor);
}

void System::update() {
    feedWatchdog();

    // Handling von blinkenden / pulsenden LED-Zuständen
    if (s_blinkInterval > 0) {
        uint32_t now = millis();
        if (now - s_ledTimer >= s_blinkInterval) {
            s_ledTimer = now;
            s_blinkState = !s_blinkState;
            
            pushPixel(s_blinkState ? s_currentColor : Color::OFF);
        }
    }
}

float System::readMcuTemperature() {
    adc_select_input(4);
    uint16_t raw = adc_read();
    constexpr float conversionFactor = 3.3f / (1 << 12);
    float voltage = raw * conversionFactor;
    
    return 27.0f - (voltage - 0.706f) / 0.001721f;
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

// ── HARDWARE PIO ENCODER IMPLEMENTATION ──

HardwarePIOEncoder::HardwarePIOEncoder(uint8_t basePinA, uint8_t btnPin, PIO pio, uint8_t stepDivisor)
    : m_pinA(basePinA),
      m_pinB(basePinA + 1),
      m_btnPin(btnPin),
      m_pio(pio),
      m_sm(0),
      m_stepDivisor(stepDivisor),
      m_lastPosition(0),
      m_lastReadTime(0),
      m_btnPressTime(0),
      m_btnWasPressed(false) {}

void HardwarePIOEncoder::begin() {
    // 1. Konfiguration für externe Hardware-Pull-Ups (Deaktivierung interner Pull-ups/downs)
    pinMode(m_pinA, INPUT);
    pinMode(m_pinB, INPUT);
    pinMode(m_btnPin, INPUT);

    gpio_set_pulls(m_pinA, false, false);
    gpio_set_pulls(m_pinB, false, false);
    gpio_set_pulls(m_btnPin, false, false);

    // 2. PIO Quadratur-Decoder Programm auf pio0 laden
    m_sm = pio_claim_unused_sm(m_pio, true);
    uint offset = pio_add_program(m_pio, &encoder_pio_program);

    pio_gpio_init(m_pio, m_pinA);
    pio_gpio_init(m_pio, m_pinB);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_in_pins(&c, m_pinA); // Liest Phase A (m_pinA) und Phase B (m_pinA + 1)
    sm_config_set_in_shift(&c, false, false, 0);

    sm_config_set_clkdiv(&c, 100.0f);

    pio_sm_init(m_pio, m_sm, offset, &c);
    pio_sm_set_enabled(m_pio, m_sm, true);

    m_lastPosition  = 0;
    m_lastReadTime  = millis();
    m_btnWasPressed = false;
    m_btnPressTime  = 0;
}

int32_t HardwarePIOEncoder::readDelta() {
    uint32_t now = millis();
    uint32_t dt  = now - m_lastReadTime;

    // Aktuelle Position direkt aus dem PIO State Machine Register lesen
    int32_t currentPosition = static_cast<int32_t>(pio_sm_get_blocking(m_pio, m_sm));
    
    int32_t rawDelta = currentPosition - m_lastPosition;
    if (rawDelta == 0) {
        return 0;
    }

    m_lastPosition = currentPosition;
    int32_t steps  = rawDelta / m_stepDivisor;

    if (steps != 0) {
        m_lastReadTime = now;
        
        // Dynamische Beschleunigung (< 30ms pro Raste)
        if (dt < 30 && dt > 0) {
            steps *= 2;
        }
    }

    return steps;
}

HardwarePIOEncoder::ButtonEvent HardwarePIOEncoder::updateButton() {
    // Externe HW-Pull-Ups -> Taster schaltet nach GND (active LOW)
    bool isPressed = (digitalRead(m_btnPin) == LOW);
    uint32_t now   = millis();
    ButtonEvent event = ButtonEvent::NONE;

    if (isPressed && !m_btnWasPressed) {
        m_btnPressTime  = now;
        m_btnWasPressed = true;
    } 
    else if (!isPressed && m_btnWasPressed) {
        uint32_t duration = now - m_btnPressTime;
        m_btnWasPressed = false;

        if (duration >= 500) {
            event = ButtonEvent::LONG_PRESS;
        } else if (duration >= 30) {
            event = ButtonEvent::SHORT_PRESS;
        }
    }

    return event;
}

} // namespace RP2350Uni