## Hardware

* 1× Olimex ESP32-POE-ISO
* 1× MCP23017 I2C GPIO expander (address 0x20)
* 5× Adafruit 4-digit 7-segment displays (HT16K33, addresses 0x71-0x75)
* 1× Adafruit Bicolor 24-Bar Bargraph w/I2C Backpack (HT16K33, address 0x70)
* 2× Momentary push buttons (active low, directly to MCP23017 with internal pull-ups)
* 3× Bicolor (green/red) LEDs for WAN status indicators (WAN1, WAN2, Local)

### I2C Wiring (Stemma QT / Qwiic)

| Signal | GPIO | Wire Color |
|--------|------|------------|
| SDA    | 13   | Blue       |
| SCL    | 16   | Yellow     |

### Pin Mapping

| Pin | Type | Function |
|-----|------|----------|
| MCP 0 | MCP23017 | WAN1 Green LED |
| MCP 1 | MCP23017 | WAN1 Red LED |
| MCP 2 | MCP23017 | WAN2 Green LED |
| MCP 3 | MCP23017 | WAN2 Red LED |
| MCP 4 | MCP23017 | Local Green LED |
| MCP 5 | MCP23017 | Local Red LED |
| MCP 7 | MCP23017 | Ethernet status LED |
| MCP 13 | MCP23017 | Power switch (INPUT_PULLUP, active low) |
| MCP 14 | MCP23017 | Packet display button (INPUT_PULLUP) |
| MCP 15 | MCP23017 | Bandwidth display button (INPUT_PULLUP) |
| GPIO 14 | ESP32 | Status LED PWM brightness (transistor base) |
| GPIO 36 | ESP32 | Brightness potentiometer (ADC1, analog input) |

**Bicolor LED states:** Green = UP, Both on (yellow/amber) = DEGRADED, Red = DOWN

### Display I2C Addresses

| Address | Display |
|---------|---------|
| 0x70 | Freshness Bar (24-segment bicolor) |
| 0x71 | WAN1 Packet (L/J/P) |
| 0x72 | WAN1 Bandwidth (d/U) |
| 0x73 | WAN2 Packet (L/J/P) |
| 0x74 | WAN2 Bandwidth (d/U) |
| 0x75 | Local Packet (L/J/P) |

---

## Circuit Diagram

This diagram provides a comprehensive overview of the electrical connections and components within the WAN Watcher system.

![WAN Watcher Circuit Diagram](./diagrams/wan-watcher-circuit.drawio.svg)

*You can find the diagrams.net (draw.io) source file for this diagram at `./diagrams/wan-watcher-circuit.drawio`.*
