Greenhouse project — SPIFFS web UI and MQTT

This project already includes a static dashboard `web_dashboard.html` in the repository root and a copy in `data/` so it can be uploaded to the board filesystem (SPIFFS/LittleFS).

To upload the UI to the ESP32 SPIFFS and flash the firmware (run these from the project root):

1. Build filesystem image and upload it to the board:

```powershell
pio run -t buildfs
pio run -t uploadfs
```

2. Build and upload firmware:

```powershell
pio run
pio run -t upload
```

After upload open the dashboard served by the board:

- Web UI (served by the device): http://<device-ip>/web_dashboard.html
- Embedded MQTT dashboard fallback: http://<device-ip>/mqtt

MQTT notes:
- Firmware publishes to `greenhouse/<mac>/status` and subscribes to `greenhouse/<mac>/cmd`.
- Example command payload to toggle relay 2:
  `{"cmd":"relay","ch":2,"state":"toggle"}`
- The web UI connects to a broker via WebSockets by default: `wss://broker.hivemq.com:8884/mqtt`. For production use run your own broker with TLS and authentication.

Security:
- Current firmware uses `WiFiClientSecure::setInsecure()` (accepts any cert) for demonstration. Replace with `setCACert()` and provide CA if you require certificate validation.

If you want I can add a GitHub Pages deployment file to host the dashboard remotely instead of on the device.
