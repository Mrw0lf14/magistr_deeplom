#include "main.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AsyncWebServer server(80);

SdFat sd;
USBMSC MSC;
// Глобальная переменная для хранения настроек
SystemSettings systemSettings;

// Данные для авторизации
const char* auth_username = "admin";
const char* auth_password = "admin123";
const char* ssid = "applied_robotics";      // Замените на имя вашей WiFi сети
const char* password = "listentome";        // Замените на пароль
uint16_t batteryLevel;
bool isCharging;
bool isDownloading;                         // Флаг скачивания
bool isCardMounted = true;
volatile bool buttonPressed = false;  // Флаг нажатия кнопки
bool showDisplay = true;
unsigned long lastDebounceTime = 0;   // Время последнего нажатия
const unsigned long debounceDelay = 200; // Задержка для антидребезга
// Глобальные счетчики
static uint32_t readCounter = 0, writeCounter = 0, busyCounter = 0;
// Глобальные переменные для USB MSC
bool usbActive = false;
bool usbEnabled = false;

static const uint16_t DISK_SECTOR_SIZE = 512;    // Should be 512
uint32_t sectors = 0;

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

void initUSB_MSC() {
    if(usbActive) return;
    
    // Отключаем SD карту перед активацией USB
    SD.end();
    
    // Инициализация USB MSC
    MSC.onStartStop(onStartStop);
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);
    
    // Получаем количество секторов SD карты
    if(!sd.begin(CS_PIN)) {
        Serial.println("Ошибка инициализации SD для USB MSC");
        return;
    }
    sectors = sd.card()->sectorCount();
    
    MSC.mediaPresent(true);
    MSC.begin(sectors, DISK_SECTOR_SIZE);
    USB.begin();
    
    usbActive = true;
    Serial.println("USB MSC включен");
}

void deinitUSB_MSC() {
    if(!usbActive) return;
    
    // Отключаем USB
    MSC.end();
    // USB.end();
    
    // Переинициализируем SD карту для SPI доступа
    SD.begin(CS_PIN, SPI, 40000000);
    
    usbActive = false;
    Serial.println("USB MSC выключен");
}

bool isUSB_MSC_Active() {
    return usbActive;
}

void setupAPMode() {
    // Отключаем WiFi (если был подключен)
    WiFi.disconnect();
    
    // Переключаем в режим точки доступа
    WiFi.mode(WIFI_AP);
    
    // Настраиваем точку доступа
    WiFi.softAP(systemSettings.ap.ssid, systemSettings.ap.password);
    
    Serial.print("Точка доступа запущена. SSID: ");
    Serial.println(systemSettings.ap.ssid);
    Serial.print("IP адрес: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Пароль: ");
    Serial.println(systemSettings.ap.password);
}

void setupNetwork() {
    if (strcmp(systemSettings.wifi.mode, "ap") == 0) {
        // Режим точки доступа
        setupAPMode();
    } else {
        // Режим клиента (подключение к WiFi)
        WiFi.begin(systemSettings.wifi.ssid, systemSettings.wifi.password);
        Serial.print("Подключение к WiFi");
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nПодключено!");
            Serial.print("IP адрес: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nНе удалось подключиться к WiFi. Переключаемся в режим точки доступа");
            strcpy(systemSettings.wifi.mode, "ap");
            saveSettings();
            setupAPMode();
        }
    }
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

