# wan-watcher

A physical WAN health indicator for pfSense using an ESP32.

wan-watcher collects real-time WAN metrics from pfSense (latency, loss, jitter, and bandwidth usage) and exposes them to an ESP32, which drives LEDs and 7-segment displays. The result is a small physical panel that shows your internet health at a glance.

## What it looks like

![wan-watcher panel](./images/wan-watcher.png)

---

## Features

* **pfSense metrics collector**
  * Polls dpinger sockets for latency / jitter / loss
  * Auto-maps each dpinger instance to its WAN interface
  * Calculates per-WAN bandwidth usage (Mbps), including 1m, 5m, and 15m Exponentially Weighted Moving Averages (EWMA)
  * Writes clean status/metrics files in `/var/run` for consumers
  * Supports multiple dpinger instances (e.g., WAN + WAN2)
  * Tracks per-WAN state and usage separately
  * Regularly posts to ESP32 each WAN's state

* **ESP32 indicator panel**
  * Ethernet connectivity via Olimex ESP32-POE-ISO with mDNS (`wan-watcher.local`)
  * Receives metrics via JSON API (`POST /api/wans` batch endpoint)
  * Bicolor LED indicators for WAN1, WAN2, and Local state (green=UP, yellow=DEGRADED, red=DOWN) via MCP23017 I2C expander
  * 24-segment bicolor LED bargraph showing data freshness
  * Dual 7-segment displays per WAN for packet and bandwidth metrics
  * Button controls for display cycling
  * Physical power switch
  * Brightness potentiometer

* **Web UI**
  * Live-updating metrics table
  * Interface Status table
  * Selectable bandwidth display source
  * CSS-based 7-segment display panel mimicking the physical hardware
  * Virtual LEDs and freshness bar
  * Dynamic favicon color based on Local pinger state
  * Display power toggle and brightness slider
  * On-device documentation viewer
  * Interactive API documentation (OpenAPI/RapiDoc)

* **ESP32 local pinger**
  * Independent ICMP pinger to a configurable target
  * Calculates latency, jitter, and loss percentage
  * Bicolor LED for local path
  * Separate 7-segment display for local packet stats

---

## Documentation

* **[Hardware and Wiring](./docs/hardware.md)**: Details on the components, pin mappings, and circuit diagram.
* **[pfSense Installation](./docs/pfsense-install.md)**: How to install the daemon on pfSense.
* **[JSON API Reference](./docs/api.md)**: API endpoints and payload formats.
* **[Technical Notes](./docs/technical-notes.md)**: Failure behavior, security, and external dependencies.

---

## Repository Structure

```
wan-watcher/
  pf/            # pfSense scripts (cron polling, dpinger, usage)
  esp32/         # ESP32 firmware (LEDs, 7-seg, API endpoints)
    data/        # Web UI files (HTML, CSS, JS)
  docs/          # Contains various markdown documentation
    diagrams/    # Circuit diagrams
  images/        # Screenshots and photos
  README.md      # This readme here
  package.json   # CDN dependency tracking for Dependabot alerts
```

---

## License

MIT

## Contributions

This is a hobby/utility project. Suggestions welcome as it evolves.
