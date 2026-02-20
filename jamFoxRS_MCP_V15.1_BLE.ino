#include <Wire.h>
#include <SPI.h>
#include <mcp_can.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <WiFi.h>
#include "time.h"
#include <Preferences.h>

#include <Fonts/FreeSansBold18pt7b.h> 
#include <Fonts/FreeSansBold12pt7b.h> 
#include <Fonts/FreeSansBold9pt7b.h> 

// ================= LIBRARY BLE BAWAAN RESMI ESP32 =================
// (Sama persis seperti referensi Github Dual-Core Anda)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ================= KONFIGURASI ALAT =================
const char* FW_VERSION = "V15.1"; 

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

// ================= KONFIGURASI NTP & BLE =================
const char* ntpServer  = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; 
const int   daylightOffset_sec = 0;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ================= OBJEK & PREFERENCES =================
Adafruit_SSD1306 display(128, 32, &Wire, -1);
RTC_DS3231 rtc;
MCP_CAN CAN0(MCP_CS_PIN);
Preferences preferences;

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

const char* DAY_NAMES[] = {"MINGGU", "SENIN", "SELASA", "RABU", "KAMIS", "JUMAT", "SABTU"};
const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};

// ================= TABEL LOOKUP SOC =================
const uint16_t socToBms[101] = {
  0, 60,70,80,90,95,105,115,125,135,140,150,160,170,180,185,195,205,215,225,
  230,240,250,260,270,275,285,295,305,315,320,330,340,350,360,365,375,385,395,405,
  410,420,430,440,450,455,465,475,485,495,500,510,520,530,540,550,555,565,575,585,
  590,600,610,620,630,635,645,655,665,675,680,690,700,710,720,725,735,745,755,765,
  770,780,790,800,810,815,825,835,845,855,860,870,880,890,900,905,915,925,935,945,950
};

float getSoCFromLookup(uint16_t raw) {
  if (raw >= socToBms[100]) return 100.0f;
  if (raw <= socToBms[0]) return 0.0f;
  for (int i = 0; i < 100; i++) {
    if (raw >= socToBms[i] && raw <= socToBms[i + 1]) {
      float range = (float)(socToBms[i + 1] - socToBms[i]);
      float delta = (float)(raw - socToBms[i]);
      if (range == 0) return (float)i;
      return (float)i + (delta / range);
    }
  }
  return 0.0f;
}

// ================= VARIABEL DATA UTAMA =================
int rpm = 0; int speed_kmh = 0;
float volts = 0.0; float amps = 0.0; float power_watt = 0.0; int soc = 0;
int tempCtrl = 0, tempMotor = 0, tempBatt = 0;
String currentMode = "PARK"; String lastMode = "PARK";

bool isCharging = false; bool chargerConnected = false; bool oriChargerDetected = false;
float chargerCurrent = 0.0f; 
unsigned long lastChargerMsg = 0; unsigned long lastOriChargerMsg = 0;

// ================= VARIABEL DATA BMS (UNTUK BLE) =================
float valRemainingCapacity = 0.0f; float valFullCapacity = 0.0f;
int valSOH = 0; uint16_t valCycleCount = 0;
uint16_t valHighestCellVolt = 0; uint8_t valHighestCellNum = 0;
uint16_t valLowestCellVolt = 0; uint8_t valLowestCellNum = 0; uint16_t valAvgCellVolt = 0;
uint8_t valMaxTemp = 0, valMaxTempCell = 0, valMinTemp = 0, valMinTempCell = 0;
uint8_t valBalanceMode = 0, valBalanceStatus = 0, valBalanceBits[4] = {0};
uint16_t valCells[23] = {0};
uint32_t valOdometer = 0;
char bmsHwVersion[8] = ""; char bmsFwVersion[8] = "";
bool bmsChargingFlag = false;

uint32_t canMsgCount = 0; uint32_t canMessagesPerSec = 0; uint32_t lastSecond = 0;
unsigned long heartbeatCounter = 0;

// ================= VARIABEL UI & SISTEM =================
int currentPage = 1;
unsigned long lastButtonPress = 0, lastModeChange = 0, lastDisplayUpdate = 0;
bool showModePopup = false;

String ssid = "", password = "", splashText = "";
bool soundEnabled = true, bleEnabled = false, inSettingsMode = false;
int settingsCursor = 0;
unsigned long beepEndTime = 0;

