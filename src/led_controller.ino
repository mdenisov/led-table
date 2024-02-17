/* ================ Настройки ================ */
#define DEBUG_MODE  // Режим отладки
#define HOSTNAME "Led"
#define PREFIX "Led"
#define AP_DEFAULT_SSID "Wi-Fi Led AP"  // Стандартное имя точки доступа ESP (До 20-ти символов)
#define AP_DEFAULT_PASS "00000000"      // Стандартный пароль точки доступа ESP (До 20-ти символов)
#define STA_DEFAULT_SSID ""             // Стандартное имя точки доступа роутера (До 20-ти символов)
#define STA_DEFAULT_PASS ""             // Стандартный пароль точки доступа роутера (До 20-ти символов)
#define WIFI_TIMEOUT 60000              // Таймаут на подключение к Wi-Fi
#define EE_KEY 0x10                     // Ключ EEPROM (1 байт) - измени, чтобы сбросить настройки
#define VERSION 0.1                     // Версия
/* =========================================== */

/* ================ Распиновка =============== */
#define G_PIN 13
#define R_PIN 12
#define B_PIN 14
/* =========================================== */

/* ============ Список библиотек ============= */
#include <Arduino.h>
#include <EEPROM.h>
#include <GRGB.h>
#include <GyverPortal.h>
#include <LittleFS.h>
/* =========================================== */

/* ============ Список объектов ============== */
GRGB led(COMMON_CATHODE, R_PIN, G_PIN, B_PIN, GRGB_10BIT);
GyverPortal ui(&LittleFS);
WiFiClient wifiClient;
/* =========================================== */

/* ================ Макросы ================== */
#ifdef DEBUG_MODE
#define DEBUG(...) Serial.print(__VA_ARGS__)
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG(...)
#define DEBUGLN(...)
#endif
/* =========================================== */

/* ========= Глобальные переменные =========== */
// Структура со всеми настройками
struct {
  char apSsid[21] = AP_DEFAULT_SSID;    // Имя сети для AP режима по умолчанию
  char apPass[21] = AP_DEFAULT_PASS;    // Пароль сети для AP режима по умолчанию
  char staSsid[21] = STA_DEFAULT_SSID;  // Имя сети для STA режима по умолчанию
  char staPass[21] = STA_DEFAULT_PASS;  // Пароль сети для STA режима по умолчанию
  bool staModeEn = false;               // Подключаться к роутеру по умолчанию?
  int color = 0xFFFFFF;                 // Цвет
  int brightness = 130;                 // Яркость
} cfg;
bool connectInProgress = false;
bool ledEnabled = false;
/* =========================================== */

/* =============== Колбэки WiFi ============== */
static WiFiEventHandler staConnectedHandler;
static WiFiEventHandler staDisconnectedHandler;
static WiFiEventHandler staGotIPHandler;
static WiFiEventHandler staDHCPTimeoutHandler;
/* =========================================== */

/* ============== Инициализация ============== */
void initPins() {
  DEBUGLN("Init pins");
}

void initEEPROM() {
  DEBUGLN("Init EEPROM");

  // Инициализация EEPROM
  EEPROM.begin(250);
  delay(50);

  // Если ключ еепром не совпадает
  if (EEPROM.read(0) == EE_KEY) {
    // Читаем настройки
    EEPROM.get(1, cfg);
    delay(50);
  } else {
    // Пишем дефолтные настройки
    resetEEPROM();
  }
}

void initUI() {
  // Подключаем билд веб морды
  ui.attachBuild(build);
  // Подключаем обработчик действий
  ui.attach(action);
  // Стартуем!
  ui.start(HOSTNAME);
  // Включаем ОТА для прошивки по воздуху
  ui.enableOTA();
}

void initFS() {
  // Инициализация файловой системы
  LittleFS.begin();
}

void initLed() {
  // led.setCRT(true);
  led.setPower(true);
}

/* ================== Логика ================= */
void updateEEPROM() {
  // Пишем настройки
  EEPROM.put(1, cfg);
  // Запись
  EEPROM.commit();
  delay(50);
}

void resetEEPROM() {
  DEBUGLN("Reset EEPROM");

  // Пишем ключ
  EEPROM.write(0, EE_KEY);
  updateEEPROM();
}

void setColor(uint32_t color, uint8_t br = cfg.brightness) {
  DEBUG("setColor: ");
  DEBUGLN(color, br);
  led.setHEX(color, br);
}

/* =================== WiFi ================== */
void onStaConnected(const WiFiEventStationModeConnected& evt)
{
  connectInProgress = 0;

  DEBUG("WiFi connected: ");
  DEBUG(evt.ssid);
  DEBUG(" ");
  DEBUGLN(evt.channel);
}

void onStaDisconnected(const WiFiEventStationModeDisconnected& evt)
{
  connectInProgress = 1;

  DEBUG("WiFi disconnected: ");
  DEBUG(evt.ssid);
  DEBUG(" ");
  DEBUGLN(evt.reason);
}

void onStaGotIP(const WiFiEventStationModeGotIP& evt)
{
  connectInProgress = 0;

  DEBUG("Got IP: ");
  DEBUG(evt.ip);
  DEBUG(" ");
  DEBUG(evt.mask);
  DEBUG(" ");
  DEBUGLN(evt.gw);
}

void onStaDHCPTimeout()
{
  DEBUGLN("DHCP Timeout");
}

