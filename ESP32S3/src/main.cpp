#include "main.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AsyncWebServer server(80);

// Глобальная переменная для хранения настроек
SystemSettings systemSettings;

// Данные для авторизации
const char* auth_username = "admin";
const char* auth_password = "admin123";
const char* ssid = "applied_robotics";     // Замените на имя вашей WiFi сети
const char* password = "listentome"; // Замените на пароль

// Глобальные счетчики
static uint32_t readCounter = 0, writeCounter = 0, busyCounter = 0;

// Callbacks для USB MSC
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if (sd.card()->isBusy()) busyCounter++;
  while (sd.card()->isBusy());
  return sd.card()->writeSectors(lba, buffer, bufsize / DISK_SECTOR_SIZE) ? bufsize : -1;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (sd.card()->isBusy()) busyCounter++;
  while (sd.card()->isBusy());
  return sd.card()->readSectors(lba, (uint8_t *)buffer, bufsize / DISK_SECTOR_SIZE) ? bufsize : -1;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}

// Обработчики WebServer
void handleRoot();
void handleFileUpload();
void handleFileList();
void handleFileRead();

// Middleware для проверки авторизации
bool checkAuth(AsyncWebServerRequest *request) {
  if(!request->authenticate(auth_username, auth_password)) {
      request->requestAuthentication();
      return false;
  }
  return true;
}

void listFiles() {
  Serial.println("\nLittleFS File List:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while(file) {
    Serial.printf("File: %s, Size: %d\n", file.name(), file.size());
    file = root.openNextFile();
  }
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

// Функция для вычисления CRC32
uint32_t calculateCRC(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }
  return ~crc;
}

// Функция для загрузки настроек из LittleFS
bool loadSettings() {
  File file = LittleFS.open("/settings.dat", "r");
  if (!file) {
    Serial.println("Не удалось открыть файл настроек для чтения");
    return false;
  }

  // Читаем данные
  size_t bytesRead = file.read((uint8_t*)&systemSettings, sizeof(SystemSettings));
  file.close();

  if (bytesRead != sizeof(SystemSettings)) {
    Serial.println("Неверный размер файла настроек");
    return false;
  }

  // Проверяем CRC
  uint32_t savedCrc = systemSettings.crc;
  systemSettings.crc = 0;
  uint32_t calculatedCrc = calculateCRC((uint8_t*)&systemSettings, sizeof(SystemSettings) - sizeof(uint32_t));

  if (savedCrc != calculatedCrc) {
    Serial.println("Ошибка CRC в настройках");
    return false;
  }

  Serial.println("Настройки успешно загружены");
  return true;
}

// Функция для сохранения настроек в LittleFS
bool saveSettings() {
  // Рассчитываем CRC перед сохранением
  systemSettings.crc = 0;
  systemSettings.crc = calculateCRC((uint8_t*)&systemSettings, sizeof(SystemSettings) - sizeof(uint32_t));

  File file = LittleFS.open("/settings.dat", "w");
  if (!file) {
    Serial.println("Не удалось открыть файл настроек для записи");
    return false;
  }

  size_t bytesWritten = file.write((uint8_t*)&systemSettings, sizeof(SystemSettings));
  file.close();

  if (bytesWritten != sizeof(SystemSettings)) {
    Serial.println("Ошибка записи настроек");
    return false;
  }

  Serial.println("Настройки успешно сохранены");
  return true;
}

