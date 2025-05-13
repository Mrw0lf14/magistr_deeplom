#include "main.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AsyncWebServer server(80);

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

  // Запуск WiFi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi");
  
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
            size_t bytesRead = file->read(buffer, min(maxLen, 512)); // Читаем по 512 байт
            if (bytesRead == 0) {
                file->close();
                delete file;
                Serial.println("Файл отправлен");
            }
            return bytesRead;
        }
    );

    response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    request->send(response);
    // Не закрываем file здесь! Он закроется в лямбде.
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
  if (!SD.begin(CS_PIN)) {
        Serial.println("Ошибка инициализации SD карты");
        display.println("SD Card Error");
        display.display();
        return;
    }
    Serial.println("SD карта инициализирована");
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
  updateBatteryDisplay();
  delay(1000); // Обновляем раз в секунду
}