bool btnState = false, lastBtnState = false;
unsigned long btnPressTime = 0;
bool isBtnPressed = false, handled3s = false, handled5s = false;

// ================= BLE STATE VARIABLES =================
bool deviceConnected = false;
bool oldDeviceConnected = false;
char bleTxBuf[2200];
uint16_t bleTxLen = 0, bleTxOffset = 0;
bool bleTxInProgress = false;
uint32_t lastDataSend = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

// ================= DEKLARASI FUNGSI =================
void checkSerialCommands();
void performNtpSync(bool silent);
void handleBuzzer();
void triggerBeep(int duration);
void executeSettingAction();
void initBLE();

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // KUNCI UTAMA: Matikan WiFi agar RAM luas untuk Standard BLE
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MCP_INT_PIN, INPUT_PULLUP); 
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while(1); }

  preferences.begin("cfg", false);

  // SAFE MODE
  if (digitalRead(BUTTON_PIN) == LOW) {
    preferences.putBool("ble", false); 
    display.clearDisplay(); display.setFont(); display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10); display.print("SAFE MODE: BLE OFF");
    display.display();
    delay(2000);
  }

  ssid = preferences.getString("ssid", "pixel8");      
  password = preferences.getString("pass", "1sampai8");
  splashText = preferences.getString("splash", "POLYTRON");
  soundEnabled = preferences.getBool("snd", true);
  bleEnabled = preferences.getBool("ble", false);

  display.clearDisplay(); display.setFont(&FreeSansBold9pt7b); display.setTextColor(SSD1306_WHITE);
  int xPos = (128 - (splashText.length() * 11)) / 2;
  display.setCursor((xPos < 0) ? 0 : xPos, 22); display.print(splashText); display.display();

  rtc.begin(); 
  
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, MCP_CS_PIN);
  if(CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK) {
    CAN0.setMode(MCP_NORMAL); triggerBeep(200); 
  }

  delay(1000); // Splash screen nampang sebentar

  // Jalankan BLE Resmi jika aktif
  if (bleEnabled) {
      initBLE();
  }
}

void initBLE() {
  if (pServer != nullptr) return; 
  
  // Persis seperti referensi komunitas:
  BLEDevice::init("Votol_BLE");
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  
  // Wajib ditambahkan untuk library bawaan agar notify bekerja
  pCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

void performNtpSync(bool silent) {
  if(!silent) {
    display.clearDisplay(); display.setFont(); display.setCursor(0,0); 
    display.println("WIFI SYNCING..."); display.print("SSID: "); display.println(ssid); display.display();
  }

  WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500); wifiTimeout++;
    if(!silent) { display.print("."); display.display(); }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      if(!silent) { display.clearDisplay(); display.setCursor(0,10); display.println("SYNC SUCCESS!"); display.display(); }
    }
  } else {
    if(!silent) { display.clearDisplay(); display.setCursor(0,10); display.println("WIFI FAILED!"); display.display(); }
  }

  if(!silent) delay(1500); else delay(1000); 
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
}

void checkSerialCommands() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); input.trim();
    if (input.startsWith("WIFI,")) {
      int c1 = input.indexOf(','); int c2 = input.indexOf(',', c1 + 1);
      if (c1 > 0 && c2 > 0) {
        preferences.putString("ssid", input.substring(c1 + 1, c2));
        preferences.putString("pass", input.substring(c2 + 1));
        ESP.restart();
      }
    } else if (input.startsWith("SPLASH,")) {
      int c = input.indexOf(',');
      if (c > 0) {
        String s = input.substring(c + 1);
        if (s.length() > 10) s = s.substring(0, 10);
        preferences.putString("splash", s);
        ESP.restart();
      }
    }
  }
}

void triggerBeep(int duration) {
  if(!soundEnabled) return; beepEndTime = millis() + duration;
}

void handleBuzzer() {
  if (!soundEnabled) { digitalWrite(BUZZER_PIN, LOW); return; }
  unsigned long now = millis();
  if (beepEndTime > now) { digitalWrite(BUZZER_PIN, HIGH); return; }
  if (speed_kmh >= 85) { digitalWrite(BUZZER_PIN, ((now % 300) < 100) ? HIGH : LOW); return; }
  if (currentMode == "REVERSE") { digitalWrite(BUZZER_PIN, ((now % 1000) < 300) ? HIGH : LOW); return; }
  digitalWrite(BUZZER_PIN, LOW);
}