void setupAP() {
  if (WiFi.getMode() == WIFI_AP) {
    return;
  }

  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.persistent(true);
  WiFi.softAP(cfg.apSsid, cfg.apPass);

  DEBUGLN("Started in AP mode");
  DEBUGLN(WiFi.softAPIP());

  connectInProgress = 0;
}

void setupLocal() {
  if (cfg.staSsid == NULL || cfg.staPass == NULL) {
    DEBUGLN("WiFi not configured");
    setupAP();
  } else {
    DEBUGLN("Connecting WiFi");

    connectInProgress = 1;

    // Включаем wifi
    WiFi.mode(WIFI_STA);
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    // Make sure the wifi does not autoconnect but always reconnects
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(true);

    // WiFi Events
    staConnectedHandler = WiFi.onStationModeConnected(&onStaConnected);
    staDisconnectedHandler = WiFi.onStationModeDisconnected(&onStaDisconnected);
    staGotIPHandler = WiFi.onStationModeGotIP(&onStaGotIP);
    staDHCPTimeoutHandler = WiFi.onStationModeDHCPTimeout(&onStaDHCPTimeout);

    // Задаем сетевое имя
    WiFi.hostname(HOSTNAME);
    // Подключаемся к сети
    WiFi.begin(cfg.staSsid, cfg.staPass, 0, NULL, true);
    delay(2000);
  }
}

void startWiFi() {
  if (cfg.staModeEn) {
    // Подключаемся к роутеру
    setupLocal();
  } else {
    // Режим точки доступа
    setupAP();
  }
}

/* =================== Web =================== */
void build() {           // Билд страницы
  GP.BUILD_BEGIN(400);   // Ширина колонок
  GP.THEME(GP_DARK);     // Темная тема
  GP.PAGE_TITLE("Led");  // Обзываем титл

  if (cfg.staModeEn && WiFi.isConnected()) {
    GP.FORM_BEGIN("/led");    // Начало формы
    GP.GRID_RESPONSIVE(600);  // Отключение респонза при узком экране
    M_BLOCK(                  // Общий блок-колонка для Ленты
      M_BLOCK_TAB("Лента",
                  M_BOX(GP.LABEL("Цвет");
                        GP.COLOR("color", cfg.color););
                  M_BOX(GP.LABEL("Яркость");
                        GP.SLIDER("brightness", cfg.brightness, 0, 255, 1, 0);););
      GP.SUBMIT("Применить");  // Кнопка отправки формы
      GP.FORM_END();           // <- Конец формы (костыль)
    );
  }

  GP.FORM_BEGIN("/cfg");    // Начало формы
  GP.GRID_RESPONSIVE(600);  // Отключение респонза при узком экране
  M_BLOCK(                  // Общий блок-колонка для WiFi
    M_BLOCK_TAB(            // Конфиг для AP режима -> текстбоксы (логин + пароль)
      "AP-Mode",            // Имя + тип DIV
      GP.TEXT("apSsid", "Логин", cfg.apSsid, "", 20);
      GP.BREAK(); GP.PASS_EYE("apPass", "Пароль", cfg.apPass, "", 20);
      GP.BREAK(););
    M_BLOCK_TAB(  // Конфиг для STA режима -> текстбоксы (логин + пароль)
      "WiFi",     // Имя + тип DIV
      GP.TEXT("staSsid", "Логин", cfg.staSsid, "", 20);
      GP.BREAK(); GP.TEXT("staPass", "Пароль", cfg.staPass, "", 20);
      GP.BREAK(); M_BOX(GP_CENTER, GP.LABEL("WiFi Enable");
                        GP.SWITCH("staEn", cfg.staModeEn);););
    GP.SUBMIT("Сохранить");  // Кнопка отправки формы
    GP.FORM_END();           // <- Конец формы (костыль)

    M_BLOCK_TAB(          // Блок с OTA-апдейтом
      "ESP UPDATE",       // Имя + тип DIV
      GP.OTA_FIRMWARE();  // Кнопка с OTA начинкой
    ););
  GP.BUILD_END();  // Конец билда страницы
}

// Подсос значений со страницы
void action(GyverPortal &p) {
  // Если есть сабмит формы - копируем все в переменные
  if (p.form("/led")) {
    DEBUGLN("Led form");

    p.copyInt("color", cfg.color);
    p.copyInt("brightness", cfg.brightness);

    // Сохраняем все настройки в EEPROM
    updateEEPROM();
    delay(50);
    setColor(cfg.color, cfg.brightness);
  }

  if (p.form("/cfg")) {
    DEBUGLN("Cfg form");

    p.copyStr("apSsid", cfg.apSsid);
    p.copyStr("apPass", cfg.apPass);
    p.copyStr("staSsid", cfg.staSsid);
    p.copyStr("staPass", cfg.staPass);
    p.copyBool("staEn", cfg.staModeEn);

    // Сохраняем все настройки в EEPROM
    updateEEPROM();

    // Перегружаем ESP
    ESP.restart();
  }
}

/* ============= Главные функции ============= */
void setup() {
#ifdef DEBUG_MODE
  Serial.begin(115200);
#endif

  initPins();
  initEEPROM();
  startWiFi();
  initLed();
  initUI();
  initFS();
}

void loop() {
  ui.tick();

  // Если долго нет подключения, запускаем AP
  static uint32_t wifiConnectingTmr = millis();
  if (cfg.staModeEn && !WiFi.isConnected()) {
    if (millis() - wifiConnectingTmr > 5 * 60 * 1000) {
      wifiConnectingTmr = millis();
      setupAP();
    }
  }
}
