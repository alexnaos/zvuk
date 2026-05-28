#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"
#include "display_module.h"
#include "audio_module.h"
#include "web_server_module.h"
#include "mqtt_module.h" // <-- ПОДКЛЮЧИЛИ МОДУЛЬ MQTT

Station stationList[MAX_STATIONS];
int stationCount = 0;

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
        
        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Удаляем скрытые символы возврата каретки \r
        line.replace("\r", ""); 
        line.trim();

        if (line.length() < 5 || line.startsWith("#")) continue;

        int separatorIdx = line.indexOf(';');
        if (separatorIdx > 0) {
            stationList[stationCount].name = line.substring(0, separatorIdx);
            stationList[stationCount].url  = line.substring(separatorIdx + 1);
            stationCount++;
        }
    }
    file.close();
    Serial.printf("[Файлы]: Плейлист считан. Успешно добавлено станций: %d\n", stationCount);
}


void setup() {
    Serial.begin(115200);
    
    initDisplay();
    initFileSystem();
    loadPlaylist();
    initAudio();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    initWebServer();
    initMQTT(); // <-- ИНИЦИАЛИЗИРУЕМ MQTT
    
    if (stationCount > 0) {
        currentStationIdx = 0;
        changeStationFlag = true;
    }
}

void loop() {
    handleWebRequests();
    loopAudio();
    loopMQTT(); // <-- СИНХРОННЫЙ ОПРОС MQTT В ОДНОМ ПОТОКЕ С МУЗЫКОЙ

    if (changeStationFlag) {
        changeStationFlag = false;
        if (currentStationIdx >= 0 && currentStationIdx < stationCount) {
            updateDisplay(stationList[currentStationIdx].name.c_str(), "Connecting...");
            startRadioStream(stationList[currentStationIdx].url.c_str());
            updateDisplay(stationList[currentStationIdx].name.c_str(), "PLAYING");
        } else if (customUrl.length() > 0) {
            updateDisplay("Custom URL", "Connecting...");
            startRadioStream(customUrl.c_str());
            updateDisplay("Custom URL", "PLAYING");
        }
    }
}

// Колбэки Wolle для передачи логов
void vs1053_showstation(const char *info) {
    Serial.printf("[Библиотека]: Станция -> %s\n", info);
    mqttPublishTrack(info); // <-- ШЛЕМ СТАНЦИЮ В HOME ASSISTANT
}

void vs1053_showstreamtitle(const char *info) {
    Serial.printf("[Библиотека]: Трек -> %s\n", info);
    mqttPublishTrack(info); // <-- ШЛЕМ ТЕКУЩИЙ ТРЕК В HOME ASSISTANT
    
    if (stationCount > 0 && currentStationIdx >= 0) {
        updateDisplay(stationList[currentStationIdx].name.c_str(), info);
    } else {
        updateDisplay("Custom URL", info);
    }
}
