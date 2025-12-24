# wan-watcher

A physical WAN health indicator for pfSense using an ESP32.

wan-watcher collects real-time WAN metrics from pfSense (latency, loss, jitter, and bandwidth usage) and exposes them to an ESP32, which drives LEDs and (eventually) a 7-segment display. The result is a small physical panel that shows your internet health at a glance.

---

## Features (current & planned)

* **pfSense metrics collector**

  * Polls dpinger sockets for latency / jitter / loss
  * Auto-maps each dpinger instance to its WAN interface
  * Calculates per-WAN bandwidth usage (Mbps)
  * Writes clean status/metrics files in `/var/run` for consumers
  * Supports multiple dpinger instances (e.g., WAN + WAN2)
  * Tracks per-WAN state and usage separately
  * Regularly posts to ESP32 each WAN's state

* **ESP32 indicator panel**
  * Receives metrics via JSON API (`POST /api/wan1`, `/api/wan2`)
  * Web UI with auto-refresh, metrics table, and LED mapping
  * LED indicators for WAN state (UP / DEGRADED / DOWN) via MCP23017 I2C expander
  * Heartbeat LED showing how recent the last pfSense update was:
    * `< 45s`: OFF
    * `45–90s`: slow blink
    * `90–180s`: fast blink
    * `>= 3m`: solid ON + WAN forced DOWN
  * 7-segment display showing seconds since last update

## Hardware

* 1× Olimex ESP32-POE-ISO
* 1× MCP23017 I2C GPIO expander (address 0x20)
* 1× Adafruit 4-digit 7-segment display (HT16K33, address 0x71)
* Panel-mount LEDs for WAN status indicators

### I2C Wiring (Stemma QT / Qwiic)

| Signal | GPIO | Wire Color |
|--------|------|------------|
| SDA    | 13   | Blue       |
| SCL    | 16   | Yellow     |

### Pin Mapping

| Pin | Type | Function |
|-----|------|----------|
| MCP 0 | MCP23017 | WAN1 UP (green) |
| MCP 1 | MCP23017 | WAN1 DEGRADED (yellow) |
| MCP 2 | MCP23017 | WAN1 DOWN (red) |
| GPIO 4 | ESP32 | WiFi status LED |
| GPIO 5 | ESP32 | Heartbeat LED |

## Repository Structure

```
wan-watcher/
  pf/            # pfSense scripts (cron polling, dpinger, usage)
  esp32/         # ESP32 firmware (LEDs, 7-seg, API endpoints)
  README.md
  .gitignore
```

---

## ESP32 Wi-Fi Configuration

Inside `esp32/src/`, you will find:

```
wifi_config.h.example
```

Copy it to `wifi_config.h` and edit your SSID/password:

```
cp esp32/src/wifi_config.h.example esp32/src/wifi_config.h
```

`wifi_config.h` is intentionally **ignored by git** and should never be committed.

Each ESP32 assigns itself a unique hostname based on its MAC address:

```
wan-watcher-xxxxxx
```

The device also exposes itself via mDNS (Bonjour/Avahi):

```
http://wan-watcher-xxxxxx.local/
```

Your OS must support mDNS for this to work (macOS: built-in, Linux: Avahi, Windows: Bonjour).

---

## Installation (pfSense)

1. Copy the daemon to `/usr/local/bin`:
```
cp pf/wan_watcher_daemon.sh /usr/local/bin/
chmod +x /usr/local/bin/wan_watcher_daemon.sh
```

2. Edit the `ESP32_HOST` variable in the script to match your ESP32's IP address.

3. Run the daemon (default interval is 15 seconds):
```
/usr/local/bin/wan_watcher_daemon.sh &
```

Or with a custom interval:
```
/usr/local/bin/wan_watcher_daemon.sh 30 &
```

---

## JSON API (ESP32)

The ESP32 exposes a JSON API for receiving WAN metrics:

**Endpoints:**
- `POST /api/wan1` - Update WAN1 metrics
- `POST /api/wan2` - Update WAN2 metrics
- `POST /api/wans` - Batch update (accepts `{"wan1":{...}, "wan2":{...}}`)

**Payload format:**
```json
{
  "state": "up",
  "loss_pct": 0,
  "latency_ms": 6,
  "jitter_ms": 0,
  "down_mbps": 2.0,
  "up_mbps": 3.3
}
```

**Example:**
```
curl -X POST -H "Content-Type: application/json" \
  -d '{"state":"up","loss_pct":0,"latency_ms":6,"jitter_ms":0,"down_mbps":2.0,"up_mbps":3.3}' \
  http://wan-watcher-xxxxxx.local/api/wan1
```

---

## Roadmap

### pfSense Side

* [x] Poll dpinger for latency/jitter/loss
* [x] Auto-detect WAN interfaces
* [x] Calculate per-WAN bandwidth usage
* [x] Implement HTTP client to POST metrics to ESP32 (JSON API)
* [x] Multi-WAN POST support

### ESP32 Side

* [x] Basic PlatformIO firmware project scaffolding
* [x] Wi-Fi connect loop with status LED
* [x] Dynamic hostname + mDNS support
* [x] HTTP server + routes
* [x] WAN1 LED state machine (UP / DEGRADED / DOWN)
* [x] Heartbeat LED (off / slow blink / fast blink / solid)
* [x] Auto-timeout WAN1 to DOWN if no update for 3 minutes
* [x] Web UI with color indicators and curl examples
* [x] MCP23017 GPIO expander for LED control
* [x] Led abstraction class (supports GPIO and MCP pins)
* [x] 7-segment display showing seconds since last update
* [ ] Multi-WAN support
* [ ] Display modes (latency / download / upload / loss)
* [ ] Button input for cycling modes
* [ ] Second 7-segment display for additional metrics

---

## License

MIT

## Contributions

This is a hobby/utility project. Suggestions welcome as it evolves.
