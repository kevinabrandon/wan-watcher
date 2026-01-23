## Troubleshooting

### Failure Behavior

- If pfSense stops reporting: after 60 seconds, all WAN LEDs blink red, 7-segment displays read "----", freshness bar blinks red (local pinger continues updating independently)
- If ESP32 loses Ethernet: status LED blinks, last state retained

### Security Notes

- Intended for a trusted VLAN
- No authentication by default
- Do not expose the ESP32 API to untrusted networks
