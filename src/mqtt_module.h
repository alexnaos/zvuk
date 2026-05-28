#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

void updateVolume(int vol);
void updateTone(int bass, int treble);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

extern int currentStationIdx;
extern bool changeStationFlag;
extern String customUrl;

// Топики строго по протоколу автора yoradio
const char* topic_sub = MQTT_BASE_TOPIC "/sub"; // Сюда НА шлет команды (JSON)
const char* topic_pub = MQTT_BASE_TOPIC "/pub"; // Сюда радио шлет отчеты (JSON)

// Переменная для хранения названия текущего трека
String currentMqttTrack = "Radio Ready";

// Функция отправки полного JSON статуса в Home Assistant
void mqttPublishStatus() {
    if (!mqttClient.connected()) return;

    // Интеграция yoRadio в НА требует строго такой формат JSON ответа
    String json = "{";
    json += "\"volume\":" + String(currentVolume) + ",";
    json += "\"bass\":" + String(currentBass) + ",";
    json += "\"treble\":" + String(currentTreble) + ",";
    json += "\"status\":\"playing\",";
    json += "\"title\":\"" + currentMqttTrack + "\",";
    json += "\"station\":\"My Mod Radio\"";
    json += "}";

    mqttClient.publish(topic_pub, json.c_str(), true);
}

// Прием JSON команд из карточки Home Assistant
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String body = "";
    for (unsigned int i = 0; i < length; i++) body += (char)payload[i];
    Serial.printf("[MQTT]: Пришла команда -> %s\n", body.c_str());

    // 1. Управление громкостью из ползунка НА
    if (body.indexOf("\"volume\":") >= 0) {
        int idx = body.indexOf("\"volume\":") + 9;
        int val = body.substring(idx).toInt();
        updateVolume(val);
    }

    // 2. Управление Басом / Треблом
    if (body.indexOf("\"bass\":") >= 0 && body.indexOf("\"treble\":") >= 0) {
        int bIdx = body.indexOf("\"bass\":") + 7;
        int tIdx = body.indexOf("\"treble\":") + 9;
        updateTone(body.substring(bIdx).toInt(), body.substring(tIdx).toInt());
    }

    // 3. Переключение станций кнопками Вперед/Назад или из списка в НА
    if (body.indexOf("\"setstation\":") >= 0) {
        int idx = body.indexOf("\"setstation\":") + 13;
        currentStationIdx = body.substring(idx).toInt();
        customUrl = "";
        changeStationFlag = true;
    }
    
    // Мгновенно отправляем отчет в НА, что команда выполнена
    mqttPublishStatus();
}

void connectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("[MQTT]: Подключаюсь к брокеру...");
        
        // КРИТИЧНО: clientId должен строго совпадать с именем в топике!
        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println(" успешно!");
            mqttClient.subscribe(topic_sub);
            mqttPublishStatus(); // Шлем первый статус
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
    
    // Раз в 10 секунд дублируем статус в НА для синхронизации
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 10000) {
        lastStatusTime = millis();
        mqttPublishStatus();
    }
}

// Вызывается из Wolle-колбэков при смене песни
void mqttPublishTrack(const char* trackInfo) {
    currentMqttTrack = String(trackInfo);
    // Заменяем кавычки, чтобы JSON не ломался
    currentMqttTrack.replace("\"", "'"); 
    mqttPublishStatus();
}