// ================= BACA CAN BUS FULL =================
void readCAN() {
  while(CAN0.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int rxId; unsigned char len = 0; unsigned char rxBuf[8];
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
    rxId = rxId & 0x1FFFFFFF;
    canMsgCount++;

    if (rxId == 0x0A010810 && len >= 8) {
      uint8_t m = rxBuf[1];
      if (m == 0x00) currentMode = "PARK"; else if (m == 0x61) currentMode = "CHARGE"; else if (m == 0x70) currentMode = "DRIVE";
      else if (m == 0x50 || m == 0xF0 || m == 0x30 || m == 0xF8) currentMode = "REVERSE";
      else if (m == 0x72 || m == 0xB2) currentMode = "BRAKE"; else if (m == 0xB0) currentMode = "SPORT"; else if (m == 0x78 || m == 0x08) currentMode = "STAND"; 

      if (currentMode != lastMode) {
        lastMode = currentMode;
        if (currentMode != "BRAKE" && currentMode != "STAND") { showModePopup = true; lastModeChange = millis(); }
      }
      rpm = rxBuf[2] | (rxBuf[3] << 8); speed_kmh = (int)(rpm * 0.1033f);
      tempCtrl = rxBuf[4]; tempMotor = rxBuf[5];
    }
    
    if (rxId == 0x0E6C0D09 && len >= 5) tempBatt = (rxBuf[0] + rxBuf[1] + rxBuf[2] + rxBuf[3] + rxBuf[4]) / 5;

    if (rxId == 0x0A6D0D09 && len >= 8) {
      volts = ((rxBuf[0] << 8) | rxBuf[1]) * 0.1f;
      int16_t iRawS = (int16_t)((rxBuf[2] << 8) | rxBuf[3]);
      amps = iRawS * 0.1f; if (abs(amps) < 0.2) amps = 0.0; 
      power_watt = volts * amps;
      valRemainingCapacity = ((rxBuf[4] << 8) | rxBuf[5]) * 0.1f;
      valFullCapacity = ((rxBuf[6] << 8) | rxBuf[7]) * 0.1f;
    }

    if (rxId == 0x0A6E0D09 && len >= 6) {
      soc = (int)getSoCFromLookup((uint16_t)((rxBuf[0] << 8) | rxBuf[1]));
      valSOH = (int)(((rxBuf[2] << 8) | rxBuf[3]) * 0.1f); if (valSOH > 100) valSOH = 100;
      valCycleCount = (rxBuf[4] << 8) | rxBuf[5];
    }

    if (rxId == 0x0A6F0D09 && len >= 8) {
      valHighestCellVolt = (rxBuf[0] << 8) | rxBuf[1]; valHighestCellNum = rxBuf[2];
      valLowestCellVolt = (rxBuf[3] << 8) | rxBuf[4];  valLowestCellNum = rxBuf[5];
      valAvgCellVolt = (rxBuf[6] << 8) | rxBuf[7];
    }

    if (rxId == 0x0A700D09 && len >= 6) {
      valMaxTemp = rxBuf[0]; valMaxTempCell = rxBuf[1];
      valMinTemp = rxBuf[4]; valMinTempCell = rxBuf[5];
    }

    if (rxId == 0x0A730D09 && len >= 6) {
      valBalanceMode = rxBuf[0]; valBalanceStatus = rxBuf[1];
      valBalanceBits[0] = rxBuf[2]; valBalanceBits[1] = rxBuf[3];
      valBalanceBits[2] = rxBuf[4]; valBalanceBits[3] = rxBuf[5];
    }

    if ((rxId & 0xFFF0FFFF) == 0x0E600D09) {
      int baseIndex = -1;
      switch (rxId) {
        case 0x0E640D09: baseIndex = 0; break; case 0x0E650D09: baseIndex = 4; break;
        case 0x0E660D09: baseIndex = 8; break; case 0x0E670D09: baseIndex = 12; break;
        case 0x0E680D09: baseIndex = 16; break; case 0x0E690D09: baseIndex = 20; break;
      }
      if (baseIndex >= 0) {
        for (int i = 0; i < 4 && (baseIndex + i) < 23; i++) {
          int off = i * 2;
          if (off + 1 < len) valCells[baseIndex + i] = (rxBuf[off] << 8) | rxBuf[off + 1];
        }
      }
    }

    if (rxId == 0x0AB40D09 && len >= 1) bmsChargingFlag = (rxBuf[0] == 0x01);
    if (rxId == 0x0A750D09 && len >= 5) { memcpy((void*)bmsHwVersion, rxBuf, 6); bmsHwVersion[6] = '\0'; }
    if (rxId == 0x0A760D09 && len >= 5) { memcpy((void*)bmsFwVersion, rxBuf, 6); bmsFwVersion[6] = '\0'; }

    if ((rxId == 0x1810D0F3 || rxId == 0x1811D0F3) && len >= 5) {
      chargerCurrent = ((uint16_t)((rxBuf[2] << 8) | rxBuf[3])) * 0.1f; 
      chargerConnected = true; lastChargerMsg = millis();
    }
    if (rxId == 0x10261041) { oriChargerDetected = true; lastOriChargerMsg = millis(); }
  }

  if (millis() - lastChargerMsg > 5000) { chargerConnected = false; chargerCurrent = 0.0f; }
  if (millis() - lastOriChargerMsg > 5000) oriChargerDetected = false;
  isCharging = (chargerConnected || oriChargerDetected);

  uint32_t nowSec = millis() / 1000;
  if (nowSec != lastSecond) { canMessagesPerSec = canMsgCount; canMsgCount = 0; lastSecond = nowSec; }
}

