#include "Connectivity.h"
#include "Clock.h"
#include "DataStore.h"
#include "Display.h"
#include "common.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <time.h>
#include <vector>

#if __has_include("ConnectivityConfig.h")
#include "ConnectivityConfig.h"
#endif

#ifndef CONNECTIVITY_WIFI_SSID
#define CONNECTIVITY_WIFI_SSID ""
#endif

#ifndef CONNECTIVITY_WIFI_PASSWORD
#define CONNECTIVITY_WIFI_PASSWORD ""
#endif

#define DEBUG_ENABLE_CONNECTIVITY 1
#if ((DEBUG_ENABLE_CONNECTIVITY == 1) && (ENABLE_DEBUG_OUTPUT == 1))
#define DEBUG_INFO(f, ...) debugPrint("[INF][Connectivity]", f, ##__VA_ARGS__)
#define DEBUG_ERROR(f, ...) debugPrint("[ERR][Connectivity]", f, ##__VA_ARGS__)
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

namespace {

constexpr char kTimeZone[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr uint32_t kWifiConnectTimeoutMillis = 10000;
constexpr uint32_t kTimeSyncTimeoutMillis = 10000;
constexpr int32_t kDailySyncHour = 20;
constexpr uint8_t kServiceButtonPin = 4;
constexpr uint32_t kFileServerDurationMillis = 10 * 60 * 1000;

WebServer fileServer(80);

std::atomic<uint8_t> lastDisconnectReason{0};

const char *wifiStatusName(wl_status_t status) {
  switch (status) {
  case WL_NO_SSID_AVAIL:
    return "SSID unavailable";
  case WL_CONNECTED:
    return "connected";
  case WL_CONNECT_FAILED:
    return "connection failed";
  case WL_CONNECTION_LOST:
    return "connection lost";
  case WL_DISCONNECTED:
    return "disconnected";
  default:
    return "other";
  }
}

bool updateNextSyncTime(time_t &nextSyncTime) {
  const time_t now = time(nullptr);
  struct tm localTime = {};
  if (now < 0 || localtime_r(&now, &localTime) == nullptr) {
    return false;
  }

  ++localTime.tm_mday;
  localTime.tm_hour = kDailySyncHour;
  localTime.tm_min = 0;
  localTime.tm_sec = 0;
  localTime.tm_isdst = -1;
  nextSyncTime = mktime(&localTime);
  return nextSyncTime != static_cast<time_t>(-1);
}

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t index = 0; index < value.length(); ++index) {
    const unsigned char character = value[index];
    switch (character) {
    case '"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (character < 0x20) {
        escaped += ' ';
      } else {
        escaped += static_cast<char>(character);
      }
      break;
    }
  }
  return escaped;
}

String makeFileListJson(const String &path,
                        const std::vector<DataStoreFile> &files) {
  String response;
  response.reserve(48 + path.length() + files.size() * 48);
  response = "{\"path\":\"";
  response += jsonEscape(path);
  response += "\",\"files\":[";
  for (size_t index = 0; index < files.size(); ++index) {
    if (index != 0) {
      response += ',';
    }
    response += "{\"name\":\"";
    response += jsonEscape(files[index].name);
    response += "\",\"size\":";
    response += String(files[index].size);
    response += ",\"isDir\":";
    response += files[index].isDirectory ? "true}" : "false}";
  }
  response += "]}";
  return response;
}

} // namespace

Connectivity &Connectivity::instance() {
  static Connectivity connectivity;
  return connectivity;
}

bool Connectivity::init() {
  pinMode(kServiceButtonPin, INPUT_PULLUP);

  setupFileServer();

  setenv("TZ", kTimeZone, 1);
  tzset();
  sntp_set_time_sync_notification_cb(&Connectivity::onTimeSync);

  Display::instance().drawSyncing();
  DEBUG_INFO("Attempting startup time synchronization ...");
  if (!syncTime()) {
    DEBUG_ERROR("Startup time synchronization failed!");
    return false;
  }

  if (!updateNextSyncTime(nextSyncTime_)) {
    DEBUG_ERROR("Failed to calculate the next time synchronization!");
    return false;
  }

  initialized_ = true;
  startFileServer();
  return true;
}

void Connectivity::run() {
  if (!initialized_) {
    return;
  }

  if (digitalRead(kServiceButtonPin) == LOW) {
    startFileServer();
  }

  if (fileServerActive_) {
    if (static_cast<uint32_t>(millis() - fileServerStartMillis_) >=
        kFileServerDurationMillis) {
      stopFileServer();
    } else {
      fileServer.handleClient();
    }
  }

  if (time(nullptr) < nextSyncTime_) {
    return;
  }

  DEBUG_INFO("Attempting scheduled time synchronization ...");
  syncTime();

  if (!updateNextSyncTime(nextSyncTime_)) {
    DEBUG_ERROR("Failed to calculate the next time synchronization!");
  }
}

