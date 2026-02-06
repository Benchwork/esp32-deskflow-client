/**
 * WebUI dashboard — status and logs over HTTP
 * Served on WEBUI_HTTP_PORT.
 */

#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

namespace web_ui {

/** Start HTTP server for dashboard. */
void begin();

/** Serve requests and update. Call from loop. */
void poll();

/** Append a log line (for status page). */
void log(const String& line);

/** Current Deskflow server URL configured via WebUI (may be empty). */
String getDeskflowServerUrl();

} // namespace web_ui

#endif // WEB_UI_H