// ================= BLE STREAMING LOGIC =================
void buildJsonInto() {
  int cellDelta = 0; uint16_t minCell = 9999, maxCell = 0;
  for (int i = 0; i < 23; i++) {
    if (valCells[i] > 0 && valCells[i] < minCell) minCell = valCells[i];
    if (valCells[i] > maxCell) maxCell = valCells[i];
  }
  if (maxCell >= minCell) cellDelta = (int)maxCell - (int)minCell;

  char balanceCells[100] = {0}; int bpos = 0;
  for (int i = 0; i < 23; i++) {
    bool isBalancing = (valBalanceBits[i / 8] & (1 << (i % 8))) != 0;
    int remain = sizeof(balanceCells) - bpos;
    if (remain > 0) bpos += snprintf(balanceCells + bpos, remain, "%d%s", isBalancing ? 1 : 0, (i < 22) ? "," : "");
  }

  char cellsStr[256] = {0}; int cpos = 0;
  for (int i = 0; i < 23; i++) {
    int remain = sizeof(cellsStr) - cpos;
    if (remain > 0) cpos += snprintf(cellsStr + cpos, remain, "%u%s", valCells[i], (i < 22) ? "," : "");
  }

  bleTxLen = snprintf(bleTxBuf, sizeof(bleTxBuf),
    "{\"rpm\":%d,\"speed\":%d,\"mode\":\"%s\",\"volts\":%.1f,\"amps\":%.1f,\"power\":%.0f,\"soc\":%d,"
    "\"temps\":{\"ctrl\":%d,\"motor\":%d,\"batt\":%d},\"cells\":[%s],\"cellDelta\":%d,"
    "\"canRate\":%lu,\"odometer\":%lu,\"health\":{\"soh\":%d,\"cycles\":%u,\"remainCap\":%.1f,\"fullCap\":%.1f},"
    "\"cellVoltStats\":{\"highest\":%u,\"highestCell\":%u,\"lowest\":%u,\"lowestCell\":%u,\"avg\":%u},"
    "\"tempStats\":{\"max\":%u,\"maxCell\":%u,\"min\":%u,\"minCell\":%u},"
    "\"balance\":{\"mode\":%u,\"status\":%u,\"cells\":[%s]},"
    "\"charger\":{\"on\":%d,\"v\":%.1f,\"a\":%.1f,\"ori\":%d},"
    "\"bms\":{\"hw\":\"%s\",\"fw\":\"%s\"},\"hb\":%lu}\n",
    rpm, speed_kmh, currentMode.c_str(), volts, amps, power_watt, soc,
    tempCtrl, tempMotor, tempBatt, cellsStr, cellDelta,
    (unsigned long)canMessagesPerSec, (unsigned long)valOdometer,
    valSOH, valCycleCount, valRemainingCapacity, valFullCapacity,
    valHighestCellVolt, valHighestCellNum, valLowestCellVolt, valLowestCellNum, valAvgCellVolt,
    valMaxTemp, valMaxTempCell, valMinTemp, valMinTempCell,
    valBalanceMode, valBalanceStatus, balanceCells,
    bmsChargingFlag ? 1 : 0, volts, (chargerCurrent > 0.1f) ? chargerCurrent : fabs(amps),
    oriChargerDetected ? 1 : 0, bmsHwVersion, bmsFwVersion, (unsigned long)heartbeatCounter++
  );
}

