# ESP32-S3 Deskflow/Synergy BLE HID Client

A hardware Deskflow/Synergy client that bridges keyboard and mouse input from a Deskflow server to any computer via Bluetooth HID. This allows you to control a computer that doesn't have network access or can't run Deskflow software directly.

## How It Works

```
┌─────────────────┐      Ethernet       ┌─────────────────┐     Bluetooth      ┌─────────────────┐
│  Deskflow       │  ──────────────────►│  ESP32-S3       │  ────────────────► │  Target         │
│  Server         │   Synergy Protocol  │  (This Device)  │   BLE HID          │  Computer       │
│  (Your main PC) │                     │                 │                    │  (Controlled)   │
└─────────────────┘                     └─────────────────┘                    └─────────────────┘
```

The ESP32-S3 connects to your Deskflow/Synergy/Barrier server over Ethernet, receives keyboard and mouse events via the Synergy protocol, and re-transmits them as Bluetooth HID to a target computer. The target computer sees this as a standard Bluetooth keyboard and mouse.

## Features

- **Ethernet connectivity** via W5500 SPI (PoE supported on compatible boards)
- **BLE HID emulation** - appears as a combined keyboard + mouse to the target
- **Full keyboard support** - letters, numbers, symbols, F-keys, modifiers, navigation keys, numpad
- **Full mouse support** - movement, left/middle/right buttons, scroll wheel
- **Web UI dashboard** - view status and configure the Deskflow server URL
- **Auto-reconnect** - automatically reconnects if connection is lost
- **Unique device name** - generated from MAC address for easy identification

## Hardware Requirements

### Tested Board
- **Waveshare ESP32-S3-PoE-ETH (W5500 version)**
  - ESP32-S3 MCU with native BLE
  - W5500 SPI Ethernet controller
  - PoE support (optional)
  - USB-C for flashing and power

### W5500 Pin Mapping (Waveshare ESP32-S3-ETH)

| Function | GPIO Pin |
|----------|----------|
| SPI SCK  | GPIO 13  |
| SPI MISO | GPIO 12  |
| SPI MOSI | GPIO 11  |
| CS       | GPIO 14  |
| INT      | GPIO 10  |
| RST      | GPIO 9   |

> **Note:** If using a different ESP32-S3 board with W5500, you may need to modify the pin definitions in `include/config.h` and the SPI initialization in `lib/Ethernet/src/utility/w5100.cpp`.

## Software Requirements

- **PlatformIO** (recommended) or Arduino IDE
- **Python 3.x** (for PlatformIO)
- **Deskflow**, **Synergy**, or **Barrier** server running on your main computer

## Project Structure

