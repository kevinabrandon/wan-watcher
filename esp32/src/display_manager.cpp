// display_manager.cpp
#include "display_manager.h"
#include "wan_metrics.h"

DisplayManager::DisplayManager()
    : _active_count(0)
    , _led_latency(nullptr)
    , _led_jitter(nullptr)
    , _led_loss(nullptr)
    , _led_download(nullptr)
    , _led_upload(nullptr)
    , _mcp(nullptr)
    , _last_cycle_ms(0)
    , _current_packet_metric(PacketMetric::LATENCY)
    , _current_bw_metric(BandwidthMetric::DOWNLOAD)
    , _packet_auto_cycle(true)
    , _bw_auto_cycle(true)
{}

int DisplayManager::displayIndex(int wan_id, DisplayType type) const {
    // wan1_packet=0, wan1_bw=1, wan2_packet=2, wan2_bw=3
    int base = (wan_id - 1) * 2;
    return base + (type == DisplayType::BANDWIDTH ? 1 : 0);
}

void DisplayManager::begin(const DisplaySystemConfig& config,
                           Adafruit_MCP23X17* mcp,
                           TwoWire* wire) {
    _config = config;
    _mcp = mcp;
    _last_cycle_ms = millis();
    _packet_auto_cycle = config.auto_cycle_enabled;
    _bw_auto_cycle = config.auto_cycle_enabled;
    _active_count = 0;

    // Initialize displays
    // Address layout: base+0=wan1_packet, base+1=wan1_bw, base+2=wan2_packet, base+3=wan2_bw
    for (int wan = 1; wan <= MAX_WANS; wan++) {
        for (int t = 0; t < 2; t++) {
            DisplayType dtype = (t == 0) ? DisplayType::PACKET : DisplayType::BANDWIDTH;
            int idx = displayIndex(wan, dtype);
            uint8_t addr = config.base_address + idx;

            if (_displays[idx].begin(addr, wire)) {
                _displays[idx].configure(dtype, wan);
                _active_count++;
                Serial.printf("Display %d (WAN%d %s) at 0x%02X: OK\n",
                              idx, wan,
                              (dtype == DisplayType::PACKET ? "packet" : "bandwidth"),
                              addr);
            } else {
                Serial.printf("Display %d (WAN%d %s) at 0x%02X: not found\n",
                              idx, wan,
                              (dtype == DisplayType::PACKET ? "packet" : "bandwidth"),
                              addr);
            }
        }
    }

    // Initialize indicator LEDs only if INDICATOR_LED mode
    if (config.mode == DisplayMode::INDICATOR_LED && mcp != nullptr) {
        _led_latency = new Led(config.led_latency_pin, LedPinType::MCP, mcp);
        _led_jitter = new Led(config.led_jitter_pin, LedPinType::MCP, mcp);
        _led_loss = new Led(config.led_loss_pin, LedPinType::MCP, mcp);
        _led_download = new Led(config.led_download_pin, LedPinType::MCP, mcp);
        _led_upload = new Led(config.led_upload_pin, LedPinType::MCP, mcp);

        _led_latency->begin();
        _led_jitter->begin();
        _led_loss->begin();
        _led_download->begin();
        _led_upload->begin();

        Serial.println("Indicator LEDs initialized");
    }

    // Initial sync and render
    syncAllDisplayMetrics();
    updateIndicatorLeds();
    renderAllDisplays();

    Serial.printf("DisplayManager: %d display(s) active, mode=%s, cycle=%lums\n",
                  _active_count,
                  config.mode == DisplayMode::PREFIX_LETTER ? "PREFIX_LETTER" : "INDICATOR_LED",
                  config.cycle_interval_ms);
}

void DisplayManager::update() {
    unsigned long now = millis();

    // Use shared timer to keep displays in sync
    if (now - _last_cycle_ms >= _config.cycle_interval_ms) {
        _last_cycle_ms = now;

        // Cycle whichever displays have auto-cycle enabled
        if (_packet_auto_cycle) {
            cyclePacketMetric();
        }
        if (_bw_auto_cycle) {
            cycleBandwidthMetric();
        }

        if (_packet_auto_cycle || _bw_auto_cycle) {
            syncAllDisplayMetrics();
            updateIndicatorLeds();
        }
    }

    // Always render (values may have changed even if metric didn't)
    renderAllDisplays();
}

