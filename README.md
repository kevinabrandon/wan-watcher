# wan-watcher

A physical WAN health indicator for pfSense using an ESP32.

wan-watcher collects real-time WAN metrics from pfSense (latency, loss, jitter, and bandwidth usage) and exposes them to an ESP32, which drives LEDs and 7-segment displays. The result is a small physical panel that shows your internet health at a glance.

---

## Features

* **pfSense metrics collector**

  * Polls dpinger sockets for latency / jitter / loss
  * Auto-maps each dpinger instance to its WAN interface
  * Calculates per-WAN bandwidth usage (Mbps)
  * Writes clean status/metrics files in `/var/run` for consumers
  * Supports multiple dpinger instances (e.g., WAN + WAN2)
  * Tracks per-WAN state and usage separately
  * Regularly posts to ESP32 each WAN's state

* **ESP32 indicator panel**
  * Receives metrics via JSON API (`POST /api/wans` batch endpoint)
  * LED indicators for WAN1, WAN2, and Local state (UP / DEGRADED / DOWN) via MCP23017 I2C expander
  * Heartbeat LED showing how recent the last pfSense update was:
    * `< 15s`: OFF
    * `15–30s`: slow blink
    * `30–45s`: medium blink
    * `> 45s`: fast blink + all WANs forced DOWN
  * Dual 7-segment displays per WAN:
    * **Packet display**: Shows latency (L), jitter (J), or packet loss (P)
    * **Bandwidth display**: Shows download (d) or upload (U) in Mbps
    * Displays show "----" when interface is DOWN or no data received
  * Button controls for display cycling:
    * Short press: advance to next metric
    * Long press: toggle auto-cycle mode
  * Auto-cycle enabled by default (5-second interval)

* **Web UI**
  * Live-updating metrics table (refreshes every 5 seconds without page reload)
  * CSS-based 7-segment display panel mimicking the physical hardware layout
  * Virtual state LEDs (UP/DEGRADED/DOWN) for each interface
  * Freshness indicator bar showing time since last pfSense update:
    * 0–15s: green bar fills (normal)
    * 15–30s: yellow bar fills (late)
    * 30–45s: red bar fills (very late)
    * >45s: full red bar blinking (stale)
  * Click on displays to cycle metrics, auto-cycles every 5 seconds
  * Dynamic favicon color based on Local pinger state (green/yellow/red)
  * Local timezone display for timestamps

* **ESP32 local pinger**

  * _Represents the controller’s independent view of internet reachability and may disagree with pfSense during routing or firewall faults._
  * ICMP pinger to configurable target (default 8.8.8.8)
  * Calculates latency, jitter, loss percentage (dpinger-style)
    * Averaged over a 60 second window with 120 samples (every 500ms)
  * Separate UP/DEGRADED/DOWN LEDs for local path
  * Separate 7-segment display for local packet stats (L/J/P)

## Failure Behavior

- If pfSense stops reporting: all WANs forced DOWN after 45 seconds; 7-segment displays read "----" (local pinger continues updating independently)
- If ESP32 loses Wi-Fi: status LED blinks, last state retained

## Hardware

* 1× Olimex ESP32-POE-ISO
* 1× MCP23017 I2C GPIO expander (address 0x20)
* 5× Adafruit 4-digit 7-segment displays (HT16K33, addresses 0x71-0x75)
* 2× Momentary push buttons (active low, directly to MCP23017 with internal pull-ups)
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
| MCP 3 | MCP23017 | WAN2 UP (green) |
| MCP 4 | MCP23017 | WAN2 DEGRADED (yellow) |
| MCP 5 | MCP23017 | WAN2 DOWN (red) |
| MCP 6 | MCP23017 | Local UP (green) |
| MCP 7 | MCP23017 | Local DEGRADED (yellow) |
| MCP 8 | MCP23017 | Local DOWN (red) |
| MCP 14 | MCP23017 | Packet display button (INPUT_PULLUP) |
| MCP 15 | MCP23017 | Bandwidth display button (INPUT_PULLUP) |
| GPIO 4 | ESP32 | WiFi status LED |
| GPIO 5 | ESP32 | Heartbeat LED |

### Display I2C Addresses

| Address | Display |
|---------|---------|
| 0x71 | WAN1 Packet (L/J/P) |
| 0x72 | WAN1 Bandwidth (d/U) |
| 0x73 | WAN2 Packet (L/J/P) |
| 0x74 | WAN2 Bandwidth (d/U) |
| 0x75 | Local Packet (L/J/P) |

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