```
├── include/
│   ├── config.h              # Hardware pin definitions and configuration
│   ├── ble_hid.h             # BLE HID interface
│   ├── deskflow_server.h     # Deskflow client interface
│   ├── device_name.h         # Unique device name generator
│   ├── ethernet_setup.h      # Ethernet initialization
│   ├── ethernet_server_esp32.h # ESP32-specific EthernetServer fix
│   ├── synergy_protocol.h    # Synergy/Barrier protocol implementation
│   └── web_ui.h              # Web dashboard interface
├── src/
│   ├── main.cpp              # Main application entry point
│   ├── ble_hid.cpp           # BLE HID keyboard/mouse implementation
│   ├── deskflow_server.cpp   # Deskflow client and key mapping
│   ├── device_name.cpp       # MAC-based device name generation
│   ├── ethernet_setup.cpp    # W5500 Ethernet initialization
│   ├── synergy_protocol.cpp  # Full Synergy protocol state machine
│   └── web_ui.cpp            # HTTP server and dashboard
├── lib/
│   └── Ethernet/             # Patched Ethernet library for ESP32-S3 W5500 pins
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

## Setup Instructions

### 1. Install PlatformIO

```bash
pip install platformio
```

Or install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code.

### 2. Clone or Download This Repository

```bash
git clone https://github.com/yourusername/esp32-deskflow-client.git
cd esp32-deskflow-client
```

### 3. Configure the Default Server (Optional)

Edit `src/web_ui.cpp` to change the default Deskflow server URL:

```cpp
static const char* DEFAULT_DESKFLOW_URL = "tcp://192.168.1.30:24800";
```

Or leave it as-is and configure via the Web UI after flashing.

### 4. Build the Firmware

```bash
python -m platformio run
```

### 5. Flash to the ESP32-S3

1. Connect the ESP32-S3 via USB-C
2. Put the board in download mode if needed (hold BOOT button while pressing RESET)
3. Flash and open the serial monitor:

```bash
python -m platformio run -t upload -t monitor
```

### 6. Connect Ethernet

Plug an Ethernet cable into the ESP32-S3. The device will:
1. Attempt DHCP to get an IP address
2. Fall back to `192.168.1.177` if DHCP fails

The serial monitor will show the assigned IP address.

### 7. Configure Deskflow Server

#### On the Deskflow/Synergy/Barrier Server:

1. **Disable TLS/SSL** - The ESP32 W5500 doesn't support TLS
   - In Deskflow: Settings → Security → Disable "Require client certificate"
   - In Barrier: Edit → Settings → Uncheck "Enable SSL"
   
2. **Add the ESP32 as a client** with the name shown in the serial output (e.g., `Deskflow-A1B2C3`)

3. **Configure screen position** - Place the ESP32 screen in your layout

### 8. Pair Bluetooth on Target Computer

1. On the target computer, open Bluetooth settings
2. Look for a device named `Deskflow-XXXXXX` (where XXXXXX is from the MAC address)
3. Pair with the device - it will appear as both a keyboard and mouse

### 9. Access the Web UI

Open a browser and navigate to the ESP32's IP address (shown in serial output):

```
http://192.168.1.xxx/
```

The dashboard shows:
- Device name and MAC address
- Current IP address
- BLE connection status
- Deskflow server URL (editable)
- Real-time terminal log for troubleshooting

## Configuration

### config.h Options

| Define | Default | Description |
|--------|---------|-------------|
| `W5500_*_PIN` | Various | SPI pin mapping for W5500 |
| `DESKFLOW_TCP_PORT` | 24800 | Default Synergy/Deskflow port |
| `WEBUI_HTTP_PORT` | 80 | Web dashboard port |
| `BLE_DEVICE_NAME_PREFIX` | "Deskflow-" | BLE device name prefix |
| `ETHERNET_FALLBACK_IP` | 192.168.1.177 | Static IP if DHCP fails |

## Troubleshooting

### Ethernet Issues

| Symptom | Solution |
|---------|----------|
| IP shows as `255.255.255.255` | Check Ethernet cable connection |
| IP shows as `0.0.0.0` | DHCP failed, using fallback IP |
| "W5500: No hardware detected" | Check SPI pin connections |

### Deskflow Connection Issues

| Symptom | Solution |
|---------|----------|
| "failed to accept secure socket" | Disable TLS on the Deskflow server |
| "new client is unresponsive" | Ensure client name matches in server config |
| "Unknown client" | Add the device name to your Deskflow server |
| Repeated disconnects | Check network stability, server logs |

### Bluetooth Issues

| Symptom | Solution |
|---------|----------|
| Device not visible | Ensure no other device is connected, restart ESP32 |
| Keeps connecting/disconnecting | Remove pairing on target, re-pair |
| "Connecting..." hangs | Power cycle the ESP32, try pairing again |

### Keyboard Issues

| Symptom | Solution |
|---------|----------|
| Keys not working | Check serial log for "Unknown key" messages |
| Wrong characters typed | Keyboard layout mismatch (uses US layout) |
| Modifier keys stuck | Release all keys on Deskflow server side |

### Mouse Issues

| Symptom | Solution |
|---------|----------|
| Mouse jumps on first move | Normal - position resets on screen enter |
| Scroll causes page jump | Fixed in latest version |
| Movement stops in one direction | Update to latest firmware |

## Protocol Details

This implementation supports the Synergy/Barrier binary protocol:

### Supported Commands
- `QINF` - Query screen info
- `CIAK` - Info acknowledgment
- `CROP` - Reset options
- `CALV` - Keep-alive
- `CINN` - Enter screen
- `COUT` - Leave screen
- `DMMV` - Mouse move
- `DMDN` - Mouse button down
- `DMUP` - Mouse button up
- `DMWM` - Mouse wheel
- `DKDN` - Key down
- `DKUP` - Key up
- `DKRP` - Key repeat
- `DCLP` - Clipboard (ignored)

### Key Mapping
The firmware converts IBM PC AT scancodes and X11 keysyms to BLE HID keycodes. This includes:
- Standard alphanumeric keys
- Function keys (F1-F12)
- Modifier keys (Shift, Ctrl, Alt, GUI/Win)
- Navigation keys (Home, End, Page Up/Down, Arrows)
- Numpad keys

## Dependencies

All dependencies are managed by PlatformIO and downloaded automatically:

- `links2004/WebSockets` - WebSocket support (unused but included)
- `bblanchon/ArduinoJson` - JSON parsing
- `h2zero/NimBLE-Arduino` - BLE stack
- `Georgegipa/ESP32-BLE-Combo` - Combined BLE keyboard/mouse HID
- Local patched `Ethernet` library - W5500 support for ESP32-S3

## License

MIT License - Feel free to use, modify, and distribute.

## Contributing

Contributions welcome! Please open an issue or pull request.

## Acknowledgments

- [Deskflow](https://github.com/deskflow/deskflow) / [Synergy](https://symless.com/synergy) / [Barrier](https://github.com/debauchee/barrier) for the protocol
- [ESP32-BLE-Combo](https://github.com/Georgegipa/ESP32-BLE-Combo) for the BLE HID implementation
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) for the reliable BLE stack
