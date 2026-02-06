/**
 * Deskflow Client — Build and hardware configuration
 * Waveshare ESP32-S3-PoE-ETH (W5500)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ——— W5500 SPI (Waveshare ESP32-S3-ETH official wiki) ———
#define W5500_SCK_PIN   13
#define W5500_MISO_PIN  12
#define W5500_MOSI_PIN  11
#define W5500_CS_PIN    14
#define W5500_INT_PIN   10
#define W5500_RST_PIN   9

// ——— Deskflow/Synergy ———
#define DESKFLOW_TCP_PORT    24800  // Default Synergy/Deskflow port

// ——— Web dashboard ———
#define WEBUI_HTTP_PORT      80

// ——— BLE HID ———
#define BLE_DEVICE_NAME_PREFIX  "Deskflow-"

// ——— Ethernet fallback when DHCP fails (e.g. cable unplugged at boot) ———
#define ETHERNET_FALLBACK_IP     192, 168, 1, 177
#define ETHERNET_FALLBACK_GW      192, 168, 1, 1
#define ETHERNET_FALLBACK_SUBNET  255, 255, 255, 0

#endif // CONFIG_H
