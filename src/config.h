#pragma once
#include <Arduino.h>
#include <LittleFS.h>

// Настройки Wi-Fi
const char* WIFI_SSID     = "Sloboda100";
const char* WIFI_PASSWORD = "2716192023";

// Наша новая аппаратная схема пинов для Lolin S2 Mini
#define PIN_SPI_SCK  7
#define PIN_SPI_MISO 9
#define PIN_SPI_MOSI 11

#define VS1053_CS     12   // XCS
#define VS1053_DCS    14   // XDCS
#define VS1053_DREQ   13   // DREQ
#define VS1053_RST    18   // XRESET

#define I2C_SDA        33
#define I2C_SCL        35


// Структура для динамического списка станций
struct Station {
    String name;
    String url;
};

// Максимально резервируем до 30 станций в памяти
const int MAX_STATIONS = 50;
extern Station stationList[MAX_STATIONS];
extern int stationCount;

// Переменные для хранения настроек звука
extern int currentVolume;
extern int currentBass;
extern int currentTreble;

// Функции для работы с файловой системой
void initFileSystem();
void loadPlaylist();

// --- НАСТРОЙКИ MQTT ДЛЯ ОФИЦИАЛЬНОЙ ИНТЕГРАЦИИ YORADIO ---
const char* MQTT_SERVER   = "192.168.1.23"; // <-- IP-адрес вашего Home Assistant
const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = "";
const char* MQTT_PASSWORD = "";

// Жестко фиксируем базовый топик для интеграции автора yoradio
#define MQTT_CLIENT_ID      "homeradio" // Имя вашего радио
#define MQTT_BASE_TOPIC     "yoradio/homeradio"

