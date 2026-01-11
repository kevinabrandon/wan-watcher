// metric_display.cpp
#include "metric_display.h"
#include "wan_metrics.h"
#include "local_pinger.h"
#include "freshness_bar.h"

// 7-segment patterns for letters (active-low segments: 0bPGFEDCBA)
// Segment layout:
//    AAA
//   F   B
//    GGG
//   E   C
//    DDD  P
static const uint8_t SEG_A = 0x01;
static const uint8_t SEG_B = 0x02;
static const uint8_t SEG_C = 0x04;
static const uint8_t SEG_D = 0x08;
static const uint8_t SEG_E = 0x10;
static const uint8_t SEG_F = 0x20;
static const uint8_t SEG_G = 0x40;

static const uint8_t LETTER_L = SEG_D | SEG_E | SEG_F;                      // L
static const uint8_t LETTER_J = SEG_B | SEG_C | SEG_D | SEG_E;              // J
static const uint8_t LETTER_P = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;      // P
static const uint8_t LETTER_d = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G;      // d (lowercase)
static const uint8_t LETTER_U = SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;      // U
static const uint8_t LETTER_DASH = SEG_G;                                    // -

MetricDisplay::MetricDisplay()
    : _wire(nullptr)
    , _i2c_addr(0)
    , _ready(false)
    , _type(DisplayType::PACKET)
    , _wan_id(1)
    , _packet_metric(PacketMetric::LATENCY)
    , _bandwidth_metric(BandwidthMetric::DOWNLOAD)
{}

bool MetricDisplay::begin(uint8_t i2c_addr, TwoWire* wire) {
    _i2c_addr = i2c_addr;
    _wire = wire;
    _ready = _display.begin(i2c_addr, wire);
    if (_ready) {
        _display.clear();
        _display.writeDisplay();
        _display.setBrightness(8);
    }
    return _ready;
}

bool MetricDisplay::isReady() const {
    return _ready;
}

void MetricDisplay::configure(DisplayType type, int wan_id) {
    _type = type;
    _wan_id = wan_id;
}

void MetricDisplay::setBrightness(uint8_t brightness) {
    if (_ready) {
        _display.setBrightness(brightness > 15 ? 15 : brightness);
    }
}

void MetricDisplay::setDisplayOn(bool on) {
    if (!_ready || !_wire) return;
    // HT16K33 display setup register: 0x80 = off, 0x81 = on
    _wire->beginTransmission(_i2c_addr);
    _wire->write(on ? 0x81 : 0x80);
    _wire->endTransmission();
}

void MetricDisplay::setPacketMetric(PacketMetric metric) {
    _packet_metric = metric;
}

void MetricDisplay::setBandwidthMetric(BandwidthMetric metric) {
    _bandwidth_metric = metric;
}

PacketMetric MetricDisplay::currentPacketMetric() const {
    return _packet_metric;
}

BandwidthMetric MetricDisplay::currentBandwidthMetric() const {
    return _bandwidth_metric;
}

DisplayType MetricDisplay::displayType() const {
    return _type;
}

int MetricDisplay::wanId() const {
    return _wan_id;
}

void MetricDisplay::writeLetterDigit(char letter) {
    uint8_t pattern = 0;
    switch (letter) {
        case 'L': pattern = LETTER_L; break;
        case 'J': pattern = LETTER_J; break;
        case 'P': pattern = LETTER_P; break;
        case 'd': pattern = LETTER_d; break;
        case 'U': pattern = LETTER_U; break;
        default:  pattern = LETTER_DASH; break;
    }
    _display.writeDigitRaw(0, pattern);
}

void MetricDisplay::showDashes() {
    _display.writeDigitRaw(0, LETTER_DASH);
    _display.writeDigitRaw(1, LETTER_DASH);
    _display.writeDigitRaw(3, LETTER_DASH);  // position 2 is colon
    _display.writeDigitRaw(4, LETTER_DASH);
}

void MetricDisplay::write3DigitValue(int value) {
    // Right-align value in positions 1, 3, 4 (position 2 is colon)
    // Cap at 999
    if (value > 999) value = 999;
    if (value < 0) value = 0;

    // Always show units digit
    _display.writeDigitNum(4, value % 10);

    // Show tens if >= 10
    if (value >= 10) {
        _display.writeDigitNum(3, (value / 10) % 10);
    }

    // Show hundreds if >= 100
    if (value >= 100) {
        _display.writeDigitNum(1, (value / 100) % 10);
    }
}

// Timeout threshold uses FRESHNESS_RED_BUFFER_END_MS from freshness_bar.h (60s)

