// local_pinger.cpp
// ESP32-based ICMP pinger implementation using ESP-IDF ping API

#include "local_pinger.h"
#include <WiFi.h>
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <math.h>

// Maximum samples in rolling window (60s / 500ms = 120)
static const int MAX_SAMPLES = 120;

// Ping result entry
struct PingEntry {
    unsigned long send_time_ms;   // When ping was sent
    uint32_t latency_ms;          // Round-trip time in milliseconds (0 if lost)
    bool received;                // True if reply received
    bool counted;                 // True if already counted in stats
};

// Module state
static LocalPingerMetrics g_metrics;
static PingEntry g_samples[MAX_SAMPLES];
static int g_sample_index = 0;
static char g_target[64] = "8.8.8.8";
static esp_ping_handle_t g_ping_handle = nullptr;
static unsigned long g_last_stats_ms = 0;
static bool g_initialized = false;

// Current ping in flight
static unsigned long g_current_ping_send_ms = 0;
static bool g_ping_in_flight = false;

// Forward declarations
static void ping_on_success(esp_ping_handle_t hdl, void* args);
static void ping_on_timeout(esp_ping_handle_t hdl, void* args);
static void ping_on_end(esp_ping_handle_t hdl, void* args);
static void calculate_stats();
static WanState determine_state(uint16_t latency_ms, uint8_t loss_pct);
static void start_ping_session();
static void stop_ping_session();

void local_pinger_init() {
    // Initialize metrics
    g_metrics.state = WanState::DOWN;
    g_metrics.latency_ms = 0;
    g_metrics.jitter_ms = 0;
    g_metrics.loss_pct = 100;
    g_metrics.sample_count = 0;
    g_metrics.window_secs = 0;
    g_metrics.last_update_ms = 0;

    // Clear sample buffer
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_samples[i].send_time_ms = 0;
        g_samples[i].latency_ms = 0;
        g_samples[i].received = false;
        g_samples[i].counted = false;
    }

    g_sample_index = 0;
    g_last_stats_ms = millis();
    g_initialized = true;

    Serial.printf("Local pinger initialized, target: %s\n", g_target);
}

void local_pinger_update() {
    if (!g_initialized) return;
    // Note: When using Ethernet, WiFi.status() won't be WL_CONNECTED
    // The ping API works with any network interface via LwIP

    unsigned long now = millis();

    // Start ping session if not running
    if (g_ping_handle == nullptr) {
        start_ping_session();
    }

    // Recalculate stats periodically
    if (now - g_last_stats_ms >= STATS_UPDATE_MS) {
        g_last_stats_ms = now;
        calculate_stats();
    }
}

const LocalPingerMetrics& local_pinger_get() {
    return g_metrics;
}

void local_pinger_set_target(const char* target) {
    strncpy(g_target, target, sizeof(g_target) - 1);
    g_target[sizeof(g_target) - 1] = '\0';

    // Restart ping session with new target
    if (g_ping_handle != nullptr) {
        stop_ping_session();
        start_ping_session();
    }

    Serial.printf("Local pinger target changed to: %s\n", g_target);
}

const char* local_pinger_get_target() {
    return g_target;
}

// Callback when ping reply received
static void ping_on_success(esp_ping_handle_t hdl, void* args) {
    uint32_t elapsed_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time_ms, sizeof(elapsed_time_ms));

    // Record in sample buffer
    int idx = g_sample_index;
    g_samples[idx].send_time_ms = millis() - elapsed_time_ms;
    g_samples[idx].latency_ms = elapsed_time_ms;
    g_samples[idx].received = true;
    g_samples[idx].counted = false;

    g_sample_index = (g_sample_index + 1) % MAX_SAMPLES;
}

// Callback when ping times out
static void ping_on_timeout(esp_ping_handle_t hdl, void* args) {
    // Record timeout in sample buffer
    int idx = g_sample_index;
    g_samples[idx].send_time_ms = millis();
    g_samples[idx].latency_ms = 0;
    g_samples[idx].received = false;
    g_samples[idx].counted = false;

    g_sample_index = (g_sample_index + 1) % MAX_SAMPLES;
}

// Callback when ping session ends (we restart it)
static void ping_on_end(esp_ping_handle_t hdl, void* args) {
    // Session ended, will be restarted on next update
    g_ping_handle = nullptr;
}

