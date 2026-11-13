## ESP32 Firmware Development/Deployment

The ESP32 firmware includes a web interface. For better development experience, the HTML, CSS, and JavaScript for the web UI are located in the `data/` directory.

### Web UI Files

| File | Description |
|------|-------------|
| `index.html` | Main dashboard page |
| `styles.css` | Stylesheet (7-segment display, dark theme) |
| `script.js` | Frontend JavaScript (polling, display updates) |
| `docs.html` | Documentation viewer (renders markdown) |
| `api-docs.html` | Interactive API documentation (RapiDoc) |
| `openapi.yaml` | OpenAPI 3.0 specification |

You can directly edit these files using your preferred web development tools.

**Note:** The `docs/` subdirectory is generated at build time by copying markdown files from the repository root. See `extra_scripts/update_version.py`.

### Deployment Steps

Two PlatformIO environments build the same firmware and differ only in how they upload:

| Environment | Upload path | Use for |
|-------------|-------------|---------|
| `esp32-poe-iso` | USB serial | First flash, or recovery if OTA is unavailable |
| `esp32-poe-iso-ota` | Network (`wan-watcher.local`, port 3232) | Everyday updates once OTA firmware is installed |

Both need the OTA password in the environment before building or uploading. It is compiled into the firmware and passed to `espota` for uploads; if it is empty the firmware boots with OTA disabled.

```bash
export WW_OTA_PASSWORD='choose-a-password'   # same value every time you build
```

When making changes to the ESP32 project, you will use two different PlatformIO commands depending on what you have modified:

1.  **To upload Web UI changes (HTML, CSS, JavaScript):**
    After modifying any files in `data/`, you need to upload the filesystem image to the ESP32.
    ```bash
    pio run -e esp32-poe-iso-ota -t uploadfs
    ```

2.  **To upload Firmware changes (C++ code):**
    After modifying any C++ source files (e.g., in `src/`), you need to compile and upload the new firmware.
    ```bash
    pio run -e esp32-poe-iso-ota -t upload
    ```

Use `-e esp32-poe-iso` instead for a USB upload (add `--upload-port /dev/cu.usbserial-XXXX` if PlatformIO does not find the board).

**For a full deployment (both firmware and web UI changes), it's recommended to run `uploadfs` first, then `upload`.** The device reboots after each successful OTA upload; wait for `wan-watcher.local` to answer again before the second one.

Notes:

* The Olimex ESP32-POE-ISO is galvanically isolated, so it can stay on PoE while a USB cable is connected. Do not assume that for the non-ISO ESP32-POE.
* `pio device monitor` only works over USB; OTA progress is printed to the serial console if one is attached.
* A failed filesystem OTA leaves LittleFS unmounted until reboot; the web UI returns errors until then. Retry the upload or power-cycle the board.
