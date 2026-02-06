/**
 * ESP32: EthernetServer is abstract (Server::begin(uint16_t) pure virtual).
 * This subclass implements begin(port) by calling the library's begin().
 */

#ifndef ETHERNET_SERVER_ESP32_H
#define ETHERNET_SERVER_ESP32_H

#include <Ethernet.h>

class EthernetServerESP32 : public EthernetServer {
public:
    explicit EthernetServerESP32(uint16_t port) : EthernetServer(port) {}
    void begin(uint16_t /*port*/ = 0) override { EthernetServer::begin(); }
};

#endif