void IRAM_ATTR handleButtonInterrupt() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  
  // Антидребезг - игнорируем нажатия чаще чем debounceDelay
  if (interruptTime - lastInterruptTime > debounceDelay) {
    buttonPressed = true;
    Serial.println("but");
    showDisplay = !showDisplay;
  }
  lastInterruptTime = interruptTime;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CHARGE, INPUT);
  // Настройка кнопки
  pinMode(PIN_BUTTON, INPUT_PULLUP); // Кнопка подключена к GND
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), handleButtonInterrupt, FALLING);
  // Инициализация дисплея
  Wire.begin(PIN_SDA, PIN_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
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

  Serial.printf("Free space: %d bytes\n", LittleFS.totalBytes() - LittleFS.usedBytes());
  
  // В функции setup() после инициализации LittleFS добавьте:
  if (!loadSettings()) {
    Serial.println("Используются настройки по умолчанию");
    setDefaultSettings();
  }
  
  setupNetwork();

  // Маршруты
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin, NULL, handleRequestBody);
  server.on("/dashboard.html", HTTP_GET, handleDashboard);
  server.on("/api/battery", HTTP_GET, handleBattery);
  server.on("/api/files", HTTP_GET, handleFileList);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUpload);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings/wifi", HTTP_POST, handleSaveWiFiSettings, NULL, handleRequestBody);
  server.on("/api/settings/usb", HTTP_POST, handleSaveUSBSettings, NULL, handleRequestBody);
  server.on("/api/settings/ports", HTTP_POST, handleSavePortsSettings, NULL, handleRequestBody);
  server.on("/api/system/reset", HTTP_POST, handleResetSettings);
  server.on("/api/system/reboot", HTTP_POST, handleReboot);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/settings.html", HTTP_GET, handleSettingsPage);
  server.on("/logout", HTTP_GET, handleLogout);
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

  if (!SD.begin(CS_PIN, SPI, 40000000)) {
        Serial.println("Ошибка инициализации SD карты");
        display.setCursor(0, 17);
        display.println("SD Card Error");
        isCardMounted = false;
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

  // Инициализация USB в соответствии с настройками
  if(systemSettings.usb.enabled) {
      initUSB_MSC();
  } else {
    // Инициализируем SD карту для SPI доступа
    if(!SD.begin(CS_PIN, SPI, 40000000)) {
        Serial.println("Ошибка инициализации SD карты");
        isCardMounted = false;
    }
  }

}

void updateStatusDisplay()
{
  if (strcmp(systemSettings.wifi.mode, "station"))
  {
    display.drawBitmap(0, 0, wifi_ap, 15, 16, 1);
    display.setCursor(0, 20);
    display.print("SSID:");
    display.println(systemSettings.ap.ssid);
    display.setCursor(0, 40);
    display.print("IP  :");
    display.println(WiFi.softAPIP());
  }
  else
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      display.drawBitmap(0, 0, wifi_sta_con, 19, 16, 1);
      display.setCursor(0, 20);
      display.print("SSID:");
      display.println(systemSettings.wifi.ssid);
      display.setCursor(0, 40);
      display.print("IP  :");
      display.println(WiFi.localIP());
    }
      
    else
      display.drawBitmap(0, 0, wifi_sta_discon, 19, 16, 1);
  }
  if (isCardMounted)
    display.drawBitmap(20, 0, sd_on, 14, 16, 1);
  else
    display.drawBitmap(20, 0, sd_off, 14, 16, 1);
  if (systemSettings.usb.enabled)
    display.drawBitmap(35, 0, usb_on, 16, 16, 1);
}
void updateBatteryDisplay() {
  uint16_t volt_bat = analogRead(PIN_VBAT);
  uint8_t state_charge = digitalRead(PIN_CHARGE);
  
  batteryLevel = (volt_bat - 3000) * 100 / 450;
  batteryLevel = batteryLevel > 100? 100: batteryLevel;
  isCharging = state_charge == 1 ? true : false;

  display.drawBitmap(90, 0, charge_bmp, 8, 16, (state_charge == 1));
  display.drawBitmap(100, 0, bat_body_bpm, 24, 16, SSD1306_WHITE);
  display.drawBitmap(103, 0, bat_cell_bpm, 8, 16, (volt_bat > 3000));
  display.drawBitmap(108, 0, bat_cell_bpm, 8, 16, (volt_bat > 3100));
  display.drawBitmap(113, 0, bat_cell_bpm, 8, 16, (volt_bat > 3200));
  display.drawBitmap(118, 0, bat_cell_bpm, 8, 16, (volt_bat > 3300));
}

void loop() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10000) { // Раз в 10 секунд
    if (showDisplay)
    {
      display.clearDisplay();
      updateBatteryDisplay();
      updateStatusDisplay();
      display.display();
    }
    lastCheck = millis();
  }
}