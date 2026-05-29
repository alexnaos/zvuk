#include <Arduino.h>
#include <LittleFS.h>
//#include "config.h"
#include "vs1053_ext.h"
#include <esp_task_wdt.h>


// Подключаем файлы строго по документации (стр. 2 и 14)
#include <GyverDBFile.h>
#include <SettingsGyver.h>
// Структура для динамического списка станций (взамен удаленного config.h)
struct Station {
    String name;
    String url;
};
const int MAX_STATIONS = 50;

// Подключаем внешние переменные и функции управления из других модулей проекта
extern int currentVolume;
extern int currentBass;
extern int currentTreble;
extern int currentStationIdx;
extern bool changeStationFlag;
extern String customUrl;
extern String currentTrack;
extern String currentStationName;
extern int stationCount;
extern Station stationList[MAX_STATIONS];

void updateVolume(int vol);
void updateTone(int bass, int treble);

// Объявляем базу данных и сервер настроек (стр. 14)
GyverDBFile db(&LittleFS, "/db.bin");
SettingsGyver sett("ESP32-S2 Radio", &db);

// Объявляем хэш-ключи для базы данных и виджетов (стр. 14, 24)
DB_KEYS(
    kk,
    vol,      // Громкость
    bass,     // Бас
    treble,   // Высокие частоты
    st_id,    // ID выбранной станции
    c_url,    // Кастомный URL
    txt_st,   // ID для текста станции <-- ДОБАВИТЬ
    txt_tr    // ID для текста трека   <-- ДОБАВИТЬ
);


// Вспомогательная функция для генерации списка станций (стр. 28)
String getStationsOptions() {
    if (stationCount == 0) return "Нет станций";
    String list = "";
    for (int i = 0; i < stationCount; i++) {
        list += stationList[i].name;
        if (i < stationCount - 1) list += ";"; // Разделитель опций в Select — точка с запятой
    }
    return list;
}

// Конструктор интерфейса строго по правилам sets::Builder (стр. 8, 23, 28, 31)
void buildInterface(sets::Builder& b) {
    // Храним выбранную вкладку в статической переменной (стр. 28)
    static uint8_t activeTab = 0;

    // Переключатель вкладок в самом верху страницы (стр. 28)
    if (b.Tabs("Управление;Эквалайзер;Станции", &activeTab)) {
        b.reload(); // Перезагружаем интерфейс для отрисовки выбранной вкладки
        return;
    }

    // Отрисовка виджетов в зависимости от выбранной вкладки
        if (activeTab == 0) {
        // === ВКЛАДКА 1: УПРАВЛЕНИЕ ===
        sets::Group g(b, "Текущий поток");
        
        // Присваиваем ID лейблам, чтобы обновлять их (стр. 4 документации)
        b.Label(kk::txt_st, "Станция", currentStationName);
        b.Label(kk::txt_tr, "Трек", currentTrack);
        
        if (b.Slider(kk::vol, "Громкость", 0, 21, 1)) {
            updateVolume(db[kk::vol].toInt());
        }

        // Ряд кнопок управления
        {
            sets::Buttons btns(b);
            
            if (b.Button("⏮ Назад")) {
                if (stationCount > 0) {
                    currentStationIdx = (currentStationIdx - 1 + stationCount) % stationCount;
                    customUrl = "";
                    changeStationFlag = true;
                }
            }
            // Кнопка Старт/Плей
            if (b.Button("▶ Плей")) {
                if (currentStationIdx >= 0 && currentStationIdx < stationCount) {
                    changeStationFlag = true; // Запускаем текущую выбранную станцию
                }
            }
            if (b.Button("⏹ Стоп")) {
                changeStationFlag = false;
                extern VS1053* player;
                if (player != nullptr) player->stop_mp3client();
            }
            if (b.Button("⏭ Вперед")) {
                if (stationCount > 0) {
                    currentStationIdx = (currentStationIdx + 1) % stationCount;
                    customUrl = "";
                    changeStationFlag = true;
                }
            }
        }
    }
 
    else if (activeTab == 1) {
        // === ВКЛАДКА 2: ЭКВАЛАЙЗЕР ===
        sets::Group g(b, "Настройки тембра");
        bool toneChanged = false;
        
        // Слайдеры низких и высоких частот (стр. 7)
        if (b.Slider(kk::bass, "Усиление НЧ (Бас)", 0, 15, 1)) toneChanged = true;
        if (b.Slider(kk::treble, "Усиление ВЧ (Требл)", -8, 7, 1)) toneChanged = true;

        if (toneChanged) {
            updateTone(db[kk::bass].toInt(), db[kk::treble].toInt());
        }
    } 
    else if (activeTab == 2) {
        // === ВКЛАДКА 3: НАСТРОЙКИ СТАНЦИЙ ===
        sets::Group g(b, "Выбор источника");
        
        // Выпадающий список станций из stations.txt (стр. 8)
        if (b.Select(kk::st_id, "Выбрать станцию", getStationsOptions())) {
            currentStationIdx = db[kk::st_id].toInt();
            customUrl = "";
            changeStationFlag = true;
        }

        // Поле для ввода кастомного URL (стр. 6)
        if (b.Input(kk::c_url, "Кастомный URL")) {
            customUrl = db[kk::c_url].toString();
            if (customUrl.length() > 5) {
                currentStationIdx = -1;
                changeStationFlag = true;
            }
        }
    }
}

