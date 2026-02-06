/**
 * Deskflow server — Synergy protocol client
 * Connects to Deskflow/Synergy server and forwards events to BLE HID.
 */

#include "../include/config.h"
#include "../include/deskflow_server.h"
#include "../include/synergy_protocol.h"
#include "../include/ble_hid.h"
#include "../include/web_ui.h"
#include "../include/device_name.h"
#include <Ethernet.h>

namespace deskflow {

static EthernetClient _remoteClient;
static String _remoteHost;
static uint16_t _remotePort = 24800;  // Default Deskflow port
static bool _useRemote = false;
static unsigned long _lastConnectAttempt = 0;

static synergy::SynergyClient _synergy;
static bool _initialized = false;

// Last mouse position for relative movement
static int16_t _lastMouseX = 0;
static int16_t _lastMouseY = 0;

// IBM PC AT Scancode Set 1 to ASCII character mapping
// Barrier/Deskflow sends hardware scancodes
static const uint8_t scancodeToAscii[128] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',   // 00-07: ?, Esc, 1-6
    '7',  '8',  '9',  '0',  '-',  '=',  0x08, 0x09,  // 08-0F: 7-0, -, =, Backspace, Tab
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   // 10-17: Q-I
    'o',  'p',  '[',  ']',  0x0D, 0xA0, 'a',  's',   // 18-1F: O, P, [, ], Enter, LCtrl, A, S
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   // 20-27: D-L, ;
    '\'', '`',  0xA1, '\\', 'z',  'x',  'c',  'v',   // 28-2F: ', `, LShift, \, Z-V
    'b',  'n',  'm',  ',',  '.',  '/',  0xA5, '*',   // 30-37: B-M, ,, ., /, RShift, KP*
    0xA2, ' ',  0xAA, 0x80, 0x81, 0x82, 0x83, 0x84,  // 38-3F: LAlt, Space, CapsLock, F1-F5
    0x85, 0x86, 0x87, 0x88, 0x89, 0,    0,    0xC7,  // 40-47: F6-F10, NumLock, ScrollLock, KP7
    0xC8, 0xC9, '-',  0xC4, 0xC5, 0xC6, '+',  0xC1,  // 48-4F: KP8, KP9, KP-, KP4, KP5, KP6, KP+, KP1
    0xC2, 0xC3, 0xC0, 0x7F, 0,    0,    0,    0x8A,  // 50-57: KP2, KP3, KP0, KP., ?, ?, NonUS\, F11
    0x8B, 0,    0,    0,    0,    0,    0,    0,     // 58-5F: F12, ...
    0,    0,    0,    0,    0,    0,    0,    0,     // 60-67
    0,    0,    0,    0,    0,    0,    0,    0,     // 68-6F
    0,    0,    0,    0,    0,    0,    0,    0,     // 70-77
    0,    0,    0,    0,    0,    0,    0,    0,     // 78-7F
};

// Special key codes (0x80+ for F keys, 0xA0+ for modifiers, 0xB0+ for navigation)
#define SPECIAL_F1      0x80
#define SPECIAL_F2      0x81
#define SPECIAL_F3      0x82
#define SPECIAL_F4      0x83
#define SPECIAL_F5      0x84
#define SPECIAL_F6      0x85
#define SPECIAL_F7      0x86
#define SPECIAL_F8      0x87
#define SPECIAL_F9      0x88
#define SPECIAL_F10     0x89
#define SPECIAL_F11     0x8A
#define SPECIAL_F12     0x8B

#define SPECIAL_LCTRL   0xA0
#define SPECIAL_LSHIFT  0xA1
#define SPECIAL_LALT    0xA2
#define SPECIAL_LGUI    0xA3
#define SPECIAL_RCTRL   0xA4
#define SPECIAL_RSHIFT  0xA5
#define SPECIAL_RALT    0xA6
#define SPECIAL_RGUI    0xA7

#define SPECIAL_HOME    0xB0
#define SPECIAL_END     0xB1
#define SPECIAL_PAGEUP  0xB2
#define SPECIAL_PAGEDN  0xB3
#define SPECIAL_INSERT  0xB4
#define SPECIAL_DELETE  0xB5
#define SPECIAL_UP      0xB6
#define SPECIAL_DOWN    0xB7
#define SPECIAL_LEFT    0xB8
#define SPECIAL_RIGHT   0xB9

