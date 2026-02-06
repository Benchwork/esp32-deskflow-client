/**
 * Synergy/Deskflow protocol handler — implementation
 */

#include "../include/synergy_protocol.h"
#include <string.h>

namespace synergy {

SynergyClient::SynergyClient()
    : _screenWidth(1920)
    , _screenHeight(1080)
    , _mouseCallback(nullptr)
    , _keyboardCallback(nullptr)
    , _screenActiveCallback(nullptr)
{
    strncpy(_clientName, "ESP32-Deskflow", sizeof(_clientName) - 1);
    _clientName[sizeof(_clientName) - 1] = '\0';
    reset();
}

void SynergyClient::reset() {
    _connected = false;
    _hasReceivedHello = false;
    _captured = false;
    _sequenceNumber = 0;
    _recvOfs = 0;
    _skipBytes = 0;
    _replyCur = _replyBuffer + 4; // Leave room for length header
    _mouseX = _mouseY = 0;
    _mouseWheelX = _mouseWheelY = 0;
    _mouseLeft = _mouseMiddle = _mouseRight = false;
}

void SynergyClient::setClientName(const char* name) {
    if (name && name[0]) {
        strncpy(_clientName, name, sizeof(_clientName) - 1);
        _clientName[sizeof(_clientName) - 1] = '\0';
    }
}

void SynergyClient::setScreenSize(uint16_t width, uint16_t height) {
    _screenWidth = width;
    _screenHeight = height;
}

int16_t SynergyClient::netToNative16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

int32_t SynergyClient::netToNative32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

void SynergyClient::addString(const char* str) {
    size_t len = strlen(str);
    memcpy(_replyCur, str, len);
    _replyCur += len;
}

void SynergyClient::addUInt8(uint8_t val) {
    *_replyCur++ = val;
}

void SynergyClient::addUInt16(uint16_t val) {
    *_replyCur++ = (uint8_t)(val >> 8);
    *_replyCur++ = (uint8_t)val;
}

void SynergyClient::addUInt32(uint32_t val) {
    *_replyCur++ = (uint8_t)(val >> 24);
    *_replyCur++ = (uint8_t)(val >> 16);
    *_replyCur++ = (uint8_t)(val >> 8);
    *_replyCur++ = (uint8_t)val;
}

bool SynergyClient::sendReply(Client& client) {
    uint32_t replyLen = (uint32_t)(_replyCur - _replyBuffer);
    uint32_t bodyLen = replyLen - 4;
    
    // Write length header (big-endian)
    _replyBuffer[0] = (uint8_t)(bodyLen >> 24);
    _replyBuffer[1] = (uint8_t)(bodyLen >> 16);
    _replyBuffer[2] = (uint8_t)(bodyLen >> 8);
    _replyBuffer[3] = (uint8_t)bodyLen;
    
    size_t written = client.write(_replyBuffer, replyLen);
    
    // Reset reply buffer
    _replyCur = _replyBuffer + 4;
    
    return written == replyLen;
}

void SynergyClient::processMessage(Client& client, const uint8_t* msg, uint32_t len) {
    if (len < 8) return; // Need at least 4-byte length + 4-byte command
    
    const uint8_t* cmd = msg + 4; // Skip length header
    uint32_t bodyLen = len - 4;
    
    // QINF - Query screen info
    if (memcmp(cmd, "QINF", 4) == 0) {
        Serial.println("[Synergy] QINF - sending screen info");
        addString("DINF");
        addUInt16(0);              // x origin
        addUInt16(0);              // y origin
        addUInt16(_screenWidth);   // width
        addUInt16(_screenHeight);  // height
        addUInt16(0);              // warp size
        addUInt16(0);              // mouse x
        addUInt16(0);              // mouse y
        sendReply(client);
        return;
    }
    
    // CIAK - Info acknowledgment
    if (memcmp(cmd, "CIAK", 4) == 0) {
        // Just acknowledge, nothing to do
        return;
    }
    
    // CROP - Reset options
    if (memcmp(cmd, "CROP", 4) == 0) {
        return;
    }
    
    // DSOP - Set options
    if (memcmp(cmd, "DSOP", 4) == 0) {
        return;
    }
    
    // CALV - Keep-alive
    if (memcmp(cmd, "CALV", 4) == 0) {
        // Reply with CALV then CNOP
        addString("CALV");
        sendReply(client);
        addString("CNOP");
        sendReply(client);
        return;
    }
    
    // CINN - Enter screen
    if (memcmp(cmd, "CINN", 4) == 0) {
        if (len >= 16) {
            _sequenceNumber = netToNative32(cmd + 8);
        }
        _captured = true;
        Serial.println("[Synergy] Screen entered");
        if (_screenActiveCallback) _screenActiveCallback(true);
        addString("CNOP");
        sendReply(client);
        return;
    }
    
    // COUT - Leave screen
    if (memcmp(cmd, "COUT", 4) == 0) {
        _captured = false;
        Serial.println("[Synergy] Screen left");
        if (_screenActiveCallback) _screenActiveCallback(false);
        addString("CNOP");
        sendReply(client);
        return;
    }
    
    // DMMV - Mouse move
    if (memcmp(cmd, "DMMV", 4) == 0) {
        if (len >= 12) {
            _mouseX = netToNative16(cmd + 4);
            _mouseY = netToNative16(cmd + 6);
            if (_mouseCallback) {
                _mouseCallback(_mouseX, _mouseY, _mouseWheelX, _mouseWheelY,
                              _mouseLeft, _mouseMiddle, _mouseRight);
            }
        }
        return;
    }
    
    // DMDN - Mouse down
    if (memcmp(cmd, "DMDN", 4) == 0) {
        if (len >= 9) {
            uint8_t btn = cmd[4] - 1;
            if (btn == 0) _mouseLeft = true;
            else if (btn == 1) _mouseMiddle = true;
            else if (btn == 2) _mouseRight = true;
            if (_mouseCallback) {
                _mouseCallback(_mouseX, _mouseY, _mouseWheelX, _mouseWheelY,
                              _mouseLeft, _mouseMiddle, _mouseRight);
            }
        }
        return;
    }
    
    // DMUP - Mouse up
    if (memcmp(cmd, "DMUP", 4) == 0) {
        if (len >= 9) {
            uint8_t btn = cmd[4] - 1;
            if (btn == 0) _mouseLeft = false;
            else if (btn == 1) _mouseMiddle = false;
            else if (btn == 2) _mouseRight = false;
            if (_mouseCallback) {
                _mouseCallback(_mouseX, _mouseY, _mouseWheelX, _mouseWheelY,
                              _mouseLeft, _mouseMiddle, _mouseRight);
            }
        }
        return;
    }
    
    // DMWM - Mouse wheel
    if (memcmp(cmd, "DMWM", 4) == 0) {
        if (len >= 12) {
            _mouseWheelX = netToNative16(cmd + 4);
            _mouseWheelY = netToNative16(cmd + 6);
            if (_mouseCallback) {
                _mouseCallback(_mouseX, _mouseY, _mouseWheelX, _mouseWheelY,
                              _mouseLeft, _mouseMiddle, _mouseRight);
            }
            // Reset wheel values after processing so they don't persist to move events
            _mouseWheelX = 0;
            _mouseWheelY = 0;
        }
        return;
    }
    
    // DKDN - Key down
    if (memcmp(cmd, "DKDN", 4) == 0) {
        if (len >= 12) {
            uint16_t mod = netToNative16(cmd + 6);
            uint16_t key = netToNative16(cmd + 8);
            if (_keyboardCallback) {
                _keyboardCallback(key, mod, true, false);
            }
        }
        return;
    }
    
    // DKUP - Key up
    if (memcmp(cmd, "DKUP", 4) == 0) {
        if (len >= 12) {
            uint16_t mod = netToNative16(cmd + 6);
            uint16_t key = netToNative16(cmd + 8);
            if (_keyboardCallback) {
                _keyboardCallback(key, mod, false, false);
            }
        }
        return;
    }
    
    // DKRP - Key repeat
    if (memcmp(cmd, "DKRP", 4) == 0) {
        if (len >= 14) {
            uint16_t mod = netToNative16(cmd + 6);
            uint16_t key = netToNative16(cmd + 10);
            if (_keyboardCallback) {
                _keyboardCallback(key, mod, true, true);
            }
        }
        return;
    }
    
    // CNOP - No-op (ignore)
    if (memcmp(cmd, "CNOP", 4) == 0) {
        return;
    }
    
    // EUNK - Error: Unknown client
    if (memcmp(cmd, "EUNK", 4) == 0) {
        Serial.printf("[Synergy] ERROR: Unknown client '%s' - add this screen name to server!\n", _clientName);
        reset();  // Reset state so we can try again
        return;
    }
    
    // EBSY - Error: Server busy
    if (memcmp(cmd, "EBSY", 4) == 0) {
        Serial.println("[Synergy] ERROR: Server busy");
        reset();
        return;
    }
    
    // EBAD - Error: Bad/incompatible version
    if (memcmp(cmd, "EBAD", 4) == 0) {
        Serial.println("[Synergy] ERROR: Incompatible protocol version");
        reset();
        return;
    }
    
    // DCLP - Clipboard data (ignore, we don't support clipboard)
    if (memcmp(cmd, "DCLP", 4) == 0) {
        // Silently ignore clipboard data
        return;
    }
    
    // DSOP - Set options (ignore)
    if (memcmp(cmd, "DSOP", 4) == 0) {
        return;
    }
    
    // Unknown packet (only log once per packet type to reduce spam)
    char pktId[5] = {0};
    memcpy(pktId, cmd, 4);
    static char lastUnknown[5] = {0};
    if (memcmp(pktId, lastUnknown, 4) != 0) {
        Serial.println("[Synergy] Unknown packet: " + String(pktId));
        memcpy(lastUnknown, pktId, 4);
    }
}

bool SynergyClient::update(Client& client) {
    if (!client.connected()) {
        if (_connected) {
            Serial.println("[Synergy] Disconnected");
            reset();
        }
        return false;
    }
    
    // Skip remaining bytes from oversized packet
    while (_skipBytes > 0 && client.available()) {
        client.read(); // Discard byte
        _skipBytes--;
    }
    if (_skipBytes > 0) {
        return true; // Still skipping, wait for more data
    }
    
    // Read available data
    while (client.available() && _recvOfs < SYNERGY_RECV_BUFFER_SIZE) {
        _recvBuffer[_recvOfs++] = client.read();
    }
    
    // Handle initial hello - server sends length-prefixed "Synergy" or "Barrier" or "Deskflow"
    // Format: [4-byte length][protocol name][2-byte major][2-byte minor]
    if (!_hasReceivedHello && _recvOfs >= 4) {
        uint32_t msgLen = netToNative32(_recvBuffer);
        uint32_t totalLen = msgLen + 4;
        
        if (_recvOfs >= (int)totalLen && msgLen >= 11) {
            const uint8_t* payload = _recvBuffer + 4;
            
            // Check for "Synergy", "Barrier", or "Deskflow" in payload
            bool isSynergy = (memcmp(payload, "Synergy", 7) == 0);
            bool isBarrier = (memcmp(payload, "Barrier", 7) == 0);
            bool isDeskflow = (msgLen >= 12 && memcmp(payload, "Deskflow", 8) == 0);
            
            if (isSynergy || isBarrier || isDeskflow) {
                const char* proto = isDeskflow ? "Deskflow" : (isBarrier ? "Barrier" : "Synergy");
                int nameLen = isDeskflow ? 8 : 7;
                uint16_t major = netToNative16(payload + nameLen);
                uint16_t minor = netToNative16(payload + nameLen + 2);
                Serial.printf("[Synergy] Server hello: %s %d.%d\n", proto, major, minor);
                
                // Send our hello response (WITH length prefix)
                addString(proto);
                addUInt16(SYNERGY_PROTOCOL_MAJOR);
                addUInt16(SYNERGY_PROTOCOL_MINOR);
                addUInt32((uint32_t)strlen(_clientName));
                addString(_clientName);
                
                if (sendReply(client)) {
                    _hasReceivedHello = true;
                    _connected = true;
                    Serial.println("[Synergy] Connected as " + String(_clientName));
                }
                
                // Consume the hello message
                memmove(_recvBuffer, _recvBuffer + totalLen, _recvOfs - totalLen);
                _recvOfs -= totalLen;
        }
        }
    }
    
    // Process length-prefixed messages (after hello)
    while (_hasReceivedHello && _recvOfs >= 4) {
        uint32_t msgLen = netToNative32(_recvBuffer);
        uint32_t totalLen = msgLen + 4; // Include header
        
        if (totalLen > SYNERGY_RECV_BUFFER_SIZE) {
            // Oversized packet - skip it instead of disconnecting
            // This happens with clipboard data (DCLP) which can be very large
            _skipBytes = totalLen - _recvOfs; // How much more to skip
            Serial.printf("[Synergy] Skipping oversized packet (%u bytes)\n", totalLen);
            _recvOfs = 0; // Clear buffer, we'll skip the rest
            break;
        }
        
        if ((uint32_t)_recvOfs < totalLen) {
            // Incomplete message, wait for more data
            break;
        }
        
        // Process this message
        processMessage(client, _recvBuffer, totalLen);
        
        // Move remaining data to front of buffer
        memmove(_recvBuffer, _recvBuffer + totalLen, _recvOfs - totalLen);
        _recvOfs -= totalLen;
    }
    
    return true;
}

} // namespace synergy