static void start_ping_session() {
    // Resolve target address
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo* res = nullptr;

    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));

    if (getaddrinfo(g_target, nullptr, &hint, &res) != 0) {
        Serial.printf("Local pinger: failed to resolve %s\n", g_target);
        return;
    }

    if (res->ai_family == AF_INET) {
        struct in_addr addr4 = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
        inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    } else {
        Serial.println("Local pinger: IPv6 not supported");
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);

    // Configure ping session
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 0;  // Infinite pings
    ping_config.interval_ms = PING_INTERVAL_MS;
    ping_config.timeout_ms = LOSS_TIMEOUT_MS;
    ping_config.data_size = 32;
    ping_config.tos = 0;

    // Set up callbacks
    esp_ping_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.on_ping_success = ping_on_success;
    cbs.on_ping_timeout = ping_on_timeout;
    cbs.on_ping_end = ping_on_end;
    cbs.cb_args = nullptr;

    // Create and start session
    esp_err_t err = esp_ping_new_session(&ping_config, &cbs, &g_ping_handle);
    if (err != ESP_OK) {
        Serial.printf("Local pinger: failed to create session: %d\n", err);
        g_ping_handle = nullptr;
        return;
    }

    err = esp_ping_start(g_ping_handle);
    if (err != ESP_OK) {
        Serial.printf("Local pinger: failed to start: %d\n", err);
        esp_ping_delete_session(g_ping_handle);
        g_ping_handle = nullptr;
        return;
    }

    Serial.printf("Local pinger: started pinging %s\n", g_target);
}

static void stop_ping_session() {
    if (g_ping_handle != nullptr) {
        esp_ping_stop(g_ping_handle);
        esp_ping_delete_session(g_ping_handle);
        g_ping_handle = nullptr;
    }
}

static void calculate_stats() {
    unsigned long now = millis();
    unsigned long window_start = (now > SAMPLE_WINDOW_MS) ? (now - SAMPLE_WINDOW_MS) : 0;

    uint32_t received_count = 0;
    uint32_t lost_count = 0;
    uint64_t sum_latency_ms = 0;
    uint64_t sum_latency_sq = 0;
    unsigned long oldest_sample_ms = now;
    unsigned long newest_sample_ms = 0;

    // Scan all samples in the window
    for (int i = 0; i < MAX_SAMPLES; i++) {
        PingEntry& entry = g_samples[i];

        // Skip entries outside the window or not yet used
        if (entry.send_time_ms == 0) continue;
        if (entry.send_time_ms < window_start) continue;

        // Track time span of samples
        if (entry.send_time_ms < oldest_sample_ms) {
            oldest_sample_ms = entry.send_time_ms;
        }
        if (entry.send_time_ms > newest_sample_ms) {
            newest_sample_ms = entry.send_time_ms;
        }

        if (entry.received) {
            received_count++;
            sum_latency_ms += entry.latency_ms;
            sum_latency_sq += (uint64_t)entry.latency_ms * entry.latency_ms;
        } else {
            // Only count as lost if enough time has passed
            if (now - entry.send_time_ms >= LOSS_TIMEOUT_MS) {
                lost_count++;
            }
        }
    }

    // Calculate average latency (already in ms)
    uint16_t avg_latency_ms = 0;
    if (received_count > 0) {
        avg_latency_ms = (uint16_t)(sum_latency_ms / received_count);
    }

    // Calculate standard deviation (jitter) in ms
    uint16_t jitter_ms = 0;
    if (received_count > 1) {
        uint64_t mean = sum_latency_ms / received_count;
        uint64_t mean_sq = sum_latency_sq / received_count;
        uint64_t variance = mean_sq - (mean * mean);
        jitter_ms = (uint16_t)sqrt((double)variance);
    }

    // Calculate loss percentage
    uint32_t total = received_count + lost_count;

    // Don't update metrics until we have at least one sample
    if (total == 0) {
        return;
    }

    uint8_t loss_pct = (uint8_t)((lost_count * 100) / total);

    // Calculate actual window span
    uint16_t window_secs = 0;
    if (newest_sample_ms > oldest_sample_ms) {
        window_secs = (uint16_t)((newest_sample_ms - oldest_sample_ms) / 1000);
    }

    // Update metrics
    g_metrics.latency_ms = avg_latency_ms;
    g_metrics.jitter_ms = jitter_ms;
    g_metrics.loss_pct = loss_pct;
    g_metrics.sample_count = (uint16_t)total;
    g_metrics.window_secs = window_secs;
    g_metrics.state = determine_state(avg_latency_ms, loss_pct);
    g_metrics.last_update_ms = now;
}

static WanState determine_state(uint16_t latency_ms, uint8_t loss_pct) {
    // DOWN: loss > 50%
    if (loss_pct > LOSS_DOWN_PCT) {
        return WanState::DOWN;
    }

    // DEGRADED: loss > 5% OR latency > 200ms
    if (loss_pct > LOSS_DEGRADED_PCT || latency_ms > LATENCY_DEGRADED_MS) {
        return WanState::DEGRADED;
    }

    // UP: everything looks good
    return WanState::UP;
}
