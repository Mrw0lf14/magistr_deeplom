#include "handlers.h"
#include "main.h"

// Внешние переменные, объявленные в main.cpp
extern Adafruit_SSD1306 display;
extern SystemSettings systemSettings;
extern bool isDownloading;
extern uint16_t batteryLevel;
extern bool isCharging;
extern AsyncWebServer server;
// Middleware для проверки авторизации
bool checkAuth(AsyncWebServerRequest *request) {
  if(!request->authenticate(auth_username, auth_password)) {
      request->requestAuthentication();
      return false;
  }
  return true;
}

String encryptionTypeToString(wifi_auth_mode_t encryptionType) {
    switch(encryptionType) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise";
        default: return "Unknown";
    }
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
    Serial.println("Login page");
}

void handleLogin(AsyncWebServerRequest *request) {
    Serial.println("\n=== Login Request ===");
    
    if(!request->hasHeader("Content-Type") || 
       request->header("Content-Type").indexOf("application/json") == -1) {
        Serial.println("Invalid Content-Type");
        request->send(400, "application/json", "{\"error\":\"Content-Type must be application/json\"}");
        return;
    }

    if(!request->_tempObject) {
        Serial.println("No body data received");
        request->send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }

    String body = String((char*)request->_tempObject);
    Serial.println("Raw body: " + body);
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if(error) {
        Serial.print("JSON error: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
        return;
    }
    
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    
    Serial.printf("Auth attempt: %s/%s\n", username, password);
    
    if(strcmp(username, auth_username) == 0 && strcmp(password, auth_password) == 0) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(401, "application/json", "{\"success\":false}");
    }
}

void handleDashboard(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/dashboard.html", "text/html");
}

void handleBattery(AsyncWebServerRequest *request) {
    Serial.println("charge response");
    DynamicJsonDocument doc(64);
    doc["level"] = batteryLevel;
    doc["charging"] = isCharging;
    Serial.println(batteryLevel);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleFileList(AsyncWebServerRequest *request) {
    Serial.println("Запрос списка файлов с SD карты");
    if (isDownloading) return;

    String path = "/";
    File root = SD.open(path);
    if(!root || !root.isDirectory()) {
        Serial.println("Ошибка открытия корневой директории SD карты");
        request->send(500, "application/json", "{\"error\":\"Failed to open SD card\"}");
        return;
    }

    DynamicJsonDocument doc(4096);
    JsonArray files = doc.createNestedArray("files");

    File file = root.openNextFile();
    while(file) {
        JsonObject fileInfo = files.createNestedObject();
        String fileName = file.name();
        
        if(fileName.startsWith("/")) {
            fileName = fileName.substring(1);
        }

        fileInfo["name"] = fileName;
        fileInfo["isDir"] = file.isDirectory();
        
        if(!file.isDirectory()) {
            fileInfo["size"] = file.size();
        }
        
        time_t lastWrite = file.getLastWrite();
        if(lastWrite > 0) {
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&lastWrite));
            fileInfo["modified"] = timeStr;
        }
        
        fileInfo["fullPath"] = fileName;
        file = root.openNextFile();
    }
    root.close();

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleDownload(AsyncWebServerRequest *request) {
    isDownloading = true;
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        isDownloading = false;
        return;
    }

    String path = "/" + request->getParam("path")->value();
    Serial.printf("Запрос на скачивание: %s\n", path.c_str());

    if (!SD.exists(path)) {
        request->send(404, "text/plain", "File not found");
        isDownloading = false;
        return;
    }

    File *file = new File(SD.open(path, FILE_READ));
    if (!*file) {
        request->send(500, "text/plain", "Failed to open file");
        isDownloading = false;
        return;
    }

    String filename = path.substring(path.lastIndexOf('/') + 1);
    Serial.printf("Скачивание: %s (%d байт)\n", filename.c_str(), file->size());

    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "application/octet-stream",
        [file, path](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            size_t bytesRead = file->read(buffer, min(maxLen, 2048));
            if (bytesRead == 0) {
                file->close();
                delete file;
                Serial.println("Файл отправлен");
                isDownloading = false;
            }
            return bytesRead;
        }
    );

    response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    request->send(response);
    // Не закрываем file здесь! Он закроется в лямбде.
}

void handleDelete(AsyncWebServerRequest *request) {
    isDownloading = true;
    if (!request->hasParam("path")) {
        request->send(400, "text/plain", "Missing 'path' parameter");
        isDownloading = false;
        return;
    }

    String path = "/" + request->getParam("path")->value();
    Serial.printf("Запрос на удаление: %s\n", path.c_str());

    if (!SD.exists(path)) {
        request->send(404, "text/plain", "File not found");
        isDownloading = false;
        return;
    }
    if (SD.remove(path)) {
        request->send(200, "text/plain", "File deleted");
    } else {
        request->send(500, "text/plain", "Failed to delete file");
    }
    isDownloading = false;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;

    if (index == 0) {
      // Начало загрузки
      Serial.printf("Начало загрузки: %s\n", filename.c_str());
      String path = "/" + filename;
      uploadFile = SD.open(path, FILE_WRITE);
      if (!uploadFile) {
        Serial.println("Ошибка открытия файла для записи");
        return;
      }
      isDownloading = true;
    }

    if (uploadFile && len > 0) {
      uploadFile.write(data, len);
    }

    if (final) {
      if (uploadFile) {
        uploadFile.close();
        Serial.printf("Завершена загрузка: %s, размер: %d байт\n", filename.c_str(), index + len);
    }
    }
}

