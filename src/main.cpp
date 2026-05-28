#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <WebServer.h> // <-- Добавили для корректной работы объекта WebServer
#include "config.h"
#include "display_module.h"
#include "audio_module.h"
#include "web_server_module.h"
#include "mqtt_module.h"

// Указываем компилятору, что эта функция находится в другом файле (в модуле веб-сервера)
extern void handleWebRequests(); 

// Создаем глобальные сетевые объекты для MQTT и Веб-сервера
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);  // <-- ФИЗИЧЕСКОЕ СОЗДАНИЕ СЕРВЕРА
File uploadFile;       // <-- ФИЗИЧЕСКОЕ СОЗДАНИЕ ОБЪЕКТА ДЛЯ ЗАГРУЗКИ ФАЙЛОВ

// Переменные состояния плейлиста и звука
Station stationList[MAX_STATIONS];
int stationCount = 0;
String currentTrack = "No Title";
String currentStationName = "No Station";

int currentStationIdx = -1;
bool changeStationFlag = false;
String customUrl = "";

// Функция для отдачи статических файлов (HTML/CSS/JS) из LittleFS браузеру
void handleStaticFile() {
    String path = server.uri();
    if (path.endsWith("/")) path += "index.html";
    
    String contentType = "text/html";
    if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "File Not Found в LittleFS");
    }
}

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
    
    initDisplay();
    initFileSystem();
    loadPlaylist();
    initAudio();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    initWebServer();
    initMQTT(); // Инициализируем MQTT
    
    if (stationCount > 0) {
        currentStationIdx = 0;
        changeStationFlag = true;
    }
}

void loop() {
    handleWebRequests();
    loopAudio();
    loopMQTT(); // Синхронный опрос MQTT в одном потоке с музыкой

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
