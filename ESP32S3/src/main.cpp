#include "main.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AsyncWebServer server(80);

const char* ssid = "Odeyalo";     // Замените на имя вашей WiFi сети
const char* password = "20012005"; // Замените на пароль

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

  // Настройка маршрутов сервера
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Настройка веб-сервера
  server.serveStatic("/", LittleFS, "/index.html");
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