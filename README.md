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
  * Receives metrics via JSON API (`POST /api/wan1`, `/api/wan2`)
  * Web UI with auto-refresh and metrics table
  * LED indicators for WAN state (UP / DEGRADED / DOWN) via MCP23017 I2C expander
  * Heartbeat LED showing how recent the last pfSense update was:
    * `< 45s`: OFF
    * `45–90s`: slow blink
    * `90–180s`: fast blink
    * `>= 3m`: solid ON + WAN forced DOWN
  * Dual 7-segment displays per WAN:
    * **Packet display**: Shows latency (L), jitter (J), or packet loss (P)
    * **Bandwidth display**: Shows download (d) or upload (U) in Mbps
  * Button controls for display cycling:
    * Short press: advance to next metric
    * Long press: toggle auto-cycle mode (5-second interval)

* **ESP32 local pinger**

The local pinger represents the controller’s independent view of internet reachability
and may disagree with pfSense during routing or firewall faults.

  * ICMP pingger to configurable target (default 8.8.8.8)
  * Calculates latency, jitter, loss percentage (dpinger-style)
    * Averaged over a 60 second window with 120 samples (every 500ms)
  * Separate UP/DEGRADED/DOWN LEDs for local path
  * Separate 7-segment display for local packet stats (L/J/P)

## Failure Behavior

- If pfSense stops reporting: WAN forced DOWN after 3 minutes and 7-Segment displays read "----"
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
| MCP 5 | MCP23017 | Local UP (green) |
| MCP 6 | MCP23017 | Local DEGRADED (yellow) |
| MCP 7 | MCP23017 | Local DOWN (red) |
| MCP 8 | MCP23017 | WAN2 UP (green) |
| MCP 9 | MCP23017 | WAN2 DEGRADED (yellow) |
| MCP 10 | MCP23017 | WAN2 DOWN (red) |
| MCP 13 | MCP23017 | Packet display button (INPUT_PULLUP) |
| MCP 14 | MCP23017 | Bandwidth display button (INPUT_PULLUP) |
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
 - Dynamic hostname + mDNS support
 - HTTP server + routes
 - WAN1 LED state machine (UP / DEGRADED / DOWN)
 - Heartbeat LED (off / slow blink / fast blink / solid)
 - Auto-timeout WAN1 to DOWN if no update for 3 minutes
 - Web UI with color indicators
 - MCP23017 GPIO expander for LED control
 - Led abstraction class (supports GPIO and MCP23017 pins)
 - 7-segment display showing seconds since last update
 - Multi-WAN support (up to 2 WANs with 2 displays each)
 - Display modes (latency / jitter / loss / download / upload)
 - Button input for cycling modes (short press: advance, long press: toggle auto-cycle)
 - Multiple 7-segment displays (packet + bandwidth per WAN)

#### ESP32 Local Pinger

 - ICMP ping to configurable target (default 8.8.8.8)
 - Calculate latency, jitter, loss percentage (dpinger-style)
 - Separate UP/DEGRADED/DOWN LEDs for local path
 - Separate 7-segment display for local packet stats (L/J/P)
 - Configurable thresholds for degraded/down states

### Planned

#### Web UI Enhancements

* [ ] Live-updating "time since last update" (no page reload)
* [ ] Dynamic metrics refresh via JavaScript

#### Display Controls

* [ ] Brightness control (potentiometer or buttons)
* [ ] Display on/off toggle for "dark mode" (guest sleeping)

---

## License

MIT

## Contributions

This is a hobby/utility project. Suggestions welcome as it evolves.
