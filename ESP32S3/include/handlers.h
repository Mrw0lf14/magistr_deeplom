#ifndef HANDLERS_H
#define HANDLERS_H

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SdFat.h"

// Объявления функций-обработчиков
void handleRoot(AsyncWebServerRequest *request);
void handleLogin(AsyncWebServerRequest *request);
void handleDashboard(AsyncWebServerRequest *request);
void handleBattery(AsyncWebServerRequest *request);
void handleFileList(AsyncWebServerRequest *request);
void handleDownload(AsyncWebServerRequest *request);
void handleDelete(AsyncWebServerRequest *request);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUploadComplete(AsyncWebServerRequest *request);
void handleGetSettings(AsyncWebServerRequest *request);
void handleSaveWiFiSettings(AsyncWebServerRequest *request);
void handleSaveUSBSettings(AsyncWebServerRequest *request);
void handleSavePortsSettings(AsyncWebServerRequest *request);
void handleResetSettings(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);
void handleWifiScan(AsyncWebServerRequest *request);
void handleSettingsPage(AsyncWebServerRequest *request);
void handleLogout(AsyncWebServerRequest *request);

// Обработчик тела запроса (общий для нескольких маршрутов)
void handleRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

#endif