The ESP32 uses the hostname `wan-watcher` and exposes itself via mDNS (Bonjour/Avahi):

```
http://wan-watcher.local/
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

The ESP32 exposes a JSON API for metrics:

**Endpoints:**
- `POST /api/wans` - Batch update for all WANs (used by pfSense daemon)
- `GET /api/status` - Get current metrics for all interfaces (used by Web UI)

### POST /api/wans

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
    "local_ip": "192.168.100.2",
    "gateway_ip": "192.168.100.1",
    "monitor_ip": "1.1.1.1"
  }
}
```

**Example:**
```
curl -X POST -H "Content-Type: application/json" \
  -d '{"router_ip":"192.168.1.1","timestamp":"2025-01-15T10:30:00Z","wan1":{"state":"up","loss_pct":0,"latency_ms":6,"jitter_ms":0,"down_mbps":2.0,"up_mbps":3.3,"local_ip":"100.64.1.5","gateway_ip":"100.64.1.1","monitor_ip":"8.8.8.8"}}' \
  http://wan-watcher.local/api/wans
```

### GET /api/status

Returns current metrics for all interfaces. Used by the Web UI for live updates.

**Response format:**
```json
{
  "timestamp": "2025-01-15T10:30:00Z",
  "router_ip": "192.168.1.1",
  "wan1": {
    "state": "up",
    "latency_ms": 6,
    "jitter_ms": 0,
    "loss_pct": 0,
    "down_mbps": 2.0,
    "up_mbps": 3.3,
    "monitor_ip": "8.8.8.8",
    "gateway_ip": "100.64.1.1",
    "local_ip": "100.64.1.5"
  },
  "wan2": { ... },
  "local": {
    "state": "up",
    "latency_ms": 12,
    "jitter_ms": 1,
    "loss_pct": 0
  }
}
```

## Security Notes

- Intended for a trusted VLAN
- No authentication by default
- Do not expose the ESP32 API to untrusted networks

---

## Roadmap

### Implemented

#### pfSense Side

 - Poll dpinger for latency/jitter/loss
 - Auto-detect WAN interfaces
 - Calculate per-WAN bandwidth usage
 - Implement HTTP client to POST metrics to ESP32 (JSON API)
 - Multi-WAN POST support

#### ESP32 Side

 - Wi-Fi + Ethernet support with status LED
 - Static hostname (`wan-watcher`) + mDNS support
 - HTTP server + routes
 - WAN1 and WAN2 LED state machines (UP / DEGRADED / DOWN)
 - Heartbeat LED (off / slow / medium / fast blink)
 - Auto-timeout all WANs to DOWN if no update for 45 seconds
 - MCP23017 GPIO expander for LED control
 - LED abstraction class (supports GPIO and MCP23017 pins)
 - Multi-WAN support (2 WANs with 2 displays each)
 - Display modes (latency / jitter / loss / download / upload)
 - Button input for cycling modes (short press: advance, long press: toggle auto-cycle)
 - Multiple 7-segment displays (packet + bandwidth per WAN)
 - Auto-cycle enabled by default (5-second interval)
 - Displays show "----" when interface is DOWN

#### ESP32 Web UI

 - Live-updating metrics table (every 5 seconds, no page reload)
 - CSS-based 7-segment display panel mimicking hardware layout
 - Virtual state LEDs (UP/DEGRADED/DOWN) for each interface
 - Freshness indicator bar (green/yellow/red based on data age)
 - Click-to-cycle and auto-cycle for display metrics
 - Dynamic favicon color based on Local pinger state
 - Local timezone display for timestamps

#### ESP32 Local Pinger

 - ICMP ping to configurable target (default 8.8.8.8)
 - Calculate latency, jitter, loss percentage (dpinger-style)
 - Separate UP/DEGRADED/DOWN LEDs for local path
 - Separate 7-segment display for local packet stats (L/J/P)
 - Configurable thresholds for degraded/down states

### Planned

#### Display Controls

* [ ] Brightness control (potentiometer or buttons)
* [ ] Display on/off toggle for "dark mode" (guest sleeping)

#### Hardware Visual Freshness Indicator

* [ ] I2C bicolor LED bargraph (HT16K33) for update freshness
  * 24-segment progress bar driven from the shared I²C bus
  * Acts as a visual timer showing time since last pfSense update
* [ ] Time-based fill with color stages (same as Web UI freshness bar)
  * >45s: full red bar blinking (stale / fault)
* [ ] Immediate reset on update receipt
  * Bar clears instantly when a valid update is received
---

## License

MIT

## Contributions

This is a hobby/utility project. Suggestions welcome as it evolves.
