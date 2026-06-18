#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>  
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <mcp_can.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>  
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Import Font OLED Resmi
#include <Fonts/FreeSansBold18pt7b.h> 
#include <Fonts/FreeSansBold12pt7b.h> 
#include <Fonts/FreeSansBold9pt7b.h> 

// ================= KONFIGURASI PIN =================
#define SDA_PIN 8       
#define SCL_PIN 9       
#define SPI_SCK 4       
#define SPI_MISO 5      
#define SPI_MOSI 6      
#define MCP_CS_PIN 7    
#define MCP_INT_PIN 10  
#define BUTTON_PIN 3    
#define BUZZER_PIN 2    
#define RELAY_PIN 20     

// ================= KONFIGURASI MEMORI =================
#define EEPROM_ADDR 0x50   
#define ADDR_TRIP_KM 0     
#define ADDR_TRIP_WH 4     

// ================= CONFIGURASI DURASI KEYLESS (KOMUNITAS) =================
// Dideklarasikan sebagai konstanta agar serasi dan mudah diedit oleh komunitas
extern const unsigned long TAG_TIMEOUT_MS;     
extern const unsigned long SHUTDOWN_GRACE_MS;   

// ================= DEKLARASI EXTERN VARIABEL GLOBAL =================
extern const char* FW_VERSION;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

extern const char* SERVICE_UUID;
extern const char* CHARACTERISTIC_UUID;
extern const char* RX_CHAR_UUID;

extern Adafruit_SSD1306 display;
extern RTC_DS1307 rtc;
extern MCP_CAN CAN0;
extern Preferences preferences;

extern BLEServer* pServer;
extern BLECharacteristic* pCharacteristic;
extern BLECharacteristic* pRxCharacteristic;

extern const char* DAY_NAMES[];
extern const char* MONTH_NAMES[];
extern const uint16_t socToBms[101];

extern int rpm; 
extern int speed_kmh;       
extern float calc_speed; 

extern float volts; extern float amps; extern float power_watt; extern int soc;
extern int tempCtrl, tempMotor, tempBatt;
extern String currentMode; extern String lastMode;

extern bool isCharging; extern bool chargerConnected; extern bool oriChargerDetected;
extern float chargerCurrent; 
extern unsigned long lastChargerMsg; extern unsigned long lastOriChargerMsg;

extern float trip_km; extern float trip_wh; extern float last_saved_trip_km;
extern unsigned long lastCalcTime;

extern float valRemainingCapacity; extern float valFullCapacity;
extern int valSOH; extern uint16_t valCycleCount;
extern uint16_t valHighestCellVolt; extern uint8_t valHighestCellNum;
extern uint16_t valLowestCellVolt; extern uint8_t valLowestCellNum; extern uint16_t valAvgCellVolt;
extern uint8_t valMaxTemp, valMaxTempCell, valMinTemp, valMinTempCell;
extern uint8_t valBalanceMode, valBalanceStatus, valBalanceBits[4];
extern uint16_t valCells[23];
extern uint32_t valOdometer;
extern char bmsHwVersion[8]; extern char bmsFwVersion[8];
extern bool bmsChargingFlag;

extern int brakeActive; extern int cruiseActive; extern int standActive;

// Variabel Kontrol Popup Cruise & Failsafe Komunitas
extern bool showCruisePopup;
extern unsigned long lastCruiseChange;
extern unsigned long lastCanPacketTime; 

extern uint32_t canMsgCount; extern uint32_t canMessagesPerSec; extern uint32_t lastSecond;
extern unsigned long heartbeatCounter;

extern int currentPage;
extern unsigned long lastButtonPress, lastModeChange, lastDisplayUpdate;
extern bool showModePopup;

extern String ssid, password, splashText;
extern bool soundEnabled, bleEnabled, inSettingsMode;
extern bool buzzerIsPassive; 
extern bool isBuzzerOn; 
extern int settingsCursor;
extern unsigned long beepEndTime;

extern bool extEepromAvailable;
extern String memoryInUse;
extern bool oledOk, rtcOk, canOk, dashboardRestricted;

extern bool oledSpeedWarnEnable; extern int oledSpeedWarnLimit;
extern bool buzzerSpeedWarnEnable; extern int buzzerSpeedWarnLimit;
extern bool modePopupEnable; extern bool autoSleepEnable;
extern float tripCalibration; 

extern bool keylessEnabled;
extern String registeredTag1;
extern String registeredTag2;
extern unsigned long lastTagSeenTime;
extern unsigned long lastPingTime; 
extern bool inShutdownWarning;                  
extern unsigned long shutdownWarningStartTime;  
extern bool relayState;
extern bool triggerWebScan;
extern bool webScanActive; 
extern unsigned long lastScanTime;
extern bool isScanningActive;
extern int keylessRssiThreshold;

extern bool btnState, lastBtnState;
extern unsigned long btnPressTime;
extern bool isBtnPressed, handled3s, handled5s;

extern bool deviceConnected; extern bool oldDeviceConnected;
extern char bleTxBuf[2200]; extern uint16_t bleTxLen, bleTxOffset;
extern bool bleTxInProgress; extern uint32_t lastDataSend;

// ================= PROTOTIPE FUNGSI UTAMA (NON-INLINE) =================
void showCenteredText(String text, int yPos);
void setBuzzer(bool state, int freq = 3500); 
void playBeep(int duration, int freq = 3500); 
float getSoCFromLookup(uint16_t raw);
void writeEEPROMFloat(unsigned int addr, float val);
float readEEPROMFloat(unsigned int addr);
void initBLE();
void handleBuzzer();
void readCAN();
void handleKeylessScan();
void buildJsonInto();
void handleBLE();
void executeSettingAction();
void updateOLED();
void scanCompleteCB(BLEScanResults results);

// Fungsi yang definisinya berada di dalam berkas sketsa utama (.ino)
void performNtpSync(bool silent);
void checkSerialCommands();

#endif