// Функция для установки настроек по умолчанию
void setDefaultSettings() {
  // WiFi
  strlcpy(systemSettings.wifi.mode, "station", sizeof(systemSettings.wifi.mode));
  strlcpy(systemSettings.wifi.ssid, "", sizeof(systemSettings.wifi.ssid));
  strlcpy(systemSettings.wifi.password, "", sizeof(systemSettings.wifi.password));

  // Точка доступа
  strlcpy(systemSettings.ap.ssid, "ESP32-FileServer", sizeof(systemSettings.ap.ssid));
  strlcpy(systemSettings.ap.password, "admin1234", sizeof(systemSettings.ap.password));

  // USB
  systemSettings.usb.enabled = true;

  // Порты
  systemSettings.ports.port1_enabled = false;
  systemSettings.ports.port2_enabled = false;

  // Сохраняем настройки по умолчанию
  saveSettings();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CHARGE, INPUT);

  // Инициализация дисплея
  Wire.begin(PIN_SDA, PIN_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.println(F("Initializing..."));
  display.display();

  // Инициализация LittleFS
  if(!LittleFS.begin()){
    Serial.println("LittleFS Mount Failed");
    display.println("FS Error");
    display.display();
    return;
  }

  // В функции setup() после инициализации LittleFS добавьте:
  if (!loadSettings()) {
    Serial.println("Используются настройки по умолчанию");
    setDefaultSettings();
  }
  
  // Подключение к WiFi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi");
  
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nПодключено!");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());

  // Маршруты
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
    Serial.println("Login page");
  });
  // Улучшенный обработчик /login
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("\n=== Login Request ===");
    
    // Проверяем Content-Type
    if(!request->hasHeader("Content-Type") || 
       request->header("Content-Type").indexOf("application/json") == -1) {
        Serial.println("Invalid Content-Type");
        request->send(400, "application/json", "{\"error\":\"Content-Type must be application/json\"}");
        return;
    }

    // Получаем тело запроса
    if(!request->_tempObject){
        Serial.println("No body data received");
        request->send(400, "application/json", "{\"error\":\"No data received\"}");
        return;
    }

    String body = String((char*)request->_tempObject);
    Serial.println("Raw body: " + body);
    
    // Парсинг JSON
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if(error){
        Serial.print("JSON error: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON format\"}");
        return;
    }
    
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    
    Serial.printf("Auth attempt: %s/%s\n", username, password);
    
    if(strcmp(username, auth_username) == 0 && strcmp(password, auth_password) == 0){
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(401, "application/json", "{\"success\":false}");
    }
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    // Обработчик для получения тела запроса
    if(!request->_tempObject && index == 0){
        request->_tempObject = malloc(total + 1);
        ((uint8_t*)request->_tempObject)[total] = 0;
    }
    if(request->_tempObject){
        memcpy((uint8_t*)request->_tempObject + index, data, len);
    }
  });
  server.on("/dashboard.html", HTTP_GET, [](AsyncWebServerRequest *request){
    // if(checkAuth(request)) {
    request->send(LittleFS, "/dashboard.html", "text/html");
    // }
  });

  // Обработчик для API батареи
  server.on("/api/battery", HTTP_GET, [](AsyncWebServerRequest *request){
    // if(!checkAuth(request)) return;
    Serial.println("charge response");
    DynamicJsonDocument doc(64);
    doc["level"] = batteryLevel;
    doc["charging"] = isCharging;
    Serial.println(batteryLevel);
    String json;
    serializeJson(doc, json);
    
    request->send(200, "application/json", json);
  });

  // Обработчик для списка файлов с SD карты
server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Запрос списка файлов с SD карты");
    if (isDownloading)
      return;
    // Всегда работаем с корневой директорией
    String path = "/";

    // Открываем корневую директорию на SD карте
    File root = SD.open(path);
    if(!root || !root.isDirectory()){
        Serial.println("Ошибка открытия корневой директории SD карты");
        request->send(500, "application/json", "{\"error\":\"Failed to open SD card\"}");
        return;
    }

    // Создаем JSON ответ
    DynamicJsonDocument doc(4096); // Достаточный размер для списка файлов
    JsonArray files = doc.createNestedArray("files");

    // Перечисляем все файлы и папки
    File file = root.openNextFile();
    while(file){
        JsonObject fileInfo = files.createNestedObject();
        String fileName = file.name();
        
        // Убираем ведущий слеш если есть
        if(fileName.startsWith("/")) {
            fileName = fileName.substring(1);
        }

        fileInfo["name"] = fileName;
        fileInfo["isDir"] = file.isDirectory();
        
        if(!file.isDirectory()) {
            fileInfo["size"] = file.size();
        }
        
        // Время модификации
        time_t lastWrite = file.getLastWrite();
        if(lastWrite > 0){
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
});
  // Обработчик для скачивания файлов с SD карты
server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
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
});

 // Обработчик для скачивания файлов с SD карты
server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) {
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
});

// Обработчик для загрузки файлов на SD карту
server.on("/upload", HTTP_POST, 
  [](AsyncWebServerRequest *request) {
    // Этот колбэк вызывается после завершения загрузки файла
    request->send(200, "text/plain", "Файл загружен");
    isDownloading = false;
  }, 
  [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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
);
// Обработчик для получения текущих настроек
server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
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
});

// Обработчик для сохранения настроек WiFi
server.on("/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
  // if(!checkAuth(request)) return;
  
  if(request->_tempObject == nullptr) {
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
    request->send(200, "text/plain", "OK");
  } else {
    request->send(500, "text/plain", "Failed to save settings");
  }
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Обработчик тела запроса
  if(!request->_tempObject && index == 0){
    request->_tempObject = malloc(total + 1);
    ((uint8_t*)request->_tempObject)[total] = 0;
  }
  if(request->_tempObject){
    memcpy((uint8_t*)request->_tempObject + index, data, len);
  }
});
// Обработчик для сохранения настроек USB
server.on("/api/settings/usb", HTTP_POST, [](AsyncWebServerRequest *request) {
  // if(!checkAuth(request)) return;
  
  if(request->_tempObject == nullptr) {
    request->send(400, "text/plain", "Bad Request");
    return;
  }
  
  String body = String((char*)request->_tempObject);
  DynamicJsonDocument doc(128);
  deserializeJson(doc, body);
  
  systemSettings.usb.enabled = doc["enabled"];
  
  if (saveSettings()) {
    request->send(200, "text/plain", "OK");
  } else {
    request->send(500, "text/plain", "Failed to save settings");
  }
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Аналогичный обработчик тела запроса
});

