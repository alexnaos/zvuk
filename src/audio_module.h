#pragma once
#include <SPI.h>
#include "vs1053_ext.h" 
#include "config.h"

VS1053* player = nullptr;

// Переменные для хранения значений ползунков веб-интерфейса
int currentVolume = 12; // Общая громкость (0-21)
int currentBass   = 0;  // Усиление баса (0-15 дБ)
int currentTreble = 0;  // Усиление требла (-8...7 дБ)

void initAudio() {
    pinMode(VS1053_RST, OUTPUT);
    digitalWrite(VS1053_RST, LOW);
    delay(50);
    digitalWrite(VS1053_RST, HIGH);
    delay(50);

    player = new VS1053(VS1053_CS, VS1053_DCS, VS1053_DREQ, HSPI, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCK);

    player->begin();
    player->setVolume(currentVolume); 
    
    // Передаем дефолтный тембр
    uint8_t toneData[4] = {
        (uint8_t)currentTreble, // Усиление ВЧ (-8...7)
        6,                      // Частота среза ВЧ по умолчанию (6 кГц)
        (uint8_t)currentBass,   // Усиление НЧ (0...15)
        5                       // Частота среза НЧ по умолчанию (50 Гц)
    };
    player->setTone(toneData); 
    
    Serial.println("[Звук]: Модуль VS1053 успешно запущен.");
}

void startRadioStream(const char* url) {
    if (player != nullptr) {
        player->connecttohost(url);
        player->setVolume(currentVolume); 
    }
}

void updateVolume(int vol) {
    if (player != nullptr) {
        currentVolume = constrain(vol, 0, 21);
        player->setVolume(currentVolume);
        Serial.printf("[Звук]: Новая громкость: %d\n", currentVolume);
    }
}

// ИСПРАВЛЕНО: Упаковываем данные эквалайзера в массив из 4 байт по правилам Wolle
void updateTone(int bass, int treble) {
    if (player != nullptr) {
        currentBass = constrain(bass, 0, 15);
        currentTreble = constrain(treble, -8, 7);
        
        uint8_t toneData[4] = {
            (uint8_t)currentTreble, // Treble Gain
            6,                      // Treble Freq (6 kHz)
            (uint8_t)currentBass,   // Bass Gain
            5                       // Bass Freq (50 Hz)
        };
        
        player->setTone(toneData); // Передаем массив в метод setTone
        Serial.printf("[Звук]: Эквалайзер -> БАС: %d, ТРЕБЛ: %d\n", currentBass, currentTreble);
    }
}

void loopAudio() {
    if (player != nullptr) {
        player->loop(); 
    }
}
