#pragma once

// Copy this file to src/config.h and adjust the values before flashing.

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional: change the host name shown in your router and via mDNS.
#define DEVICE_HOSTNAME "xgimi-ble-wake"

// XGIMI RC manufacturer payload captured from the original remote.
// Do not include the BLE company id here. The firmware prepends 0x0046.
#define XGIMI_PAYLOAD_HEX "5386b44412c970ffffff3043524b544d"