void handleBLE() {
  if (!bleEnabled) return;

  if (!deviceConnected && oldDeviceConnected) { delay(50); BLEDevice::startAdvertising(); oldDeviceConnected = deviceConnected; }
  if (deviceConnected && !oldDeviceConnected) { oldDeviceConnected = deviceConnected; }

  if (deviceConnected) {
    uint32_t now = millis();
    if (!bleTxInProgress && (now - lastDataSend >= 200)) { 
      lastDataSend = now;
      buildJsonInto();
      bleTxOffset = 0;
      bleTxInProgress = true;
    }

    if (bleTxInProgress) {
      const uint32_t startUs = micros(); int sent = 0;
      while (bleTxInProgress && sent < 4 && (micros() - startUs) < 10000) {
        int remain = bleTxLen - bleTxOffset;
        if (remain <= 0) { bleTxInProgress = false; break; }
        int chunkLen = (remain > 200) ? 200 : remain; 
        pCharacteristic->setValue((uint8_t*)(bleTxBuf + bleTxOffset), chunkLen);
        pCharacteristic->notify();
        bleTxOffset += chunkLen;
        sent++;
      }
      if (bleTxOffset >= bleTxLen) bleTxInProgress = false;
    }
  }
}

void executeSettingAction() {
  if (settingsCursor == 0) {
    soundEnabled = !soundEnabled; preferences.putBool("snd", soundEnabled);
  } else if (settingsCursor == 1) {
    bleEnabled = !bleEnabled; preferences.putBool("ble", bleEnabled);
    display.clearDisplay(); display.setFont(); display.setTextSize(1);
    display.setCursor(20, 10); display.print("REBOOTING..."); display.display();
    delay(1500); ESP.restart(); 
  } else if (settingsCursor == 2) {
    if (bleEnabled) {
        display.clearDisplay(); display.setFont(); display.setTextSize(1);
        display.setCursor(0, 10); display.print("TURN OFF BLE FIRST!");
        display.display(); delay(2500);
    } else {
        performNtpSync(false); inSettingsMode = false;
    }
  } else if (settingsCursor == 3) {
    inSettingsMode = false; lastButtonPress = millis();
  }
}

