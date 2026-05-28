#pragma once
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void initDisplay() {
    Wire.begin(I2C_SDA, I2C_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("❌ Ошибка: Дисплей SSD1306 не найден!");
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Radio Loading...");
    display.display();
}

void updateDisplay(const char* stationName, const char* status) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.println("--- INTERNET RADIO ---");
    display.println("");
    display.setTextSize(2);
    display.println(stationName);
    display.setTextSize(1);
    display.println("");
    display.print("Status: ");
    display.println(status);
    display.display();
}