void DisplayManager::cyclePacketMetric() {
    uint8_t next = (static_cast<uint8_t>(_current_packet_metric) + 1) % PACKET_METRIC_COUNT;
    _current_packet_metric = static_cast<PacketMetric>(next);
}

void DisplayManager::cycleBandwidthMetric() {
    uint8_t next = (static_cast<uint8_t>(_current_bw_metric) + 1) % BANDWIDTH_METRIC_COUNT;
    _current_bw_metric = static_cast<BandwidthMetric>(next);
}

void DisplayManager::syncAllDisplayMetrics() {
    // Sync all packet displays to same metric
    // Sync all bandwidth displays to same metric
    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (_displays[i].isReady()) {
            if (_displays[i].displayType() == DisplayType::PACKET) {
                _displays[i].setPacketMetric(_current_packet_metric);
            } else {
                _displays[i].setBandwidthMetric(_current_bw_metric);
            }
        }
    }
}

void DisplayManager::updateIndicatorLeds() {
    if (_config.mode != DisplayMode::INDICATOR_LED) return;

    // Packet indicators: only one lit
    if (_led_latency) _led_latency->set(_current_packet_metric == PacketMetric::LATENCY);
    if (_led_jitter) _led_jitter->set(_current_packet_metric == PacketMetric::JITTER);
    if (_led_loss) _led_loss->set(_current_packet_metric == PacketMetric::LOSS);

    // Bandwidth indicators: only one lit
    if (_led_download) _led_download->set(_current_bw_metric == BandwidthMetric::DOWNLOAD);
    if (_led_upload) _led_upload->set(_current_bw_metric == BandwidthMetric::UPLOAD);
}

void DisplayManager::renderAllDisplays() {
    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (_displays[i].isReady()) {
            _displays[i].render(_config.mode);
        }
    }
}

void DisplayManager::advancePacketMetric() {
    _last_cycle_ms = millis();  // Reset shared cycle timer
    cyclePacketMetric();
    syncAllDisplayMetrics();
    updateIndicatorLeds();
    renderAllDisplays();
    Serial.println("Packet metric advanced");
}

void DisplayManager::advanceBandwidthMetric() {
    _last_cycle_ms = millis();  // Reset shared cycle timer
    cycleBandwidthMetric();
    syncAllDisplayMetrics();
    updateIndicatorLeds();
    renderAllDisplays();
    Serial.println("Bandwidth metric advanced");
}

void DisplayManager::togglePacketAutoCycle() {
    _packet_auto_cycle = !_packet_auto_cycle;
    _last_cycle_ms = millis();  // Reset timer to keep displays in sync
    Serial.printf("Packet auto-cycle: %s\n", _packet_auto_cycle ? "ON" : "OFF");
}

void DisplayManager::toggleBandwidthAutoCycle() {
    _bw_auto_cycle = !_bw_auto_cycle;
    _last_cycle_ms = millis();  // Reset timer to keep displays in sync
    Serial.printf("Bandwidth auto-cycle: %s\n", _bw_auto_cycle ? "ON" : "OFF");
}

DisplayMode DisplayManager::displayMode() const {
    return _config.mode;
}

void DisplayManager::setDisplayMode(DisplayMode mode) {
    _config.mode = mode;
}

bool DisplayManager::isPacketAutoCycleEnabled() const {
    return _packet_auto_cycle;
}

bool DisplayManager::isBandwidthAutoCycleEnabled() const {
    return _bw_auto_cycle;
}

void DisplayManager::setPacketAutoCycleEnabled(bool enabled) {
    _packet_auto_cycle = enabled;
}

void DisplayManager::setBandwidthAutoCycleEnabled(bool enabled) {
    _bw_auto_cycle = enabled;
}

PacketMetric DisplayManager::currentPacketMetric() const {
    return _current_packet_metric;
}

BandwidthMetric DisplayManager::currentBandwidthMetric() const {
    return _current_bw_metric;
}

uint8_t DisplayManager::activeDisplayCount() const {
    return _active_count;
}

bool DisplayManager::isDisplayReady(int wan_id, DisplayType type) const {
    int idx = displayIndex(wan_id, type);
    return (idx >= 0 && idx < MAX_DISPLAYS) && _displays[idx].isReady();
}
