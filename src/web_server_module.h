#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

void updateVolume(int vol);
void updateTone(int bass, int treble);
void loadPlaylist();

WebServer server(80);
File uploadFile; 

extern int currentStationIdx;
extern bool changeStationFlag;
extern String customUrl;

// Функция отдачи index.html из памяти LittleFS
void handleStaticFile() {
    String path = server.uri();
    if (path == "/") path = "/index.html";
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        String contentType = "text/html";
        server.streamFile(file, contentType);
        file.close();
        return;
    }
    server.send(404, "text/plain", "Not Found");
}

void initWebServer() {
    // API получения статуса для ползунков
    server.on("/getstatus", HTTP_GET, []() {
        String json = "{\"vol\":" + String(currentVolume) + 
                      ",\"bass\":" + String(currentBass) + 
                      ",\"treble\":" + String(currentTreble) + "}";
        server.send(200, "application/json", json);
    });

    // API отдачи текстового контента плейлиста в JS-парсер браузера
    server.on("/api/getplaylist", HTTP_GET, []() {
        if (LittleFS.exists("/stations.txt")) {
            File f = LittleFS.open("/stations.txt", "r");
            server.streamFile(f, "text/plain");
            f.close();
        } else {
            server.send(200, "text/plain", "");
        }
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

    // Надежный прием больших файлов по сетевым пакетам (Исправлено)
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
                Serial.println("[Файлы]: Большой плейлист успешно сохранен в LittleFS!");
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
