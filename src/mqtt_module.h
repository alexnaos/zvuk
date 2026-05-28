#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Функции управления звуком из вашего audio_module
void updateVolume(int vol);
void updateTone(int bass, int treble);

// Подключаем объект плеера Wolle из вашего audio_module для остановки потока
#include "vs1053_ext.h"
extern VS1053* player; 

// Даем доступ к объектам веб-клиента и MQTT (они созданы в main.cpp)
extern WiFiClient espClient;
extern PubSubClient mqttClient;

// Глобальные переменные состояния из main.cpp
extern int currentVolume;
extern int currentBass;
extern int currentTreble;
extern int currentStationIdx;
extern bool changeStationFlag;
extern String customUrl;
extern String currentStationName; 
extern int stationCount;
extern Station stationList[MAX_STATIONS];

// Топики строго по реальному коду интеграции e2002/yoradio
const char* topic_command  = MQTT_BASE_TOPIC "/command";  // Сюда НА шлет текстовые команды
const char* topic_status   = MQTT_BASE_TOPIC "/status";   // Сюда радио шлет статус (JSON)
const char* topic_playlist = MQTT_BASE_TOPIC "/playlist"; // Сюда шлем ссылку на плейлист для НА
const char* topic_volume   = MQTT_BASE_TOPIC "/volume";   // Сюда шлем сырую громкость для volume_listener

String currentMqttTrack = "Radio Ready";
String currentMqttStatus = "playing"; // Может быть "playing" или "stopped"

// Отправка ссылки на плейлист в НА
void mqttPublishPlaylist() {
    if (!mqttClient.connected()) return;
    String playlistUrl = "http://" + WiFi.localIP().toString() + "/api/ha_playlist";
    Serial.printf("[MQTT]: Публикация ссылки плейлиста для НА -> %s\n", playlistUrl.c_str());
    mqttClient.publish(topic_playlist, playlistUrl.c_str(), true);
}

// Отправка статуса и громкости строго по структурам status_listener и volume_listener
void mqttPublishStatus() {
    if (!mqttClient.connected()) return;

    // 1. ОТПРАВЛЯЕМ ГРОМКОСТЬ В ОТДЕЛЬНЫЙ СЫРОЙ ТОПИКОМ (как требует volume_listener)
    int haVolume = (int)((currentVolume * 254) / 21);
    if (haVolume > 254) haVolume = 254;
    if (haVolume < 0) haVolume = 0;
    mqttClient.publish(topic_volume, String(haVolume).c_str(), true);

    // 2. ФОРМИРУЕМ JSON СТРОГО ПО КЛЮЧАМ СТРУКТУРЫ js['title'], js['name'], js['on'], js['status'], js['station']
    String track = currentMqttTrack;
    String station = currentStationName;
    track.replace("\"", "'");
    station.replace("\"", "'");
    
    if (station.length() == 0 || station == "No Station") station = "Sloboda Radio";
    if (track.length() == 0) track = "Radio Ready";

    int currentIdx = (currentStationIdx >= 0) ? currentStationIdx + 1 : 1;
    int isStatusPlaying = (currentMqttStatus == "playing") ? 1 : 0;

    String json = "{";
    json += "\"title\":\"" + track + "\",";      // js['title']
    json += "\"name\":\"" + station + "\",";     // js['name']
    json += "\"on\":1,";                         // js['on']
    json += "\"status\":" + String(isStatusPlaying) + ","; // js['status']
    json += "\"station\":" + String(currentIdx); // js['station']
    json += "}";

    mqttClient.publish(topic_status, json.c_str(), true);
}

// Разбор текстовых команд строго из методов set_command, set_volume, set_source и кнопок НА
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String body = "";
    for (unsigned int i = 0; i < length; i++) body += (char)payload[i];
    body.trim();
    Serial.printf("[MQTT]: Получена текстовая команда -> %s\n", body.c_str());

    // Команда громкости от api.set_volume: "vol X"
    if (body.startsWith("vol ")) {   
        int haVol = body.substring(4).toInt();
        int vsVol = (haVol * 21) / 254;
        updateVolume(vsVol);
    }
    // Команда выбора станции от api.set_source или списка: "play X"
    else if (body.startsWith("play ")) {     
        int targetIdx = body.substring(5).toInt() - 1; // Вычитаем 1, так как у автора список с 1
        if (targetIdx >= 0 && targetIdx < stationCount) {
            currentStationIdx = targetIdx;
            customUrl = "";
            changeStationFlag = true;
            currentMqttStatus = "playing";
        }
    }
    // Кнопка Вперед (async_media_next_track)
    else if (body == "next") {               
        if (stationCount > 0) {
            currentStationIdx = (currentStationIdx + 1) % stationCount;
            customUrl = "";
            changeStationFlag = true;
            currentMqttStatus = "playing";
        }
    }
    // Кнопка Назад (async_media_previous_track)
    else if (body == "prev") {               
        if (stationCount > 0) {
            currentStationIdx = (currentStationIdx - 1 + stationCount) % stationCount;
            customUrl = "";
            changeStationFlag = true;
            currentMqttStatus = "playing";
        }
    }
    // Кнопки Стоп / Пауза (async_media_stop, async_media_pause)
    else if (body == "stop") {
        currentMqttStatus = "stopped";
        changeStationFlag = false;
        if (player != nullptr) player->stop_mp3client(); 
        Serial.println("[MQTT]: Поток остановлен.");
    }
    // Кнопка ИГРАТЬ (async_media_play шлет строго строку "start")
    else if (body == "start" || body == "play") {
        currentMqttStatus = "playing";
        changeStationFlag = true; 
    }
    // Кнопка выключения питания (async_turn_off)
    else if (body == "turnoff") {
        currentMqttStatus = "stopped";
        changeStationFlag = false;
        if (player != nullptr) player->stop_mp3client(); 
    }

    mqttPublishStatus();
}

void connectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("[MQTT]: Подключение к брокеру...");
        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println(" успешно!");
            mqttClient.subscribe(topic_command); 
            mqttPublishPlaylist(); 
            mqttPublishStatus(); 
        } else {
            Serial.printf(" ошибка, rc=%d. Повтор через 5 сек.\n", mqttClient.state());
        }
    }
}

void initMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
}

void loopMQTT() {
    if (!mqttClient.connected()) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            connectMQTT();
        }
    }
    mqttClient.loop();
    
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 10000) {
        lastStatusTime = millis();
        mqttPublishStatus();
    }
}

void mqttPublishTrack(const char* trackInfo) {
    currentMqttTrack = String(trackInfo);
    currentMqttStatus = "playing";
    mqttPublishStatus();
}