// Convert Synergy/Barrier scancode to ASCII (or special key code)
static uint8_t synergyToAscii(uint16_t synergyKey) {
    // Check if it's a scancode (0x00-0x7F range)
    if (synergyKey < 128) {
        return scancodeToAscii[synergyKey];
    }
    
    // Extended scancodes with 0x01xx prefix (Barrier's format for extended keys)
    if ((synergyKey & 0xFF00) == 0x0100) {
        uint8_t ext = synergyKey & 0xFF;
        switch (ext) {
            case 0x47: return SPECIAL_HOME;     // Home
            case 0x4F: return SPECIAL_END;      // End
            case 0x49: return SPECIAL_PAGEUP;   // Page Up
            case 0x51: return SPECIAL_PAGEDN;   // Page Down
            case 0x52: return SPECIAL_INSERT;   // Insert
            case 0x53: return SPECIAL_DELETE;   // Delete
            case 0x48: return SPECIAL_UP;       // Up arrow
            case 0x50: return SPECIAL_DOWN;     // Down arrow
            case 0x4B: return SPECIAL_LEFT;     // Left arrow
            case 0x4D: return SPECIAL_RIGHT;    // Right arrow
            case 0x1D: return SPECIAL_RCTRL;    // Right Ctrl
            case 0x38: return SPECIAL_RALT;     // Right Alt
            case 0x5B: return SPECIAL_LGUI;     // Left Windows
            case 0x5C: return SPECIAL_RGUI;     // Right Windows
            case 0x1C: return 0x0D;             // Numpad Enter
            case 0x35: return '/';              // Numpad /
        }
    }
    
    // Extended scancodes (E0 prefix, alternative format)
    if ((synergyKey & 0xFF00) == 0xE000) {
        uint8_t ext = synergyKey & 0xFF;
        switch (ext) {
            case 0x47: return SPECIAL_HOME;
            case 0x4F: return SPECIAL_END;
            case 0x49: return SPECIAL_PAGEUP;
            case 0x51: return SPECIAL_PAGEDN;
            case 0x52: return SPECIAL_INSERT;
            case 0x53: return SPECIAL_DELETE;
            case 0x48: return SPECIAL_UP;
            case 0x50: return SPECIAL_DOWN;
            case 0x4B: return SPECIAL_LEFT;
            case 0x4D: return SPECIAL_RIGHT;
            case 0x1D: return SPECIAL_RCTRL;
            case 0x38: return SPECIAL_RALT;
            case 0x5B: return SPECIAL_LGUI;
            case 0x5C: return SPECIAL_RGUI;
        }
    }
    
    // X11 keysym special keys (0xFFxx range)
    switch (synergyKey) {
        case 0xFF0D: return 0x0D;  // Return/Enter
        case 0xFF1B: return 0x1B;  // Escape
        case 0xFF08: return 0x08;  // BackSpace
        case 0xFF09: return 0x09;  // Tab
        case 0x0020: return ' ';   // Space
        // Navigation keys (X11 keysyms)
        case 0xFF50: return SPECIAL_HOME;
        case 0xFF57: return SPECIAL_END;
        case 0xFF55: return SPECIAL_PAGEUP;
        case 0xFF56: return SPECIAL_PAGEDN;
        case 0xFF63: return SPECIAL_INSERT;
        case 0xFFFF: return SPECIAL_DELETE;
        case 0xFF52: return SPECIAL_UP;
        case 0xFF54: return SPECIAL_DOWN;
        case 0xFF51: return SPECIAL_LEFT;
        case 0xFF53: return SPECIAL_RIGHT;
        // Modifiers (X11 keysyms)
        case 0xFFE1: return SPECIAL_LSHIFT;
        case 0xFFE2: return SPECIAL_RSHIFT;
        case 0xFFE3: return SPECIAL_LCTRL;
        case 0xFFE4: return SPECIAL_RCTRL;
        case 0xFFE9: return SPECIAL_LALT;
        case 0xFFEA: return SPECIAL_RALT;
        case 0xFFEB: return SPECIAL_LGUI;
        case 0xFFEC: return SPECIAL_RGUI;
        default: return 0;
    }
}

// Convert Synergy modifiers to HID modifiers
static uint8_t synergyToHidMod(uint16_t synergyMod) {
    uint8_t hid = 0;
    if (synergyMod & 0x0001) hid |= 0x02;  // Shift -> Left Shift
    if (synergyMod & 0x0002) hid |= 0x01;  // Ctrl -> Left Ctrl
    if (synergyMod & 0x0004) hid |= 0x04;  // Alt -> Left Alt
    if (synergyMod & 0x0008) hid |= 0x08;  // Meta/Win -> Left GUI
    return hid;
}

