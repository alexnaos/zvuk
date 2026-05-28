#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

// Даем доступ к глобальным переменным из main.cpp
extern WebServer server;
extern File uploadFile;
extern int currentVolume;
extern int currentBass;
extern int currentTreble;
extern int currentStationIdx;
extern bool changeStationFlag;
extern String customUrl;

// Подключаем наши строки состояния
extern String currentTrack;
extern String currentStationName;

// Прототипы функций
void loadPlaylist();
void handleStaticFile();
void updateVolume(int vol);
void updateTone(int bass, int treble);

void initWebServer() {
    // API получения расширенного статуса для ползунков и метаданных браузера радио
    server.on("/getstatus", HTTP_GET, []() {
        String track = currentTrack;
        String station = currentStationName;
        track.replace("\"", "\\\"");
        station.replace("\"", "\\\"");

        String json = "{\n";
        json += "  \"vol\":" + String(currentVolume) + ",\n";
        json += "  \"bass\":" + String(currentBass) + ",\n";
        json += "  \"treble\":" + String(currentTreble) + ",\n";
        json += "  \"track\":\"" + track + "\",\n";
        json += "  \"station\":\"" + station + "\"\n";
        json += "}";
        
        server.send(200, "application/json", json);
    });

    // API отдачи текстового контента плейлиста в JS-парсер вашего браузера радио (без изменений)
    server.on("/api/getplaylist", HTTP_GET, []() {
        if (LittleFS.exists("/stations.txt")) {
            File f = LittleFS.open("/stations.txt", "r");
            server.streamFile(f, "text/plain");
            f.close();
        } else {
            server.send(200, "text/plain", "");
        }
    });

    // ЭНДПОИНТ-КОНВЕРТЕР: Разделяет имя и ссылку знаком ТАБУЛЯЦИИ (\t) строго под line.split('\t') в НА
    server.on("/api/ha_playlist", HTTP_GET, []() {
        if (!LittleFS.exists("/stations.txt")) {
            server.send(200, "text/plain", "");
            return;
        }

        File f = LittleFS.open("/stations.txt", "r");
        String output = "";

        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");
            line.trim();

            if (line.length() < 5 || line.startsWith("#")) continue;

            if (line.endsWith(";")) {
                line = line.substring(0, line.length() - 1);
            }

            int separatorIdx = line.indexOf(';');
            if (separatorIdx > 0) {
                String name = line.substring(0, separatorIdx);
                String url = line.substring(separatorIdx + 1);
                name.trim();
                url.trim();

                output += name + "\t" + url + "\n";
            }
        }
        f.close();
        server.send(200, "text/plain", output);
    });

    server.on("/api/vol", HTTP_GET, []() {
        if (server.hasArg("val")) updateVolume(server.arg("val").toInt());
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/tone", HTTP_GET, []() {
        if (server.hasArg("bass") && server.hasArg("treble")) {
            updateTone(server.arg("bass").toInt(), server.arg("treble").toInt());
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/set", HTTP_GET, []() {
        if (server.hasArg("id")) {
            currentStationIdx = server.arg("id").toInt();
            customUrl = "";
            changeStationFlag = true;
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/custom", HTTP_GET, []() {
        if (server.hasArg("url")) {
            customUrl = server.arg("url");
            currentStationIdx = -1;
            changeStationFlag = true;
        }
        server.send(200, "text/plain", "OK");
    });

    // Прием файлов
    server.on("/api/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "OK");
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            uploadFile = LittleFS.open("/stations.txt", "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (uploadFile) {
                uploadFile.close();
                Serial.println("[Файлы]: Большой плейлист успешно сохранен!");
                loadPlaylist();
            }
        }
    });

    server.onNotFound(handleStaticFile);
    server.begin();
    Serial.println("[Веб]: Чистый REST API сервер успешно запущен.");
}

void handleWebRequests() {
    server.handleClient();
}
