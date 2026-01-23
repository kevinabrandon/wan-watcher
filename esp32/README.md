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

When making changes to the ESP32 project, you will use two different PlatformIO commands depending on what you have modified:

1.  **To upload Web UI changes (HTML, CSS, JavaScript):**
    After modifying any files in `data/`, you need to upload the filesystem image to the ESP32.
    ```bash
    pio run -t uploadfs
    ```

2.  **To upload Firmware changes (C++ code):**
    After modifying any C++ source files (e.g., in `src/`), you need to compile and upload the new firmware.
    ```bash
    pio run -e esp32-poe-iso -t upload
    ```

**For a full deployment (both firmware and web UI changes), it's recommended to run `uploadfs` first, then `upload`.**
