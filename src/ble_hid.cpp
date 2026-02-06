/**
 * BLE HID keyboard + mouse — implementation
 * Uses ESP32-BLE-Combo library for combined keyboard+mouse as single BLE device.
 * Uses NimBLE stack for better HID compatibility.
 */

#include "../include/ble_hid.h"
#include "../include/config.h"

// USE_NIMBLE is defined in platformio.ini build_flags
#include <BleKeyboard.h>
#include <BleMouse.h>
#include <NimBLEDevice.h>

namespace ble_hid {

static String _name;
static uint8_t _lastButtons = 0;
static bool _initialized = false;
static bool _wasConnected = false;
static unsigned long _lastMouseReport = 0;
static const unsigned long MOUSE_REPORT_INTERVAL_MS = 8; // Minimum ms between reports

void begin(const char* deviceName) {
    _name = deviceName;
    Serial.println("[BLE] Initializing BLE HID combo device (NimBLE)...");
    
    // Set device name before begin
    bleDevice.setName(deviceName);
    bleDevice.setBatteryLevel(100);
    
    // Minimal delay between reports for responsiveness (default is 7ms)
    bleDevice.setDelay(5);
    
    // Start the combined keyboard+mouse BLE device
    Keyboard.begin();
    
    // Don't clear bonds - allow persistent pairing
    int numBonds = NimBLEDevice::getNumBonds();
    Serial.printf("[BLE] Found %d existing bond(s)\n", numBonds);
    
    _initialized = true;
    Serial.println("[BLE] BLE HID device started: " + _name);
    Serial.println("[BLE] Waiting for host to connect...");
}

void keyboardReport(uint8_t modifiers, const uint8_t* keys) {
    if (!_initialized || !bleDevice.isConnected()) {
        return;
    }
    
    // Release all first, then apply new state
    Keyboard.releaseAll();
    
    // Apply modifier keys
    if (modifiers & 0x01) Keyboard.press(KEY_LEFT_CTRL);
    if (modifiers & 0x02) Keyboard.press(KEY_LEFT_SHIFT);
    if (modifiers & 0x04) Keyboard.press(KEY_LEFT_ALT);
    if (modifiers & 0x08) Keyboard.press(KEY_LEFT_GUI);
    if (modifiers & 0x10) Keyboard.press(KEY_RIGHT_CTRL);
    if (modifiers & 0x20) Keyboard.press(KEY_RIGHT_SHIFT);
    if (modifiers & 0x40) Keyboard.press(KEY_RIGHT_ALT);
    if (modifiers & 0x80) Keyboard.press(KEY_RIGHT_GUI);
    
    // Press regular keys (up to 6)
    for (int i = 0; i < 6; i++) {
        if (keys[i] != 0) {
            Keyboard.press(keys[i]);
        }
    }
}

// Special key codes (0x80+) to BleKeyboard key constants
static uint8_t specialToKey(uint8_t code) {
    switch (code) {
        // F keys (0x80-0x8B)
        case 0x80: return KEY_F1;
        case 0x81: return KEY_F2;
        case 0x82: return KEY_F3;
        case 0x83: return KEY_F4;
        case 0x84: return KEY_F5;
        case 0x85: return KEY_F6;
        case 0x86: return KEY_F7;
        case 0x87: return KEY_F8;
        case 0x88: return KEY_F9;
        case 0x89: return KEY_F10;
        case 0x8A: return KEY_F11;
        case 0x8B: return KEY_F12;
        // Modifiers (0xA0-0xA7)
        case 0xA0: return KEY_LEFT_CTRL;
        case 0xA1: return KEY_LEFT_SHIFT;
        case 0xA2: return KEY_LEFT_ALT;
        case 0xA3: return KEY_LEFT_GUI;
        case 0xA4: return KEY_RIGHT_CTRL;
        case 0xA5: return KEY_RIGHT_SHIFT;
        case 0xA6: return KEY_RIGHT_ALT;
        case 0xA7: return KEY_RIGHT_GUI;
        // Lock keys (0xA8-0xAA)
        case 0xAA: return KEY_CAPS_LOCK;
        // Navigation (0xB0-0xB9)
        case 0xB0: return KEY_HOME;
        case 0xB1: return KEY_END;
        case 0xB2: return KEY_PAGE_UP;
        case 0xB3: return KEY_PAGE_DOWN;
        case 0xB4: return KEY_INSERT;
        case 0xB5: return KEY_DELETE;
        case 0xB6: return KEY_UP_ARROW;
        case 0xB7: return KEY_DOWN_ARROW;
        case 0xB8: return KEY_LEFT_ARROW;
        case 0xB9: return KEY_RIGHT_ARROW;
        // Numpad (0xC0-0xC9) - map to regular number keys for simplicity
        // Or use KEY_KP_* if the BleKeyboard library supports it
        case 0xC0: return '0';  // KP 0
        case 0xC1: return '1';  // KP 1
        case 0xC2: return '2';  // KP 2
        case 0xC3: return '3';  // KP 3
        case 0xC4: return '4';  // KP 4
        case 0xC5: return '5';  // KP 5
        case 0xC6: return '6';  // KP 6
        case 0xC7: return '7';  // KP 7
        case 0xC8: return '8';  // KP 8
        case 0xC9: return '9';  // KP 9
        default: return 0;
    }
}

void keyPress(uint8_t asciiKey, uint16_t modifiers, bool down) {
    if (!_initialized || !bleDevice.isConnected()) {
        return;
    }
    
    // Handle special keys (F1-F12, modifiers, navigation)
    if (asciiKey >= 0x80) {
        uint8_t specialKey = specialToKey(asciiKey);
        if (specialKey) {
            if (down) Keyboard.press(specialKey);
            else Keyboard.release(specialKey);
        }
        return;
    }
    
    // Handle control characters
    uint8_t key = asciiKey;
    switch (asciiKey) {
        case 0x08: key = KEY_BACKSPACE; break;
        case 0x09: key = KEY_TAB; break;
        case 0x0D: key = KEY_RETURN; break;
        case 0x1B: key = KEY_ESC; break;
        case 0x7F: key = KEY_DELETE; break;
        default: break;
    }
    
    // Regular ASCII characters
    if (key != 0) {
        if (down) {
            Keyboard.press(key);
        } else {
            Keyboard.release(key);
        }
    }
}

// Accumulated movement between reports
static int16_t _accumDx = 0;
static int16_t _accumDy = 0;
static int8_t _accumWheel = 0;

void mouseReport(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel) {
    if (!_initialized || !bleDevice.isConnected()) {
        return;
    }
    
    // Handle button state changes immediately
    uint8_t changed = buttons ^ _lastButtons;
    if (changed & 0x01) { // Left button
        if (buttons & 0x01) Mouse.press(MOUSE_LEFT);
        else Mouse.release(MOUSE_LEFT);
    }
    if (changed & 0x02) { // Right button
        if (buttons & 0x02) Mouse.press(MOUSE_RIGHT);
        else Mouse.release(MOUSE_RIGHT);
    }
    if (changed & 0x04) { // Middle button
        if (buttons & 0x04) Mouse.press(MOUSE_MIDDLE);
        else Mouse.release(MOUSE_MIDDLE);
    }
    _lastButtons = buttons;
    
    // Accumulate movement
    _accumDx += dx;
    _accumDy += dy;
    _accumWheel += wheel;
    
    // Rate limit movement reports to avoid flooding BLE
    unsigned long now = millis();
    if (now - _lastMouseReport < MOUSE_REPORT_INTERVAL_MS) {
        return; // Will send accumulated movement on next report
    }
    
    // Send accumulated movement
    if (_accumDx != 0 || _accumDy != 0 || _accumWheel != 0) {
        // Clamp accumulated values to int8 range
        int8_t sendDx = (_accumDx > 127) ? 127 : ((_accumDx < -127) ? -127 : (int8_t)_accumDx);
        int8_t sendDy = (_accumDy > 127) ? 127 : ((_accumDy < -127) ? -127 : (int8_t)_accumDy);
        int8_t sendWheel = (_accumWheel > 127) ? 127 : ((_accumWheel < -127) ? -127 : _accumWheel);
        
        Mouse.move(sendDx, sendDy, sendWheel, 0);
        
        // Subtract what we sent (preserving any overflow for next report)
        _accumDx -= sendDx;
        _accumDy -= sendDy;
        _accumWheel -= sendWheel;
        
        _lastMouseReport = now;
    }
}

bool isConnected() {
    return _initialized && bleDevice.isConnected();
}

void poll() {
    if (!_initialized) return;
    
    // Track connection state changes
    bool connected = bleDevice.isConnected();
    if (connected != _wasConnected) {
        if (connected) {
            Serial.println("[BLE] Host connected");
        } else {
            Serial.println("[BLE] Host disconnected");
            // Reset state on disconnect
            _lastButtons = 0;
            _accumDx = 0;
            _accumDy = 0;
            _accumWheel = 0;
        }
        _wasConnected = connected;
    }
}

} // namespace ble_hid