void handleUploadComplete(AsyncWebServerRequest *request) {
// Этот колбэк вызывается после завершения загрузки файла
    request->send(200, "text/plain", "Файл загружен");
    isDownloading = false;
}

void handleGetSettings(AsyncWebServerRequest *request) {
// if(!checkAuth(request)) return;
  
    DynamicJsonDocument doc(1024);

    // WiFi settings
    doc["wifi"]["mode"] = systemSettings.wifi.mode;
    doc["wifi"]["ssid"] = systemSettings.wifi.ssid;
    doc["wifi"]["password"] = "********"; // Не возвращаем реальный пароль

    // AP settings
    doc["ap"]["ssid"] = systemSettings.ap.ssid;
    doc["ap"]["password"] = "********"; // Не возвращаем реальный пароль

    // USB settings
    doc["usb"]["enabled"] = systemSettings.usb.enabled;

    // Ports settings
    doc["ports"]["port1_enabled"] = systemSettings.ports.port1_enabled;
    doc["ports"]["port2_enabled"] = systemSettings.ports.port2_enabled;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleSaveWiFiSettings(AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;
    if(!request->_tempObject){
    request->send(400, "text/plain", "Bad Request");
    return;
    }

    String body = String((char*)request->_tempObject);
    DynamicJsonDocument doc(512);
    deserializeJson(doc, body);

    // Обновляем WiFi настройки
    strlcpy(systemSettings.wifi.mode, doc["wifi"]["mode"], sizeof(systemSettings.wifi.mode));
    strlcpy(systemSettings.wifi.ssid, doc["wifi"]["ssid"], sizeof(systemSettings.wifi.ssid));
    strlcpy(systemSettings.wifi.password, doc["wifi"]["password"], sizeof(systemSettings.wifi.password));

    // Обновляем AP настройки
    strlcpy(systemSettings.ap.ssid, doc["ap"]["ssid"], sizeof(systemSettings.ap.ssid));
    strlcpy(systemSettings.ap.password, doc["ap"]["password"], sizeof(systemSettings.ap.password));

    // Сохраняем настройки
    if (saveSettings()) {
    Serial.println("Настройки сохранены");
    setupNetwork();
    request->send(200, "text/plain", "OK");
    } else {
    request->send(500, "text/plain", "Failed to save settings");
    }
}
void handleSaveUSBSettings(AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;

    Serial.println("Запрос на сохранение настроек USB");

    String body = String((char*)request->_tempObject);
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    systemSettings.usb.enabled = doc["enabled"];

    if (saveSettings()) {
    request->send(200, "text/plain", "OK");
    Serial.println("Настройки сохранены");
    } else {
    request->send(500, "text/plain", "Failed to save settings");
    }
}

void handleSavePortsSettings(AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;

    // if(request->_tempObject == nullptr) {
    //   request->send(400, "text/plain", "Bad Request");
    //   return;
    // }

    String body = String((char*)request->_tempObject);
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    systemSettings.ports.port1_enabled = doc["port1_enabled"];

    if (saveSettings()) {
    request->send(200, "text/plain", "OK");
    } else {
    request->send(500, "text/plain", "Failed to save settings");
    }
}

void handleResetSettings(AsyncWebServerRequest *request) {
        // if(!checkAuth(request)) return;

    setDefaultSettings();

    request->send(200, "text/plain", "Settings reset to default. Rebooting...");
    delay(1000);
    ESP.restart();
}

void handleReboot(AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;
    
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

void handleWifiScan(AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;
    
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.to<JsonArray>();
    Serial.println("Сканирование WIFI");
    // Пример сканирования WiFi сетей
    int n = WiFi.scanNetworks();
    for(int i = 0; i < n; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["channel"] = WiFi.channel(i);
        network["encryption"] = encryptionTypeToString(WiFi.encryptionType(i));
    }
    
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleSettingsPage(AsyncWebServerRequest *request) {
    // if(checkAuth(request)) {
    request->send(LittleFS, "/settings.html", "text/html");
    // }
}

void handleLogout(AsyncWebServerRequest *request) {
    // Очищаем заголовки авторизации
    request->send(401, "text/plain", "Logged out");
    // Или перенаправляем на страницу входа
    // request->redirect("/index.html");
}
// Остальные обработчики аналогично...

void handleRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if(!request->_tempObject && index == 0) {
        request->_tempObject = malloc(total + 1);
        ((uint8_t*)request->_tempObject)[total] = 0;
    }
    if(request->_tempObject) {
        memcpy((uint8_t*)request->_tempObject + index, data, len);
    }
}