void updateOLED() {
  if (bleEnabled && currentPage != 1 && !inSettingsMode) {
      currentPage = 1;
  }

  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);

  if (inSettingsMode) {
    display.setFont(); display.setTextSize(1);
    String opt[4] = {"SOUND  : " + String(soundEnabled ? "ON" : "OFF"), "BLE OUT: " + String(bleEnabled ? "ON" : "OFF"), "SYNC NTP (WIFI)", "EXIT SETTINGS"};
    for(int i = 0; i < 4; i++) {
      display.setCursor(0, i * 8); 
      if (settingsCursor == i) display.print(">"); else display.print(" ");
      display.print(opt[i]); 
    }
    display.display(); return;
  }

  if (isCharging) {
    display.setFont(); bool showAmps = (millis() / 2000) % 2 == 0; 
    display.setTextSize(1); display.setCursor(0, 0);
    display.print(oriChargerDetected ? "ORI CHARGER:" : "FAST CHARGER:");
    display.setTextSize(2); display.setCursor(0, 15); 
    if (showAmps) { display.print("IN: "); display.print((chargerCurrent > 0.1f) ? chargerCurrent : fabs(amps), 1); display.print("A"); } 
    else { display.print("BAT: "); display.print(soc); display.print("%"); }
    display.display(); return;
  }

  if (speed_kmh > 70) {
    display.setFont(&FreeSansBold18pt7b); display.setCursor(0, 28); display.printf("%d", speed_kmh);
    display.setFont(); display.setTextSize(1); display.setCursor(88, 15); display.print("KM/H");
    display.display(); return;
  }

  if (showModePopup) {
    if (millis() - lastModeChange < 3000) { 
      display.setFont(&FreeSansBold9pt7b);
      int xPos = (128 - (currentMode.length() * 11)) / 2;
      display.setCursor((xPos < 0) ? 0 : xPos, 22); display.print(currentMode);
      display.display(); return;
    } else { showModePopup = false; currentPage = 1; }
  }

  display.setFont(); 
  switch (currentPage) {
    case 1: { 
      DateTime dt = rtc.now();
      display.setFont(&FreeSansBold18pt7b); display.setCursor(0, 28); display.printf("%02d:%02d", dt.hour(), dt.minute());
      display.setFont(); display.setTextSize(1);
      display.setCursor(88, 5);  display.print(DAY_NAMES[dt.dayOfTheWeek()]);
      display.setCursor(88, 15); display.printf("%d %s", dt.day(), MONTH_NAMES[dt.month() - 1]);
      
      display.setCursor(88, 25); 
      if (bleEnabled) {
          display.print("BLE ON");
      } else {
          display.printf("%04d", dt.year());
      }
      break;
    }
    case 2: { 
      display.setTextSize(1);
      display.setCursor(0, 4); display.print("ECU"); display.setCursor(43, 4); display.print("MOTOR"); display.setCursor(86, 4); display.print("BATT");
      display.setTextSize(2);
      display.setCursor(0, 16); display.print(tempCtrl); display.setCursor(43, 16); display.print(tempMotor); display.setCursor(86, 16); display.print(tempBatt);
      break;
    }
    case 3: { 
      display.setTextSize(1);
      display.setCursor(8, 4); display.print("VOLT"); display.setCursor(86, 4); display.print("ARUS");
      display.setTextSize(2);
      display.setCursor(0, 16); display.print(volts, 1); display.setTextSize(1); display.setCursor(55, 22); display.print("V");
      display.setTextSize(2);
      display.setCursor(78, 16); display.print(fabs(amps), 1); display.setTextSize(1); display.setCursor(120, 22); display.print("A");
      break;
    }
    case 4: { 
      int pwr = abs((int)power_watt);
      display.setTextSize(2); display.setCursor(10, 4); display.print((power_watt > 0.1f) ? "+" : "-");
      display.setTextSize(3); display.setCursor(32, 2); display.print(pwr);
      display.setTextSize(1); display.setCursor(102, 22); display.print("watt");
      break;
    }
    case 5: {
      display.setTextSize(1);
      display.setCursor(0, 0);  display.print("WIFI: "); display.print(ssid);
      display.setCursor(0, 8);  display.print("PASS: "); display.print(password);
      display.setCursor(0, 16); display.print("NAME: "); display.print(splashText);
      display.setCursor(0, 24); display.print("FW  : "); display.print(FW_VERSION);
      break;
    }
  }
  display.display();
}

// ================= MAIN LOOP =================
void loop() {
  readCAN();
  checkSerialCommands();
  handleBuzzer();
  handleBLE(); 

  btnState = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long now = millis();

  if (btnState && !lastBtnState) {
    btnPressTime = now; isBtnPressed = true; handled3s = false; handled5s = false; triggerBeep(50);
  } else if (!btnState && lastBtnState) {
    isBtnPressed = false; unsigned long duration = now - btnPressTime;
    if (duration < 3000 && duration > 50) {
      if (inSettingsMode) { 
          settingsCursor++; if (settingsCursor > 3) settingsCursor = 0; 
      } else { 
          if (!bleEnabled) {
              currentPage++; if (currentPage > 5) currentPage = 1; 
          } else {
              currentPage = 1; 
          }
          lastButtonPress = now; 
      }
    }
  } else if (isBtnPressed) {
    unsigned long duration = now - btnPressTime;
    if (inSettingsMode) {
      if (duration >= 3000 && !handled3s) { handled3s = true; triggerBeep(100); executeSettingAction(); }
    } else {
      if (duration >= 5000 && !handled5s) { handled5s = true; inSettingsMode = true; settingsCursor = 0; triggerBeep(300); }
    }
  }
  lastBtnState = btnState;

  if (!inSettingsMode && (now - lastButtonPress > 30000) && currentPage != 1 && !showModePopup && !isCharging && speed_kmh <= 70) {
    currentPage = 1;
  }

  if (now - lastDisplayUpdate > 100) { updateOLED(); lastDisplayUpdate = now; }

  delay(5); 
}