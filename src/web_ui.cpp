/**
 * WebUI — HTTP server and dashboard HTML
 */

#include "../include/config.h"
#include "../include/web_ui.h"
#include "../include/ethernet_server_esp32.h"
#include "../include/device_name.h"
#include "../include/ethernet_setup.h"
#include "../include/ble_hid.h"
#include <Ethernet.h>

namespace web_ui {

static EthernetServerESP32* _httpServer = nullptr;
static String _logLines;
static const size_t MAX_LOG_LINES = 100;
static String _deskflowUrl;
static const char* DEFAULT_DESKFLOW_URL = "tcp://192.168.1.30:24800";

static int fromHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static String urlDecode(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        if (c == '%' && i + 2 < in.length()) {
            int hi = fromHex(in[i + 1]);
            int lo = fromHex(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += char((hi << 4) | lo);
                i += 2;
                continue;
            }
        } else if (c == '+') {
            out += ' ';
            continue;
        }
        out += c;
    }
    return out;
}

void begin() {
    _httpServer = new EthernetServerESP32(WEBUI_HTTP_PORT);
    _httpServer->begin();
    // Set default Deskflow server URL
    _deskflowUrl = DEFAULT_DESKFLOW_URL;
    log("Default Deskflow server: " + _deskflowUrl);
}

void log(const String& line) {
    _logLines += line + "\n";
    int n = 0;
    for (size_t i = 0; i < _logLines.length(); i++)
        if (_logLines[i] == '\n') n++;
    while (n > (int)MAX_LOG_LINES) {
        int idx = _logLines.indexOf('\n');
        if (idx >= 0) _logLines.remove(0, idx + 1);
        n--;
    }
}

static void handleRequestLine(const String& line) {
    // Expect something like: GET /?deskflow=... HTTP/1.1
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0 || secondSpace < 0) return;
    String path = line.substring(firstSpace + 1, secondSpace);
    int qIdx = path.indexOf('?');
    if (qIdx < 0) return;
    String query = path.substring(qIdx + 1);

    // Very small query parser: deskflow=...
    int keyPos = query.indexOf("deskflow=");
    if (keyPos < 0) return;
    String val = query.substring(keyPos + 9);
    int amp = val.indexOf('&');
    if (amp >= 0) val = val.substring(0, amp);

    String newUrl = urlDecode(val);
    newUrl.trim();
    // Only log if URL actually changed
    if (newUrl != _deskflowUrl) {
        _deskflowUrl = newUrl;
        if (_deskflowUrl.length()) {
            log("Deskflow URL set to: " + _deskflowUrl);
        } else {
            log("Deskflow URL cleared");
        }
    }
}

void poll() {
    if (!_httpServer) return;
    EthernetClient client = _httpServer->accept();
    if (!client || !client.connected()) return;
    // Read request header, process first line for query parameters
    String firstLine;
    bool first = true;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line.endsWith("\r")) line.remove(line.length() - 1);
        if (first) {
            firstLine = line;
            handleRequestLine(firstLine);
            first = false;
        }
        if (line.length() == 0) break; // end of headers
    }

    String currentUrl = _deskflowUrl;
    if (!currentUrl.length()) {
        currentUrl = DEFAULT_DESKFLOW_URL;
    }

    // Response: HTML dashboard with two-column layout
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();
    
    // HTML with CSS for two-column layout
    client.println("<!DOCTYPE html><html><head>");
    client.println("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    client.println("<meta http-equiv=\"refresh\" content=\"5\">"); // Auto-refresh every 5 seconds
    client.println("<title>Deskflow Client</title>");
    client.println("<style>");
    client.println("* { box-sizing: border-box; margin: 0; padding: 0; }");
    client.println("body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; }");
    client.println("h1 { color: #00d4ff; margin-bottom: 20px; }");
    client.println("h2 { color: #00d4ff; margin-bottom: 10px; font-size: 1.1em; }");
    client.println(".container { display: flex; gap: 20px; flex-wrap: wrap; }");
    client.println(".panel { background: #16213e; border-radius: 8px; padding: 20px; }");
    client.println(".details { flex: 1; min-width: 300px; }");
    client.println(".terminal { flex: 2; min-width: 400px; }");
    client.println(".info-row { margin: 10px 0; padding: 8px 0; border-bottom: 1px solid #0f3460; }");
    client.println(".info-row b { color: #00d4ff; }");
    client.println(".status-connected { color: #00ff88; }");
    client.println(".status-disconnected { color: #ff6b6b; }");
    client.println("input[type=text] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #0f3460; border-radius: 4px; background: #0f3460; color: #eee; font-size: 14px; }");
    client.println("button { background: #00d4ff; color: #1a1a2e; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; font-weight: bold; }");
    client.println("button:hover { background: #00b8e6; }");
    client.println(".log-area { background: #0d1117; border: 1px solid #30363d; border-radius: 4px; padding: 15px; height: 400px; overflow-y: auto; font-family: 'Consolas', 'Monaco', monospace; font-size: 12px; line-height: 1.5; color: #c9d1d9; white-space: pre-wrap; word-wrap: break-word; }");
    client.println(".log-area::-webkit-scrollbar { width: 8px; }");
    client.println(".log-area::-webkit-scrollbar-track { background: #0d1117; }");
    client.println(".log-area::-webkit-scrollbar-thumb { background: #30363d; border-radius: 4px; }");
    client.println("</style>");
    client.println("</head><body>");
    
    client.println("<h1>Deskflow Client</h1>");
    client.println("<div class=\"container\">");
    
    // Left panel - Details
    client.println("<div class=\"panel details\">");
    client.println("<h2>Device Information</h2>");
    client.print("<div class=\"info-row\"><b>Device:</b> "); client.print(device_name::get()); client.println("</div>");
    client.print("<div class=\"info-row\"><b>MAC:</b> "); client.print(device_name::getMacString()); client.println("</div>");
    client.print("<div class=\"info-row\"><b>IP:</b> "); client.print(ethernet::getLocalIP().toString()); client.println("</div>");
    client.print("<div class=\"info-row\"><b>BLE HID:</b> <span class=\"");
    client.print(ble_hid::isConnected() ? "status-connected\">Connected" : "status-disconnected\">Disconnected");
    client.println("</span></div>");
    client.print("<div class=\"info-row\"><b>Deskflow Server:</b> "); client.print(currentUrl); client.println("</div>");
    
    client.println("<h2 style=\"margin-top:20px\">Configuration</h2>");
    client.println("<form method=\"GET\" action=\"/\">");
    client.println("<label for=\"deskflow\">Deskflow Server URL:</label>");
    client.print("<input type=\"text\" id=\"deskflow\" name=\"deskflow\" value=\"");
    client.print(currentUrl);
    client.println("\">");
    client.println("<button type=\"submit\">Save</button>");
    client.println("</form>");
    client.println("</div>");
    
    // Right panel - Terminal Log
    client.println("<div class=\"panel terminal\">");
    client.println("<h2>Terminal Log</h2>");
    client.println("<div class=\"log-area\" id=\"logArea\">");
    client.print(_logLines.length() ? _logLines : "(no logs yet)");
    client.println("</div>");
    client.println("</div>");
    
    client.println("</div>"); // container
    
    // JavaScript to scroll log to bottom
    client.println("<script>");
    client.println("var logArea = document.getElementById('logArea');");
    client.println("logArea.scrollTop = logArea.scrollHeight;");
    client.println("</script>");
    
    client.println("</body></html>");
    client.stop();
}

String getDeskflowServerUrl() {
    return _deskflowUrl;
}

} // namespace web_ui
