/**
 * W5500 Ethernet setup for Waveshare ESP32-S3-PoE-ETH
 * SPI pins and init sequence.
 */

#ifndef ETHERNET_SETUP_H
#define ETHERNET_SETUP_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

namespace ethernet {

/** Initialize W5500 with Waveshare pin mapping. DHCP by default. */
bool begin();

/** Optional: static IP. Call instead of begin() or after. */
bool begin(const uint8_t* mac, IPAddress ip, IPAddress dns, IPAddress gw, IPAddress subnet);

/** Check link status. */
bool isLinked();

/** Get current local IP (or fallback if chip not detected). */
IPAddress getLocalIP();

/** True if using static fallback IP because DHCP failed. */
bool isUsingFallbackIP();

/** Poll (e.g. for link/status). Call from loop. */
void poll();

} // namespace ethernet

#endif // ETHERNET_SETUP_H
