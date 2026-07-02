# XGIMI ESP32 BLE Wake

Small ESP32 firmware that wakes an XGIMI projector by sending the BLE advertisement captured from the original remote.

It provides:

- a tiny web UI with an **Einschalten** button
- `POST /poweron` and `GET /poweron` endpoints for Home Assistant
- `GET /status` JSON status output
- mDNS at `http://xgimi-ble-wake.local/` when your network supports it

## Hardware

Use a regular ESP32 board, for example:

- ESP32 DevKit
- ESP32-WROOM board
- M5Stack Atom Lite

Place the ESP32 close to the projector.

## Configure

Copy the example config:

```bash
cp src/config.example.h src/config.h
```

Edit `src/config.h`:

```cpp
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define DEVICE_HOSTNAME "xgimi-ble-wake"
#define XGIMI_PAYLOAD_HEX "5386b44412c970ffffff3043524b544d"
```

`XGIMI_PAYLOAD_HEX` is the manufacturer payload captured from the XGIMI remote. Do not prepend the company id. The firmware prepends XGIMI company id `0x0046` as `46 00`.

## Build And Flash

Install [PlatformIO](https://platformio.org/), then run:

```bash
pio run -t upload
pio device monitor
```

The serial monitor prints the device IP address.

## Web UI

Open:

```text
http://xgimi-ble-wake.local/
```

or:

```text
http://ESP32-IP/
```

Press **Einschalten** to send the BLE wake advertisement.

## Home Assistant

The firmware can be used from Home Assistant through its HTTP endpoint. Use either the mDNS host name or the static IP address of the ESP32.

### Basic Configuration

Add this to `configuration.yaml`:

```yaml
rest_command:
  xgimi_ble_power_on:
    url: "http://xgimi-ble-wake.local/poweron"
    method: POST
```

If mDNS is not reliable in your network, use a static ESP32 address instead:

```yaml
rest_command:
  xgimi_ble_power_on:
    url: "http://192.168.1.42/poweron"
    method: POST
```

Restart Home Assistant or reload YAML where supported.

### Script Entity

Add a script if you want a reusable entity for dashboards and automations:

```yaml
script:
  xgimi_power_on:
    alias: XGIMI einschalten
    icon: mdi:projector
    sequence:
      - action: rest_command.xgimi_ble_power_on
```

You can then call:

```yaml
action: script.xgimi_power_on
```

### Dashboard Button

Example Lovelace button card:

```yaml
type: button
name: XGIMI einschalten
icon: mdi:projector
tap_action:
  action: call-service
  service: script.xgimi_power_on
```

### Optional Status Sensor

The ESP32 exposes `GET /status`. Home Assistant can read it with a REST sensor:

```yaml
sensor:
  - platform: rest
    name: XGIMI BLE Wake Status
    resource: "http://xgimi-ble-wake.local/status"
    value_template: "{{ value_json.status }}"
    json_attributes:
      - ip
      - hostname
      - rssi
      - uptime
      - power_on_count
      - last_power_on
    scan_interval: 60
```

This is optional. The wake command does not depend on this sensor.

### Patch XGIMI Integration Turn-On

The custom XGIMI Home Assistant integration normally tries to send the BLE wake advertisement through a local BlueZ Bluetooth adapter. If Home Assistant has no local Bluetooth adapter, patch the integration so `remote.turn_on` calls the ESP32 instead.

In the custom integration file:

```text
custom_components/xgimi/remote.py
```

replace the existing turn-on method with:

```python
async def async_turn_on(self, **kwargs):
    """Turn the Xgimi Projector On."""
    await self.hass.services.async_call(
        "rest_command",
        "xgimi_ble_power_on",
        blocking=True,
    )
```

Restart Home Assistant after editing the integration. This keeps the normal Home Assistant call unchanged:

```yaml
action: remote.turn_on
target:
  entity_id: remote.xgimi
```

Only the implementation behind `remote.turn_on` changes: it calls this ESP32 instead of looking for a local BlueZ Bluetooth adapter.

### Automation Example

Example automation that wakes the projector when a helper button is pressed:

```yaml
automation:
  - alias: XGIMI Wake From Helper
    trigger:
      - platform: state
        entity_id: input_button.xgimi_power_on
    action:
      - action: rest_command.xgimi_ble_power_on
```

## API

Send wake command:

```bash
curl -X POST http://xgimi-ble-wake.local/poweron
```

Read status:

```bash
curl http://xgimi-ble-wake.local/status
```

Example response:

```json
{
  "status": "ok",
  "ip": "192.168.1.42",
  "hostname": "xgimi-ble-wake",
  "rssi": -52,
  "uptime": "0h 3m 12s",
  "power_on_count": 1,
  "last_power_on": "192s after boot"
}
```

## Notes

The projector must be in standby. If it is fully disconnected from power, BLE wake cannot work.

If the wake command is unreliable, move the ESP32 closer to the projector or increase `ADVERTISEMENT_REPEATS` in `src/main.cpp`.