void MetricDisplay::render(DisplayMode mode) {
    if (!_ready) return;

    // Get last update timestamp based on data source
    unsigned long last_update_ms;
    if (_wan_id == 0) {
        // Local pinger
        last_update_ms = local_pinger_get().last_update_ms;
    } else {
        // WAN metrics
        last_update_ms = wan_metrics_get(_wan_id).last_update_ms;
    }

    // Show dashes if never updated
    if (last_update_ms == 0) {
        showDashes();
        _display.writeDisplay();
        return;
    }

    // Show dashes if data is stale (no update for 60s - matches freshness bar)
    unsigned long elapsed = millis() - last_update_ms;
    if (elapsed > FRESHNESS_RED_BUFFER_END_MS) {
        showDashes();
        _display.writeDisplay();
        return;
    }

    _display.clear();

    if (_type == DisplayType::PACKET) {
        renderPacketValue(mode);
    } else {
        renderBandwidthValue(mode);
    }

    _display.writeDisplay();
}

void MetricDisplay::renderPacketValue(DisplayMode mode) {
    int value = 0;
    char letter = 'L';

    if (_wan_id == 0) {
        // Local pinger data
        const LocalPingerMetrics& m = local_pinger_get();
        switch (_packet_metric) {
            case PacketMetric::LATENCY:
                value = m.latency_ms;
                letter = 'L';
                break;
            case PacketMetric::JITTER:
                value = m.jitter_ms;
                letter = 'J';
                break;
            case PacketMetric::LOSS:
                value = m.loss_pct;
                letter = 'P';
                break;
        }
    } else {
        // WAN metrics data
        const WanMetrics& m = wan_metrics_get(_wan_id);
        switch (_packet_metric) {
            case PacketMetric::LATENCY:
                value = m.latency_ms;
                letter = 'L';
                break;
            case PacketMetric::JITTER:
                value = m.jitter_ms;
                letter = 'J';
                break;
            case PacketMetric::LOSS:
                value = m.loss_pct;
                letter = 'P';
                break;
        }
    }

    if (mode == DisplayMode::PREFIX_LETTER) {
        // First digit: letter, remaining 3: value
        writeLetterDigit(letter);
        write3DigitValue(value);
    } else {
        // INDICATOR_LED mode: full 4 digits
        if (value > 9999) value = 9999;
        _display.print(value, DEC);
    }
}

void MetricDisplay::renderBandwidthValue(DisplayMode mode) {
    const WanMetrics& m = wan_metrics_get(_wan_id);

    float value = 0.0f;
    char letter = 'd';

    switch (_bandwidth_metric) {
        case BandwidthMetric::DOWNLOAD:
            value = m.down_mbps;
            letter = 'd';
            break;
        case BandwidthMetric::UPLOAD:
            value = m.up_mbps;
            letter = 'U';
            break;
    }

    if (mode == DisplayMode::PREFIX_LETTER) {
        writeLetterDigit(letter);

        // Format bandwidth in 3 digits
        // If >= 100: show as integer (e.g., 150 -> "150")
        // If < 100: show with 1 decimal (e.g., 45.2 -> "452" with decimal point)
        int display_val;
        bool show_decimal = false;

        if (value >= 100.0f) {
            display_val = (int)value;
            if (display_val > 999) display_val = 999;
        } else {
            // Show one decimal place: 45.2 becomes 452
            display_val = (int)(value * 10.0f + 0.5f);
            if (display_val > 999) display_val = 999;
            show_decimal = true;
        }

        // Write digits: position 1 (hundreds), 3 (tens), 4 (units)
        // Decimal point after position 3 when value < 100
        if (display_val >= 100) {
            _display.writeDigitNum(1, (display_val / 100) % 10);
        }
        if (display_val >= 10) {
            _display.writeDigitNum(3, (display_val / 10) % 10, show_decimal);
        } else if (show_decimal) {
            // Value < 10, need leading zero for decimal (e.g., 0.5 -> " 05" with DP)
            _display.writeDigitNum(3, 0, true);
        }
        _display.writeDigitNum(4, display_val % 10);
    } else {
        // INDICATOR_LED mode: full 4 digits with decimal
        // Format: XXX.X (e.g., 123.4)
        int int_val = (int)(value * 10.0f + 0.5f);
        if (int_val > 9999) int_val = 9999;

        _display.writeDigitNum(0, (int_val / 1000) % 10);
        _display.writeDigitNum(1, (int_val / 100) % 10);
        _display.writeDigitNum(3, (int_val / 10) % 10, true);  // decimal point
        _display.writeDigitNum(4, int_val % 10);
    }
}
