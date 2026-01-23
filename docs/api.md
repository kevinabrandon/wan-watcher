## JSON API (ESP32)

The ESP32 exposes a JSON API for metrics and display control.

**Interactive Documentation:** The ESP32 serves an OpenAPI specification with interactive documentation at [`/api-docs.html`](http://wan-watcher.local/api-docs.html).

---

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Get current metrics for all interfaces |
| GET | `/api/display-power` | Get display power state and switch position |
| POST | `/api/display-power` | Set display power state |
| GET | `/api/brightness` | Get brightness level and potentiometer position |
| POST | `/api/brightness` | Set brightness level (0-15) |
| GET | `/api/bw-source` | Get bandwidth display source |
| POST | `/api/bw-source` | Set bandwidth display source |
| POST | `/api/wans` | Update WAN metrics (pfSense daemon only) |

---

### GET /api/status

Returns current metrics for all interfaces. Used by the Web UI for live updates.

**Response format:**
```json
{
  "hostname": "wan-watcher",
  "timestamp": "2025-01-15T10:30:00Z",
  "router_ip": "192.168.1.1",
  "wan1": {
    "state": "up",
    "latency_ms": 6,
    "jitter_ms": 0,
    "loss_pct": 0,
    "down_mbps": 2.0,
    "up_mbps": 3.3,
    "down_1m": 2.1,
    "up_1m": 3.2,
    "down_5m": 2.5,
    "up_5m": 3.0,
    "down_15m": 2.3,
    "up_15m": 3.1,
    "monitor_ip": "8.8.8.8",
    "gateway_ip": "100.64.1.1",
    "local_ip": "100.64.1.5"
  },
  "wan2": { ... },
  "local": {
    "state": "up",
    "latency_ms": 12,
    "jitter_ms": 1,
    "loss_pct": 0,
    "local_ip": "192.168.1.100",
    "monitor_ip": "8.8.8.8"
  },
  "freshness": {
    "green_fill_end": 15,
    "green_buffer_end": 20,
    "yellow_fill_end": 35,
    "yellow_buffer_end": 40,
    "red_fill_end": 55,
    "red_buffer_end": 60,
    "fill_duration": 15,
    "led_count": 24
  }
}
```

### GET /api/display-power

Returns current display/LED power state and physical switch position.

**Response format:**
```json
{
  "on": true,
  "switch_position": true
}
```

- `on`: Current power state (software-controlled)
- `switch_position`: Physical toggle switch position (true = ON)

When `on` differs from `switch_position`, the web UI shows "(overridden)" to indicate the software state differs from the physical switch.

### POST /api/display-power

Set the display/LED power state (overrides physical switch until switch is toggled).

**Payload format:**
```json
{
  "on": true
}
```

### GET /api/brightness

Returns current brightness level and potentiometer position.

**Response format:**
```json
{
  "brightness": 8,
  "pot_level": 10
}
```

- `brightness`: Current display brightness (0-15, software-controlled)
- `pot_level`: Physical potentiometer position (0-15)

When `brightness` differs from `pot_level`, the web UI shows "(overridden)" to indicate the software setting differs from the dial position.

### POST /api/brightness

Set the display brightness (overrides potentiometer until dial is turned).

**Payload format:**
```json
{
  "brightness": 8
}
```

### GET /api/bw-source

Returns the currently selected bandwidth display source.

**Response format:**
```json
{
  "source": "1m"
}
```

- `source`: Can be "15s", "1m", "5m", or "15m".

### POST /api/bw-source

Set the bandwidth display source.

**Payload format:**
```json
{
  "source": "5m"
}
```

- `source`: Can be "15s", "1m", "5m", or "15m".

---

## pfSense Integration

### POST /api/wans

> **Note:** This endpoint is used internally by the pfSense daemon (`wan_watcher_daemon.sh`). It is not intended for general use. The daemon pushes metrics every 15 seconds, so any manual changes will be overwritten.

Batch update endpoint for WAN metrics from pfSense.

**Payload format:**
```json
{
  "router_ip": "192.168.1.1",
  "timestamp": "2025-01-15T10:30:00Z",
  "wan1": {
    "state": "up",
    "loss_pct": 0,
    "latency_ms": 6,
    "jitter_ms": 0,
    "down_mbps": 2.0,
    "up_mbps": 3.3,
    "down_1m": 2.1,
    "up_1m": 3.2,
    "down_5m": 2.5,
    "up_5m": 3.0,
    "down_15m": 2.3,
    "up_15m": 3.1,
    "local_ip": "100.64.1.5",
    "gateway_ip": "100.64.1.1",
    "monitor_ip": "8.8.8.8"
  },
  "wan2": {
    "state": "up",
    "loss_pct": 0,
    "latency_ms": 12,
    "jitter_ms": 1,
    "down_mbps": 50.0,
    "up_mbps": 10.0,
    "down_1m": 50.1,
    "up_1m": 10.2,
    "down_5m": 50.5,
    "up_5m": 10.0,
    "down_15m": 50.3,
    "up_15m": 10.1,
    "local_ip": "192.168.100.2",
    "gateway_ip": "192.168.100.1",
    "monitor_ip": "1.1.1.1"
  }
}
```

**Response format:**
```json
{
  "status": "ok",
  "wan1": {
    "state": "up",
    "loss_pct": 0,
    "latency_ms": 6,
    "jitter_ms": 0,
    "down_mbps": 2.0,
    "up_mbps": 3.3
  },
  "wan2": {
    "state": "up",
    "loss_pct": 0,
    "latency_ms": 12,
    "jitter_ms": 1,
    "down_mbps": 50.0,
    "up_mbps": 10.0
  }
}
```

**Example (for testing):**
```bash
curl -X POST -H "Content-Type: application/json" \
  -d '{"router_ip":"192.168.1.1","timestamp":"2025-01-15T10:30:00Z","wan1":{"state":"up","loss_pct":0,"latency_ms":6,"jitter_ms":0,"down_mbps":2.0,"up_mbps":3.3,"local_ip":"100.64.1.5","gateway_ip":"100.64.1.1","monitor_ip":"8.8.8.8"}}' \
  http://wan-watcher.local/api/wans
```