// Обработчик скачивания файлов для Home Assistant эндпоинта /api/ha_playlist (стр. 2)

// 1. ПРАВИЛЬНЫЙ ОБРАБОТЧИК ДЛЯ HOME ASSISTANT (БЕЗ ЛИШНИХ СТРОКОВЫХ АРГУМЕНТОВ)
void haPlaylistHandler(Text path) {
    if (path == "/api/ha_playlist") {
        if (!LittleFS.exists("/stations.txt")) return;
        
        File f = LittleFS.open("/stations.txt", "r");
        String txt = "";
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.replace("\r", "");
            line.trim();
            if (line.length() < 5 || line.startsWith("#")) continue;
            if (line.endsWith(";")) line = line.substring(0, line.length() - 1);
            
            int separatorIdx = line.indexOf(';');
            if (separatorIdx > 0) {
                String name = line.substring(0, separatorIdx);
                String url = line.substring(separatorIdx + 1);
                name.trim(); url.trim();
                txt += name + "\t" + url + "\n";
            }
        }
        f.close();
        
        // В этой версии Gyver HTTP-ответ отправляется через внутренний буфер сервера.
        // Если библиотека не предусматривает прямую отправку строки через return,
        // сгенерированный плейлист можно сохранить во временный файл в LittleFS, 
        // чтобы интеграция HA смогла скачать его по этому пути стандартными средствами:
        File out = LittleFS.open("/api/ha_playlist", "w");
        if (out) {
            out.print(txt);
            out.close();
        }
    }
}

// Функция отправки обновлений в браузер по запросу (стр. 44 документации)
void updateInterface(sets::Updater& upd) {
    upd.update(kk::txt_st, currentStationName);
    upd.update(kk::txt_tr, currentTrack);
}


// 2. ИСПРАВЛЕННАЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ ВЕБ-СЕРВЕРА
void initWebServer() {
    // Запуск файловой системы (стр. 14)
    if (!LittleFS.begin(true)) {
        Serial.println("[Файлы]: Ошибка LittleFS!");
    }

    // Запуск базы данных и чтение из файла (стр. 14)
    db.begin();
        // Инициализация базы данных начальными значениями по умолчанию (стр. 14)
    db.init(kk::vol, currentVolume);
    db.init(kk::bass, currentBass);
    db.init(kk::treble, currentTreble);
    db.init(kk::st_id, 0);
    db.init(kk::c_url, "");

    // Синхронизируем переменные радио с прочитанными значениями из сохраненной БД
    currentVolume = db[kk::vol].toInt();
    currentBass = db[kk::bass].toInt();
    currentTreble = db[kk::treble].toInt();
        // Отключаем Watchdog при старте прошивки по воздуху, чтобы избежать ресета
  
    // Запуск сервера GyverSettings (стр. 2)
    sett.begin();
    sett.onBuild(buildInterface);
        sett.onUpdate(updateInterface); // Подключаем живые обновления (стр. 44 документации)

    
    // Теперь сигнатура функцииhaPlaylistHandler строго соответствует void(Text)
    sett.onFetch(haPlaylistHandler); 

    Serial.println("[Веб]: Сервер GyverSettings успешно запущен.");
}

void handleWebRequests() {
    // Перед обработкой сетевого пакета (включая OTA) убираем loop из контроля WDT
    esp_task_wdt_delete(NULL); 
    
    sett.tick(); // Вызываем тикер веб-сервера (здесь происходит прием прошивки)
    
    // После успешной обработки возвращаем контроль ватчдога обратно
    esp_task_wdt_add(NULL); 
}