// Обработчик для сохранения настроек портов
server.on("/api/settings/ports", HTTP_POST, [](AsyncWebServerRequest *request) {
  // if(!checkAuth(request)) return;
  
  if(request->_tempObject == nullptr) {
    request->send(400, "text/plain", "Bad Request");
    return;
  }
  
  String body = String((char*)request->_tempObject);
  DynamicJsonDocument doc(128);
  deserializeJson(doc, body);
  
  systemSettings.ports.port1_enabled = doc["port1_enabled"];
  systemSettings.ports.port2_enabled = doc["port2_enabled"];
  
  if (saveSettings()) {
    request->send(200, "text/plain", "OK");
  } else {
    request->send(500, "text/plain", "Failed to save settings");
  }
}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Аналогичный обработчик тела запроса
});

// Обработчик для сброса настроек
server.on("/api/system/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
  // if(!checkAuth(request)) return;
  
  setDefaultSettings();
  
  request->send(200, "text/plain", "Settings reset to default. Rebooting...");
  delay(1000);
  ESP.restart();
});

server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    // if(!checkAuth(request)) return;
    
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
});

server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
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
});
  server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request){
    // if(checkAuth(request)) {
    request->send(LittleFS, "/settings.html", "text/html");
    // }
  });
  // Настройка веб-сервера
  server.serveStatic("/", LittleFS, "/");
  server.serveStatic("/css/style.css", LittleFS, "/css/style.css", "text/css");
  server.serveStatic("/js/", LittleFS, "/js/");
  listFiles();
  server.begin();

  // Инициализация SD
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  // if (!sd.begin(CS_PIN)) {
  //   Serial.println("SD-карта не найдена!");
  //   display.println(F("ERROR SD"));
  //   display.display();
  //   while (true);
  // }
  // sectors = sd.card()->sectorCount();
  // Serial.printf("SD sectors: %d\n", sectors);
  if (!SD.begin(CS_PIN, SPI, 40000000)) {
        Serial.println("Ошибка инициализации SD карты");
        display.println("SD Card Error");
        display.display();
        return;
    }
    Serial.println("SD карта инициализирована");
    File testFile = SD.open("/speedtest.bin", FILE_WRITE);
  uint8_t buf[512] = {0};
  uint32_t start = millis();
  for (int i = 0; i < 100; i++) {
      testFile.write(buf, sizeof(buf));
  }
  testFile.close();
  Serial.printf("SD write speed: %.2f KB/s\n", 50.0 / ((millis() - start) / 1000.0));
  // // Инициализация USB MSC
  // MSC.onStartStop(onStartStop);
  // MSC.onRead(onRead);
  // MSC.onWrite(onWrite);
  // MSC.mediaPresent(true);
  // MSC.begin(sectors, DISK_SECTOR_SIZE);
  // USB.begin();
}

void updateBatteryDisplay() {
  uint16_t volt_bat = analogRead(PIN_VBAT);
  uint8_t state_charge = digitalRead(PIN_CHARGE);
  display.clearDisplay();
  
  batteryLevel = (volt_bat - 3000) * 100 / 450;
  batteryLevel = batteryLevel > 100? 100: batteryLevel;
  isCharging = state_charge == 1 ? true : false;
  // Очищаем только область батареи
  display.fillRect(90, 0, 40, 16, SSD1306_BLACK);
  
  // Рисуем иконки
  // Serial.println(volt_bat);
  display.drawBitmap(90, 0, charge_bmp, 8, 16, (state_charge == 1));
  display.drawBitmap(100, 0, bat_body_bpm, 24, 16, SSD1306_WHITE);
  display.drawBitmap(103, 0, bat_cell_bpm, 8, 16, (volt_bat > 3000));
  display.drawBitmap(108, 0, bat_cell_bpm, 8, 16, (volt_bat > 3100));
  display.drawBitmap(113, 0, bat_cell_bpm, 8, 16, (volt_bat > 3200));
  display.drawBitmap(118, 0, bat_cell_bpm, 8, 16, (volt_bat > 3300));
  display.display();
}

void loop() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10000) { // Раз в 10 секунд
      updateBatteryDisplay();
      lastCheck = millis();
  }
}