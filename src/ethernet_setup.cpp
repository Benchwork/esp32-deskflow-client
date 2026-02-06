/**
 * W5500 Ethernet setup — implementation
 * Waveshare: SCK=12, MISO=13, MOSI=11, CS=10, INT=14, RST=9
 */

#include "config.h"
#include "../include/ethernet_setup.h"
#include <Arduino.h>

namespace ethernet {

static byte _mac[6] = {0};
static bool _started = false;
static bool _usingFallbackIP = false;
static IPAddress _fallbackIP;  // used when chip not detected but we want to show intended IP

bool begin() {
    // Use ESP32 MAC (WiFi/BT) for unique address
    esp_read_mac(_mac, ESP_MAC_WIFI_STA);
    // Use Waveshare SPI pins before any library SPI.begin()
    SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);
    Ethernet.init(W5500_CS_PIN);
    if (W5500_RST_PIN != -1) {
        pinMode(W5500_RST_PIN, OUTPUT);
        digitalWrite(W5500_RST_PIN, LOW);
        delay(10);
        digitalWrite(W5500_RST_PIN, HIGH);
        delay(150);
    }
    // Try DHCP with 15s timeout (cable must be in at boot for DHCP to succeed)
    _started = Ethernet.begin(_mac, 15000, 4000) != 0;
    if (_started) {
        _usingFallbackIP = false;
        return true;
    }
    // No DHCP: try static fallback. W5500 must be detected for this to set IP.
    _usingFallbackIP = true;
    IPAddress ip(ETHERNET_FALLBACK_IP);
    IPAddress gw(ETHERNET_FALLBACK_GW);
    IPAddress subnet(ETHERNET_FALLBACK_SUBNET);
    IPAddress dns(ETHERNET_FALLBACK_GW);
    _fallbackIP = ip;  // for getLocalIP() when chip not detected
    Ethernet.begin(_mac, ip, dns, gw, subnet);
    _started = true;
    return true;
}

bool begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gw, IPAddress subnet) {
    memcpy(_mac, mac, 6);
    SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);
    Ethernet.init(W5500_CS_PIN);
    if (W5500_RST_PIN != -1) {
        pinMode(W5500_RST_PIN, OUTPUT);
        digitalWrite(W5500_RST_PIN, LOW);
        delay(10);
        digitalWrite(W5500_RST_PIN, HIGH);
        delay(100);
    }
    Ethernet.begin(const_cast<uint8_t*>(mac), ip, dns, gw, subnet);
    _started = true;
    _usingFallbackIP = false;
    return true;
}

bool isLinked() {
    return _started && Ethernet.hardwareStatus() == EthernetW5500;
}

IPAddress getLocalIP() {
    if (_usingFallbackIP && _fallbackIP[0] != 0 && Ethernet.hardwareStatus() != EthernetW5500)
        return _fallbackIP;
    return Ethernet.localIP();
}

bool isUsingFallbackIP() {
    return _usingFallbackIP;
}

void poll() {
    Ethernet.maintain();
}

} // namespace ethernet
