## Technical Notes

### Failure Behavior

- If pfSense stops reporting: after 60 seconds, all WAN LEDs blink red, 7-segment displays read "----", freshness bar blinks red (local pinger continues updating independently)
- If ESP32 loses Ethernet: status LED blinks, last state retained

### Security Notes

- Intended for a trusted VLAN
- No authentication by default
- Do not expose the ESP32 API to untrusted networks

### External Dependencies

The web UI loads two JavaScript libraries from CDNs:

| Library | Purpose | CDN |
|---------|---------|-----|
| [marked.js](https://marked.js.org/) v15.0.12 | Markdown rendering for docs | jsdelivr |
| [RapiDoc](https://rapidocweb.com/) v9.3.8 | OpenAPI documentation | unpkg |

Both are loaded with [Subresource Integrity (SRI)](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity) hashes to prevent tampering. The main dashboard has no external dependencies and works fully offline.

### Releasing a New Version

1. Create a release branch
2. Update the version in `esp32/data/openapi.yaml`
3. Update `package.json` if dependencies changed
4. Commit and push, create PR
5. Merge PR to main
6. Tag and push: `git tag v1.x.x && git push origin v1.x.x`
