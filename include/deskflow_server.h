/**
 * Deskflow TCP / WebSocket server
 * Receives keyboard and mouse events and forwards to BLE HID.
 */

#ifndef DESKFLOW_SERVER_H
#define DESKFLOW_SERVER_H

#include <Arduino.h>

namespace deskflow {

/** Start TCP and optional WebSocket listeners. */
void begin();

/** Process incoming clients and parse events. Call from loop. */
void poll();

/** Configure remote Deskflow endpoint (e.g. tcp://host:port). Empty to use local TCP server only. */
void setRemoteEndpoint(const String& url);

} // namespace deskflow

#endif // DESKFLOW_SERVER_H
