#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Настройки дисплея SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const uint8_t charge_bmp [] PROGMEM = {
0x06, 0x0A, 0x12, 0x24, 0x44, 0x88, 0xEE, 0x22, 0x44, 0x48, 0x90, 0xA0, 0xC0, 0x00, 0x00, 0x00
};
  
const uint8_t bat_body_bpm [] PROGMEM = {
0x3F, 0xFF, 0xFE, 0x40, 0x00, 0x01, 0x40, 0x00, 0x01, 0x40, 0x00, 0x01, 0xC0, 0x00, 0x01, 0xC0,
0x00, 0x01, 0xC0, 0x00, 0x01, 0xC0, 0x00, 0x01, 0x40, 0x00, 0x01, 0x40, 0x00, 0x01, 0x40, 0x00,
0x01, 0x40, 0x00, 0x01, 0x3F, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t bat_cell_bpm [] PROGMEM = {
  0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(5, INPUT);
  // Инициализация дисплея
  Wire.begin(17, 18); // Настройка I2C
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.println(F("init"));
  display.display();
}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t volt_bat = analogRead(4);
  uint8_t state_charge = digitalRead(5);
  Serial.println(volt_bat);
  uint8_t x = 90;
  uint8_t y = 0;
  display.drawBitmap(x, y, charge_bmp, 8, 16, (state_charge == 0));

  display.drawBitmap(x + 10, y, bat_body_bpm, 24, 16, 1);
  display.drawBitmap(x + 13, y, bat_cell_bpm, 8, 16, (volt_bat > 3200));
  display.drawBitmap(x + 18, y, bat_cell_bpm, 8, 16, (volt_bat > 3500));
  display.drawBitmap(x + 23, y, bat_cell_bpm, 8, 16, (volt_bat > 3800));
  display.drawBitmap(x + 28, y, bat_cell_bpm, 8, 16, (volt_bat > 4000));
  display.display();
  delay(100);
} 