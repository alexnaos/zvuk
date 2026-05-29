#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "config.h" // Для пинов I2C_SDA и I2C_SCL

// Инициализируем объект дисплея для разрешения 128x64
inline Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Функция инициализации, которую потерял main.cpp
void initDisplay() {
    // Запускаем I2C на ваших пинах (SDA=33, SCL=35)
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Инициализируем сам OLED дисплей по адресу 0x3C
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("[Экран]: Ошибка SSD1306!"));
        return;
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("--- INTERNET RADIO ---");
    display.display();
    Serial.println(F("[Экран]: Успешно запущен."));
}

// Функция конвертации русского языка (чтобы треки читались)
String utf8rus(String source) {
    int i;
    String target = "";
    unsigned char a, b;
    for (i = 0; i < source.length(); i++) {
        a = source[i];
        if (a < 128) {
            target += (char)a;
        } else if (a == 208 || a == 209) {
            b = source[++i];
            if (a == 208 && b == 129) target += (char)161;
            if (a == 209 && b == 145) target += (char)177;
            if (a == 208 && b >= 144 && b <= 191) target += (char)(b + 48);
            if (a == 209 && b >= 128 && b <= 143) target += (char)(b + 112);
        }
    }
    return target;
}


// Функция вывода с крупным треком и временем ЧЧ:ММ
void updateDisplay(const char* station, const char* track) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    
    // --- СТРОКА 1: IP-адрес и Время (Размер 1) ---
    display.setTextSize(1);
    
    struct tm timeinfo;
    char timeStr[6] = "--:--"; // Размер буфера уменьшен под ЧЧ:ММ + нулевой символ
    if (getLocalTime(&timeinfo)) {
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo); // Убрали секунды
    }
    
    String ipStr = "No Wi-Fi";
    if (WiFi.status() == WL_CONNECTED) {
        ipStr = WiFi.localIP().toString();
    }
    
    display.setCursor(0, 0);
    display.print(ipStr);
    
    // Сдвигаем курсор под 5 символов времени (128 ширина - 30 пикселей на текст)
    display.setCursor(98, 0);
    display.print(timeStr);
    
    // Рисуем разделительную линию под первой строкой
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
    
    // --- СТРОКА 2: Станция (Размер 1) ---
    display.setCursor(0, 14);
    display.print("ST: ");
    display.println(utf8rus(String(station)));
    display.drawFastHLine(0, 32, 128, SSD1306_WHITE);
    
    // --- СТРОКА 3: ТРЕК (КРУПНЫЙ, Размер 2) ---
    display.setTextSize(1); // Включаем крупный шрифт
    display.setCursor(0, 38); // Опускаем курсор ниже
    // Так как места мало, префикс "TR:" убираем для экономии пространства
    // Крупный текст автоматически переносится библиотекой Adafruit на следующую строку!
    display.println(utf8rus(String(track)));
    
    display.display();
}