// Mouse callback from Synergy protocol
static void onMouse(int16_t x, int16_t y, int16_t wheelX, int16_t wheelY,
                    bool btnLeft, bool btnMiddle, bool btnRight) {
    // Calculate relative movement
    int16_t dx = x - _lastMouseX;
    int16_t dy = y - _lastMouseY;
    _lastMouseX = x;
    _lastMouseY = y;
    
    // Clamp to int8 range
    if (dx > 127) dx = 127;
    if (dx < -127) dx = -127;
    if (dy > 127) dy = 127;
    if (dy < -127) dy = -127;
    
    // Build button mask
    uint8_t buttons = 0;
    if (btnLeft) buttons |= 0x01;
    if (btnRight) buttons |= 0x02;
    if (btnMiddle) buttons |= 0x04;
    
    // Wheel is sent as delta in 120-unit increments (Windows standard)
    // Scale down to reasonable HID scroll amount
    int8_t wheel = 0;
    if (wheelY != 0) {
        // Each 120 units = 1 scroll notch
        wheel = (int8_t)(wheelY / 120);
        // If there's a small value, still send at least 1
        if (wheel == 0 && wheelY > 0) wheel = 1;
        if (wheel == 0 && wheelY < 0) wheel = -1;
    }
    
    ble_hid::mouseReport(buttons, (int8_t)dx, (int8_t)dy, wheel);
}

// Track currently pressed keys for proper release
static uint8_t _pressedKey = 0;

// Keyboard callback from Synergy protocol
static void onKeyboard(uint16_t key, uint16_t modifiers, bool down, bool repeat) {
    uint8_t asciiKey = synergyToAscii(key);
    
    // Debug: show what we receive and what we send
    Serial.printf("[Key] syn=0x%04X -> ascii=0x%02X ('%c') %s\n", 
                  key, asciiKey, (asciiKey >= 32 && asciiKey < 127) ? asciiKey : '?', 
                  down ? "DOWN" : "UP");
    
    if (asciiKey == 0 && key != 0) {
        return;
    }
    
    ble_hid::keyPress(asciiKey, modifiers, down);
}

// Screen active callback
static void onScreenActive(bool active) {
    if (active) {
        web_ui::log("Screen activated - receiving input");
        // Reset mouse position on enter
        _lastMouseX = 0;
        _lastMouseY = 0;
    } else {
        web_ui::log("Screen deactivated");
        // Release all keys/buttons when leaving
        uint8_t keys[6] = {0};
        ble_hid::keyboardReport(0, keys);
        ble_hid::mouseReport(0, 0, 0, 0);
    }
}

void begin() {
    _synergy.setClientName(device_name::get().c_str());
    _synergy.setScreenSize(1920, 1080);  // Virtual screen size
    _synergy.setMouseCallback(onMouse);
    _synergy.setKeyboardCallback(onKeyboard);
    _synergy.setScreenActiveCallback(onScreenActive);
    _initialized = true;
}

static void ensureRemoteConnected() {
    if (!_useRemote) return;
    if (_remoteClient && _remoteClient.connected()) return;
    
    unsigned long now = millis();
    if (now - _lastConnectAttempt < 5000) return; // Retry every 5s
    _lastConnectAttempt = now;
    
    if (!_remoteHost.length()) return;
    
    // Reset synergy state before new connection attempt
    _synergy.resetState();
    
    Serial.println("[Deskflow] Connecting to " + _remoteHost + ":" + String(_remotePort));
    web_ui::log("Connecting to " + _remoteHost + ":" + String(_remotePort));
    
    _remoteClient.stop();
    if (_remoteClient.connect(_remoteHost.c_str(), _remotePort)) {
        Serial.println("[Deskflow] TCP connected, waiting for handshake...");
        web_ui::log("TCP connected");
    } else {
        Serial.println("[Deskflow] Connection failed");
        web_ui::log("Connection failed");
    }
}

void setRemoteEndpoint(const String& url) {
    String u = url;
    u.trim();
    
    if (!u.length()) {
        if (_useRemote) {
            _useRemote = false;
            _remoteClient.stop();
            Serial.println("[Deskflow] Remote endpoint cleared");
        }
        return;
    }
    
    // Strip protocol prefix
    if (u.startsWith("tcp://")) u.remove(0, 6);
    else if (u.startsWith("synergy://")) u.remove(0, 10);
    else if (u.startsWith("deskflow://")) u.remove(0, 11);
    
    // Parse host:port
    String host = u;
    uint16_t port = 24800;
    int colon = u.lastIndexOf(':');
    if (colon > 0 && colon < (int)u.length() - 1) {
        host = u.substring(0, colon);
        port = (uint16_t)u.substring(colon + 1).toInt();
        if (!port) port = 24800;
    }
    
    // Check if changed
    if (host == _remoteHost && port == _remotePort && _useRemote) {
        return;
    }
    
    _remoteHost = host;
    _remotePort = port;
    _useRemote = true;
    _remoteClient.stop();
    _lastConnectAttempt = 0;
    
    Serial.println("[Deskflow] Remote endpoint: " + _remoteHost + ":" + String(_remotePort));
    web_ui::log("Endpoint: " + _remoteHost + ":" + String(_remotePort));
}

void poll() {
    if (!_initialized) return;
    
    ensureRemoteConnected();
    
    if (_useRemote && _remoteClient.connected()) {
        _synergy.update(_remoteClient);
    }
}

} // namespace deskflow
