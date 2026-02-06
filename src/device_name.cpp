/**
 * Device name from MAC — implementation
 */

#include "../include/config.h"
#include "../include/device_name.h"
#include <esp_mac.h>

namespace device_name {

String get() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%02X%02X", BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);
    return String(buf);
}

String getMacString() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

} // namespace device_name
