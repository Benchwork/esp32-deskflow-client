/**
 * ESP32-S3-PoE-ETH (W5500) Deskflow Client
 * Bridge: Deskflow → Ethernet → ESP32 → BLE HID → Target Computer
 */

#include "config.h"
#include "device_name.h"
#include "ethernet_setup.h"
#include "ble_hid.h"
#include "deskflow_server.h"
#include "web_ui.h"
#include <Ethernet.h>

void setup() {
    Serial.begin(115200);
    // Give USB CDC time to reconnect after reset so first lines are not lost
    delay(2000);
    Serial.println("[Deskflow] setup start");

    Serial.println("[Deskflow] device name...");
    String name = device_name::get();
    web_ui::log("Device: " + name);
    web_ui::log("MAC: " + device_name::getMacString());
    Serial.println("[Deskflow] name: " + name);

    Serial.println("[Deskflow] ethernet init...");
    ethernet::begin();
    {
        EthernetHardwareStatus hw = Ethernet.hardwareStatus();
        const char* hwStr = (hw == EthernetNoHardware) ? "NoHardware" : (hw == EthernetW5500) ? "W5500" : "Other";
        Serial.println("[Deskflow] Ethernet HW: " + String(hwStr) + " (chip not found = no W5500 on SPI 12,13,11,10)");
    }
    if (ethernet::isUsingFallbackIP()) {
        Serial.println("[Deskflow] Ethernet: no DHCP, using fallback IP " + ethernet::getLocalIP().toString());
        Serial.println("[Deskflow] Set your PC to 192.168.1.x to reach WebUI and TCP.");
        web_ui::log("Ethernet: fallback " + ethernet::getLocalIP().toString());
    } else {
        Serial.println("[Deskflow] Ethernet OK (DHCP): " + ethernet::getLocalIP().toString());
        web_ui::log("Ethernet: " + ethernet::getLocalIP().toString());
    }

    Serial.println("[Deskflow] BLE HID init...");
    ble_hid::begin(name.c_str());
    web_ui::log("BLE HID: advertising as " + name);
    Serial.println("[Deskflow] BLE HID started");

    Serial.println("[Deskflow] Deskflow client init...");
    deskflow::begin();
    web_ui::log("Deskflow client ready");
    Serial.println("[Deskflow] WebUI...");
    web_ui::begin();
    web_ui::log("WebUI: http://" + ethernet::getLocalIP().toString());

    Serial.println("[Deskflow] Ready. Set server URL via WebUI at http://" + ethernet::getLocalIP().toString());
}

static uint32_t _lastLog = 0;

void loop() {
    ethernet::poll();
    // Update remote endpoint from WebUI (if set)
    deskflow::setRemoteEndpoint(web_ui::getDeskflowServerUrl());
    deskflow::poll();
    ble_hid::poll();
    web_ui::poll();

    // Heartbeat every 30s (reduced from 10s to reduce log spam)
    uint32_t now = millis();
    if (now - _lastLog >= 30000) {
        _lastLog = now;
        Serial.println("[Deskflow] running | IP " + ethernet::getLocalIP().toString() + " | BLE " + (ble_hid::isConnected() ? "connected" : "advertising"));
    }
    delay(1);
}
