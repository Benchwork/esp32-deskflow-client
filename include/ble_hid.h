/**
 * BLE HID keyboard + mouse emulation
 * ESP32 NimBLE/Bluetooth stack — reports to target computer.
 */

#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>

namespace ble_hid {

/** Initialize BLE stack and HID (keyboard + mouse). Name from device_name module. */
void begin(const char* deviceName);

/** Send keyboard report (modifiers + key codes). */
void keyboardReport(uint8_t modifiers, const uint8_t* keys);

/** Press or release a single key (ASCII or special key code). */
void keyPress(uint8_t asciiKey, uint16_t modifiers, bool down);

/** Send mouse report (buttons, dx, dy, wheel). */
void mouseReport(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);

/** Whether a HID host is connected. */
bool isConnected();

/** Call from loop to handle BLE. */
void poll();

} // namespace ble_hid

#endif // BLE_HID_H
