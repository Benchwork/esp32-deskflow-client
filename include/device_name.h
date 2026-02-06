/**
 * Unique device name from MAC address
 * Format: Deskflow-XXXX (last 2 bytes of MAC as hex).
 */

#ifndef DEVICE_NAME_H
#define DEVICE_NAME_H

#include <Arduino.h>

namespace device_name {

/** Generate unique name, e.g. "Deskflow-A3B2". */
String get();

/** Get raw MAC as string for display. */
String getMacString();

} // namespace device_name

#endif // DEVICE_NAME_H