void Connectivity::setupFileServer() {
  fileServer.on("/api/files/read", HTTP_GET, []() {
    if (!fileServer.hasArg("path")) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Missing path parameter\"}");
      return;
    }

    const String path = fileServer.arg("path");
    DEBUG_INFO("Requested LittleFS file read: %s", path.c_str());
    std::vector<uint8_t> contents;
    if (!DataStore::instance().readFile(path.c_str(), contents)) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Invalid path or file could not be read\"}");
      return;
    }

    fileServer.setContentLength(contents.size());
    fileServer.send(200, "application/octet-stream", "");
    if (!contents.empty()) {
      fileServer.sendContent(reinterpret_cast<const char *>(contents.data()),
                             contents.size());
    }
  });
  fileServer.on("/api/files", HTTP_GET, []() {
    if (!fileServer.hasArg("path")) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Missing path parameter\"}");
      return;
    }

    const String path = fileServer.arg("path");
    DEBUG_INFO("Requested LittleFS path: %s", path.c_str());
    std::vector<DataStoreFile> files;
    if (!DataStore::instance().listFiles(path.c_str(), files)) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Invalid path or directory unavailable\"}");
      return;
    }
    fileServer.send(200, "application/json", makeFileListJson(path, files));
  });
  fileServer.on("/api/files", HTTP_DELETE, []() {
    if (!fileServer.hasArg("path")) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Missing path parameter\"}");
      return;
    }

    const String path = fileServer.arg("path");
    DEBUG_INFO("Requested LittleFS file deletion: %s", path.c_str());
    if (!DataStore::instance().deleteFile(path.c_str())) {
      fileServer.send(400, "application/json",
                      "{\"error\":\"Invalid path or file deletion failed\"}");
    } else {
      fileServer.send(200, "application/json", "{\"deleted\":true}");
    }
  });
  fileServer.onNotFound([]() {
    fileServer.send(404, "application/json", "{\"error\":\"Not found\"}");
  });
}

void Connectivity::startFileServer() {
  if (!initialized_ || fileServerActive_) {
    return;
  }

  if (!connectToWifi()) {
    return;
  }

  fileServer.begin();
  fileServerStartMillis_ = millis();
  fileServerActive_ = true;
  DEBUG_INFO("File server started at http://%s/api/files?path=/ for 10 minutes",
             WiFi.localIP().toString().c_str());
}

void Connectivity::stopFileServer() {
  fileServer.stop();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  fileServerActive_ = false;
  DEBUG_INFO("File server stopped");
}

bool Connectivity::connectToWifi() {
  ASSERT(CONNECTIVITY_WIFI_SSID[0] != '\0' &&
         CONNECTIVITY_WIFI_PASSWORD[0] != '\0');

  if (!wifiEventHandlerRegistered_) {
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        lastDisconnectReason.store(info.wifi_sta_disconnected.reason);
      }
    });
    wifiEventHandlerRegistered_ = true;
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED) {
    lastDisconnectReason.store(0);
    if (!WiFi.setTxPower(WIFI_POWER_8_5dBm)) {
      DEBUG_ERROR("Failed to set Wi-Fi transmit power to 8.5 dBm!");
    } else {
      DEBUG_INFO("Wi-Fi transmit power set to 8.5 dBm");
    }
    WiFi.begin(CONNECTIVITY_WIFI_SSID, CONNECTIVITY_WIFI_PASSWORD);

    const uint32_t connectStartMillis = millis();
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<uint32_t>(millis() - connectStartMillis) <
               kWifiConnectTimeoutMillis) {
      delay(100);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    const wl_status_t status = WiFi.status();
    const uint8_t reason = lastDisconnectReason.load();
    DEBUG_ERROR("WiFi connection failed: status=%d (%s), reason=%u (%s)",
                status, wifiStatusName(status), reason,
                reason == 0 ? "not reported"
                            : WiFi.disconnectReasonName(
                                  static_cast<wifi_err_reason_t>(reason)));
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  return true;
}

void Connectivity::onTimeSync(struct timeval *) {
  Connectivity::instance().timeSyncReceived_.store(true);
}

bool Connectivity::syncTime() {
  if (!connectToWifi()) {
    DEBUG_ERROR("Time synchronization failed due to Wi-Fi connection failure!");
    return false;
  }

  timeSyncReceived_.store(false);
  configTzTime(kTimeZone, kNtpServer);

  const uint32_t syncStartMillis = millis();
  while (!timeSyncReceived_.load() &&
         static_cast<uint32_t>(millis() - syncStartMillis) <
             kTimeSyncTimeoutMillis) {
    delay(100);
  }

  bool synced = false;
  if (!timeSyncReceived_.load()) {
    DEBUG_ERROR("Timed out waiting for the time server!");
  } else {
    struct tm timeInfo = {};
    if (!getLocalTime(&timeInfo, 1000)) {
      DEBUG_ERROR("Failed to read synchronized time!");
    } else {
      const DateTime dateTime = {.year = timeInfo.tm_year + 1900,
                                 .month = timeInfo.tm_mon + 1,
                                 .day = timeInfo.tm_mday,
                                 .hour = timeInfo.tm_hour,
                                 .minute = timeInfo.tm_min,
                                 .second = timeInfo.tm_sec};
      synced = Clock::instance().setDateTime(dateTime);
      if (!synced) {
        DEBUG_ERROR("Failed to set Clock from the time server!");
      }
    }
  }

  if (!fileServerActive_) {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
  }

  if (synced) {
    DEBUG_INFO(fileServerActive_
                   ? "Time synchronized; WiFi retained for file server."
                   : "Time synchronized; WiFi disabled.");
  } else {
    DEBUG_ERROR(
        fileServerActive_
            ? "Time synchronization failed; WiFi retained for file server."
            : "Time synchronization failed; WiFi disabled.");
  }
  return synced;
}
