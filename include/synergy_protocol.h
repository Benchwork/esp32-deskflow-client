/**
 * Synergy/Deskflow protocol handler
 * Based on μSynergy (micro-client) reference implementation.
 */

#ifndef SYNERGY_PROTOCOL_H
#define SYNERGY_PROTOCOL_H

#include <Arduino.h>
#include <Client.h>

namespace synergy {

// Protocol version
#define SYNERGY_PROTOCOL_MAJOR 1
#define SYNERGY_PROTOCOL_MINOR 6

// Buffer sizes
#define SYNERGY_RECV_BUFFER_SIZE 4096
#define SYNERGY_REPLY_BUFFER_SIZE 256

// Callbacks
typedef void (*MouseCallback)(int16_t x, int16_t y, int16_t wheelX, int16_t wheelY, 
                               bool btnLeft, bool btnMiddle, bool btnRight);
typedef void (*KeyboardCallback)(uint16_t key, uint16_t modifiers, bool down, bool repeat);
typedef void (*ScreenActiveCallback)(bool active);

class SynergyClient {
public:
    SynergyClient();
    
    void setClientName(const char* name);
    void setScreenSize(uint16_t width, uint16_t height);
    
    void setMouseCallback(MouseCallback cb) { _mouseCallback = cb; }
    void setKeyboardCallback(KeyboardCallback cb) { _keyboardCallback = cb; }
    void setScreenActiveCallback(ScreenActiveCallback cb) { _screenActiveCallback = cb; }
    
    // Call with connected client; returns false on disconnect/error
    bool update(Client& client);
    
    // Reset client state (call when TCP connection drops)
    void resetState() { reset(); }
    
    bool isConnected() const { return _connected; }
    bool isCaptured() const { return _captured; }
    
private:
    void reset();
    bool sendReply(Client& client);
    void processMessage(Client& client, const uint8_t* msg, uint32_t len);
    
    void addString(const char* str);
    void addUInt8(uint8_t val);
    void addUInt16(uint16_t val);
    void addUInt32(uint32_t val);
    
    static int16_t netToNative16(const uint8_t* data);
    static int32_t netToNative32(const uint8_t* data);
    
    char _clientName[64];
    uint16_t _screenWidth;
    uint16_t _screenHeight;
    
    bool _connected;
    bool _hasReceivedHello;
    bool _captured;
    uint32_t _sequenceNumber;
    
    uint8_t _recvBuffer[SYNERGY_RECV_BUFFER_SIZE];
    int _recvOfs;
    uint32_t _skipBytes; // Bytes remaining to skip for oversized packet
    
    uint8_t _replyBuffer[SYNERGY_REPLY_BUFFER_SIZE];
    uint8_t* _replyCur;
    
    // Mouse state
    int16_t _mouseX, _mouseY;
    int16_t _mouseWheelX, _mouseWheelY;
    bool _mouseLeft, _mouseMiddle, _mouseRight;
    
    // Callbacks
    MouseCallback _mouseCallback;
    KeyboardCallback _keyboardCallback;
    ScreenActiveCallback _screenActiveCallback;
};

} // namespace synergy

#endif // SYNERGY_PROTOCOL_H
