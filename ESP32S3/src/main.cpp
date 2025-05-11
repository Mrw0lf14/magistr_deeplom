#include "main.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AsyncWebServer server(80);

// Данные для авторизации
const char* auth_username = "admin";
const char* auth_password = "admin123";
const char* ssid = "Odeyalo";     // Замените на имя вашей WiFi сети
const char* password = "20012005"; // Замените на пароль

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

  // Настройка веб-сервера
  server.serveStatic("/", LittleFS, "/");
  server.serveStatic("/css/", LittleFS, "/css/");
  server.serveStatic("/js/", LittleFS, "/js/");
  listFiles();
  server.begin();
}

void updateBatteryDisplay() {
  uint16_t volt_bat = analogRead(PIN_VBAT);
  uint8_t state_charge = digitalRead(PIN_CHARGE);
  display.clearDisplay();
  
  // Очищаем только область батареи
  display.fillRect(90, 0, 40, 16, SSD1306_BLACK);
  
  // Рисуем иконки
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