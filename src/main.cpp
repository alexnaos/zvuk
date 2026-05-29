#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h> // <-- ПОДКЛЮЧИЛИ АППАРАТНЫЙ WATCHDOG
#include "config.h"
#include "display_module.h"
#include "audio_module.h"
#include "mqtt_module.h"

// Таймер для неблокирующей проверки связи Wi-Fi
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000; // Проверяем сеть раз в 10 секунд

// Указываем компилятору прототипы функций из модуля GyverSettings (веб-сервера)
extern void handleWebRequests(); 
extern void initWebServer();

// Создаем глобальные сетевые объекты для MQTT и Веб-сервера
WiFiClient espClient;
PubSubClient mqttClient(espClient);
File uploadFile;       

// Переменные состояния плейлиста и звука
Station stationList[MAX_STATIONS];
int stationCount = 0;
String currentTrack = "No Title";
String currentStationName = "No Station";

int currentStationIdx = -1;
bool changeStationFlag = false;
String customUrl = "";

void initFileSystem() {
    if (!LittleFS.begin(true)) {
        Serial.println("[Файлы]: Ошибка LittleFS!");
    }
}

void loadPlaylist() {
    stationCount = 0;
    if (!LittleFS.exists("/stations.txt")) {
        Serial.println("[Файлы]: Файл stations.txt не найден.");
        return;
    }

    File file = LittleFS.open("/stations.txt", "r");
    if (!file) return;

    while (file.available() && stationCount < MAX_STATIONS) {
        String line = file.readStringUntil('\n');
        
        // Удаляем скрытые символы возврата каретки \r
        line.replace("\r", ""); 
        line.trim();

        if (line.length() < 5 || line.startsWith("#")) continue;

        int separatorIdx = line.indexOf(';');
        if (separatorIdx > 0) {
            stationList[stationCount].name = line.substring(0, separatorIdx);
            
            String url = line.substring(separatorIdx + 1);
            // Если в конце строки случайно осталась точка с запятой, отрезаем её
            if (url.endsWith(";")) {
                url = url.substring(0, url.length() - 1);
            }
            url.trim();

            stationList[stationCount].url  = url;
            stationCount++;
        }
    }
    file.close();
    Serial.printf("[Файлы]: Плейлист считан. Успешно добавлено станций: %d\n", stationCount);
}

void setup() {
    Serial.begin(115200);
    
    // Инициализируем Аппаратный Watchdog на 8 секунд (перезагрузит чип при зависании)
    esp_task_wdt_init(8, true); 
    esp_task_wdt_add(NULL); // Привязываем WDT к главному циклу loop()
    
    initDisplay();
    initFileSystem();
    loadPlaylist();
    initAudio();

    // НЕБЛОКИРУЮЩИЙ СТАРТ WI-FI (зашиваем автореконнект на уровне SDK)
    WiFi.mode(WIFI_MODE_STA);
    WiFi.setAutoReconnect(true); 
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    Serial.print("[Сеть]: Подключение к Wi-Fi...");
    unsigned long startAttempt = millis();
    
    // Ждем роутер не более 10 секунд. Если он не готов — идем дальше, реконнект будет в loop
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset(); // Продлеваем WDT во время старта
    }

    initWebServer();
    initMQTT(); 
    
    if (stationCount > 0) {
        currentStationIdx = 0;
        changeStationFlag = true;
    }
}

void loop() {
    // СБРОС СТОРОЖЕВОГО ТАЙМЕРА (если loop выполняется — плата жива и не зависла)
    esp_task_wdt_reset();

    // Неблокирующая проверка связи по таймеру раз в 10 секунд
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Сеть]: Связь потеряна! Инициирую мягкое переподключение...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    // Крутим сетевые сервисы только тогда, когда Wi-Fi реально подключен
    if (WiFi.status() == WL_CONNECTED) {
        handleWebRequests();
        loopMQTT(); 
    }

    // Аудиопоток должен кормиться ВСЕГДА, даже если временно отвалилась сеть
    loopAudio(); 

    if (changeStationFlag) {
        changeStationFlag = false;
        if (currentStationIdx >= 0 && currentStationIdx < stationCount) {
            updateDisplay(stationList[currentStationIdx].name.c_str(), "Connecting...");
            startRadioStream(stationList[currentStationIdx].url.c_str());
            updateDisplay(stationList[currentStationIdx].name.c_str(), "PLAYING");
            
            // Записываем имя текущей станции, чтобы веб-интерфейс и MQTT сразу увидели её
            currentStationName = stationList[currentStationIdx].name; 
        } else if (customUrl.length() > 0) {
            updateDisplay("Custom URL", "Connecting...");
            startRadioStream(customUrl.c_str());
            updateDisplay("Custom URL", "PLAYING");
            currentStationName = "Custom URL";
        }
    }
}

// Колбэки Wolle для передачи логов
void vs1053_showstation(const char *info) {
    Serial.printf("[Библиотека]: Станция -> %s\n", info);
    currentStationName = String(info); 
    mqttPublishTrack(info); 
}

void vs1053_showstreamtitle(const char *info) {
    Serial.printf("[Библиотека]: Трек -> %s\n", info);
    currentTrack = String(info);       
    mqttPublishTrack(info); 
    
    if (stationCount > 0 && currentStationIdx >= 0) {
        updateDisplay(stationList[currentStationIdx].name.c_str(), info);
    } else {
        updateDisplay("Custom URL", info);
    }
}
