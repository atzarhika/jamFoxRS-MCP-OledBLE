#include "Config.h"
#include "BuzzerSystem.h"
#include "EepromSystem.h"
#include "CanSystem.h"
#include "BleKeyless.h"
#include "DisplaySystem.h"

// ================= ALOKASI & DEFINISI VARIABEL GLOBAL SEBENARNYA =================
const char* FW_VERSION = "V15.50-beta"; // Evaluasi 6: Firmware Versi Terbaru Komunitas
const char* ntpServer  = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600; 
const int   daylightOffset_sec = 0;

const char* SERVICE_UUID        = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char* CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const char* RX_CHAR_UUID        = "beb5483e-36e1-4688-b7f5-ea07361b26a9";

// Alokasi memori konfigurasi waktu Keyless (Konstanta Fisik default total: 20 detik)
const unsigned long TAG_TIMEOUT_MS     = 10000; // Durasi jangkauan tag hilang sebelum warning (Default: 10 detik)
const unsigned long SHUTDOWN_GRACE_MS   = 10000; // Durasi masa tenggang visual OLED (Default: 10 detik)

Adafruit_SSD1306 display(128, 32, &Wire, -1);
RTC_DS3231 rtc; 
MCP_CAN CAN0(MCP_CS_PIN);
Preferences preferences;

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLECharacteristic* pRxCharacteristic = nullptr;

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

int rpm = 0; int speed_kmh = 0; float calc_speed = 0.0f;
float volts = 0.0; float amps = 0.0; float power_watt = 0.0; int soc = 0;
int tempCtrl = 0, tempMotor = 0, tempBatt = 0;
String currentMode = "PARK"; String lastMode = "PARK";

bool isCharging = false; bool chargerConnected = false; bool oriChargerDetected = false;
float chargerCurrent = 0.0f; 
unsigned long lastChargerMsg = 0; unsigned long lastOriChargerMsg = 0;

float trip_km = 0.0; float trip_wh = 0.0; float last_saved_trip_km = 0.0;
unsigned long lastCalcTime = 0;

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

int brakeActive = 0; int cruiseActive = 0; int standActive = 0;

// Alokasi memori global fitur-fitur baru
bool showCruisePopup = false;
unsigned long lastCruiseChange = 0;
unsigned long lastCanPacketTime = 0; 

uint32_t canMsgCount = 0; uint32_t canMessagesPerSec = 0; uint32_t lastSecond = 0;
unsigned long heartbeatCounter = 0;

int currentPage = 1;
unsigned long lastButtonPress = 0, lastModeChange = 0, lastDisplayUpdate = 0;
bool showModePopup = false;

String ssid = "", password = "", splashText = "";
bool soundEnabled = true, bleEnabled = false, inSettingsMode = false;
bool buzzerIsPassive = false; bool isBuzzerOn = false; 
int settingsCursor = 0; unsigned long beepEndTime = 0;

bool extEepromAvailable = false; String memoryInUse = "INTERNAL";
bool oledOk = false; bool rtcOk = false; bool canOk = false; bool dashboardRestricted = false;

bool oledSpeedWarnEnable = true; int oledSpeedWarnLimit = 70;
bool buzzerSpeedWarnEnable = true; int buzzerSpeedWarnLimit = 80;
bool modePopupEnable = true; bool autoSleepEnable = true;
float tripCalibration = 1.0f; 

// Alokasi memori global fitur Keyless
bool keylessEnabled = false;
String registeredTag1 = "";
String registeredTag2 = "";
unsigned long lastTagSeenTime = 0;
unsigned long lastPingTime = 0; 
bool inShutdownWarning = false;                 
unsigned long shutdownWarningStartTime = 0;     
bool relayState = false;
bool triggerWebScan = false;
bool webScanActive = false; 
unsigned long lastScanTime = 0;
bool isScanningActive = false;
int keylessRssiThreshold = -80;

bool btnState = false, lastBtnState = false; unsigned long btnPressTime = 0;
bool isBtnPressed = false, handled3s = false, handled5s = false;

bool deviceConnected = false; bool oldDeviceConnected = false;
char bleTxBuf[2200]; uint16_t bleTxLen = 0, bleTxOffset = 0;
bool bleTxInProgress = false; uint32_t lastDataSend = 0;

// ================= IMPLEMENTASI FUNGSI SISTEM (DEFINISI) =================

void performNtpSync(bool silent) {
  if(oledOk && !silent) { display.clearDisplay(); display.setFont(); display.setCursor(0,0); display.println("WIFI SYNCING..."); display.print("SSID: "); display.println(ssid); display.display(); }
  WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) { delay(500); wifiTimeout++; if(oledOk && !silent) { display.print("."); display.display(); } }
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); struct tm timeinfo;
    if (getLocalTime(&timeinfo)) { rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec)); if(oledOk && !silent) { display.clearDisplay(); display.setCursor(0,10); display.println("SYNC SUCCESS!"); display.display(); playBeep(200, 3500); } }
  } else {
    if(oledOk && !silent) { display.clearDisplay(); display.setCursor(0,10); display.println("WIFI FAILED!"); display.display(); playBeep(500, 1000); }
  }
  if(!silent) delay(1500); else delay(1000); 
  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
}

void checkSerialCommands() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); input.trim();
    if (input.startsWith("WIFI,")) { 
        int c1 = input.indexOf(','); 
        int c2 = input.indexOf(',', c1 + 1); 
        if (c1 > 0 && c2 > 0) { 
            preferences.putString("ssid", input.substring(c1 + 1, c2)); 
            preferences.putString("pass", input.substring(c2 + 1)); 
            ESP.restart(); 
        } 
    }
    else if (input.startsWith("SPLASH,")) { 
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

// ================= SETUP UTAMA =================
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); 
  
  delay(150); 
  
  // --- HARDWARE CEK: SAFE MODE ---
  if (digitalRead(BUTTON_PIN) == LOW) {
      Preferences prefs;
      prefs.begin("cfg", false);
      prefs.putBool("ble", false);       
      prefs.putBool("keylessEn", false); 
      prefs.putString("tag1", "");       
      prefs.putString("tag2", "");       
      prefs.end();

      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;

      Wire.begin(SDA_PIN, SCL_PIN);
      if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SSD1306_WHITE);
          showCenteredText("WIPING ALL BLE KEY", 10);
          showCenteredText("SAFE MODE ACTIVE", 24);
          display.display();
      }
      Serial.println("[SYSTEM] Safe Mode Aktif: Semua Kunci Dihapus, Relay Menyala (Bypass ON).");
      for(int i=0; i<3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      delay(2000); 
  }

  WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  pinMode(MCP_INT_PIN, INPUT_PULLUP); 
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(100); 
  
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  preferences.begin("cfg", true); // Membaca dalam mode read-only

  ssid = preferences.getString("ssid", "pixel8");      
  password = preferences.getString("pass", "1sampai8");
  splashText = preferences.getString("splash", "POLYTRON");
  soundEnabled = preferences.getBool("snd", true);
  buzzerIsPassive = preferences.getBool("buzzPass", false); 
  bleEnabled = preferences.getBool("ble", false);
  
  oledSpeedWarnEnable = preferences.getBool("oledSpdEn", true);
  oledSpeedWarnLimit = preferences.getInt("oledSpd", 70);
  buzzerSpeedWarnEnable = preferences.getBool("buzzSpdEn", true);
  buzzerSpeedWarnLimit = preferences.getInt("buzzSpd", 80);
  modePopupEnable = preferences.getBool("popMode", true);
  autoSleepEnable = preferences.getBool("slpMode", true);
  tripCalibration = preferences.getFloat("tripCal", 1.0f);

  keylessEnabled = preferences.getBool("keylessEn", false);
  registeredTag1 = preferences.getString("tag1", "");
  registeredTag2 = preferences.getString("tag2", "");
  keylessRssiThreshold = preferences.getInt("keyRssi", -80);
  
  preferences.end(); // Langsung tutup agar thread-safe!

  if (oledOk) {
      display.clearDisplay(); display.setFont(&FreeSansBold9pt7b); display.setTextColor(SSD1306_WHITE);
      showCenteredText(splashText, 22); display.display();
  }

  if (soundEnabled) {
      if (buzzerIsPassive) {
          int marioNotes[] = {2637, 2637, 2637, 2093, 2637, 3136, 1568}; 
          int marioDurs[]  = {100,  100,  100,  100,  100,  150,  150}; 
          int marioDelays[]= {150,  300,  150,  150,  300,  600,  600}; 
          for (int i = 0; i < 7; i++) {
              tone(BUZZER_PIN, marioNotes[i]); delay(marioDurs[i]); noTone(BUZZER_PIN); delay(marioDelays[i] - marioDurs[i]); 
          }
      } else {
          digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW); delay(100);
          digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(200);
      }
  } else { delay(1500); }

  rtcOk = rtc.begin();

  Wire.beginTransmission(EEPROM_ADDR);
  if (Wire.endTransmission() == 0) {
      extEepromAvailable = true; memoryInUse = "EXT(RTC)";
      trip_km = readEEPROMFloat(ADDR_TRIP_KM); trip_wh = readEEPROMFloat(ADDR_TRIP_WH);
  } else {
      extEepromAvailable = false; memoryInUse = "INTERNAL";
      
      preferences.begin("cfg", true);
      trip_km = preferences.getFloat("trip_km", 0.0); 
      trip_wh = preferences.getFloat("trip_wh", 0.0);
      preferences.end();
  }
  last_saved_trip_km = trip_km;

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, MCP_CS_PIN);
  canOk = (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK);
  dashboardRestricted = !canOk; 

  if (oledOk && (!canOk || !rtcOk)) {
      display.clearDisplay(); display.setFont(); display.setTextSize(1); display.setCursor(0, 0);
      display.println("== SYSTEM WARNING ==");
      display.print("RTC : "); display.println(rtcOk ? "OK" : "ERROR");
      display.print("MEM : "); display.println(extEepromAvailable ? "EXT(OK)" : "INT(FB)");
      display.print("CAN : "); display.println(canOk ? "OK" : "FAIL (Cek Kabel)");
      display.display();
      delay(5000); 
  } 
  
  if (canOk) { CAN0.setMode(MCP_NORMAL); }
  lastCalcTime = millis();
  
  if (bleEnabled) { 
      initBLE(); 
  }
}

// ================= LOOP UTAMA =================
void loop() {
  if (canOk) { readCAN(); }
  checkSerialCommands(); 
  handleBuzzer(); 
  handleBLE(); 
  handleKeylessScan();

  unsigned long now = millis();
  if (lastCalcTime == 0) lastCalcTime = now;
  unsigned long dt = now - lastCalcTime;
  
  if (dt > 0) {
      if (calc_speed > 0) trip_km += (calc_speed * dt) / 3600000.0; 
      if (fabs(power_watt) > 0) trip_wh += (fabs(power_watt) * dt) / 3600000.0; 
      lastCalcTime = now;
  }

  static unsigned long stopTime = 0;
  if (speed_kmh == 0 && (trip_km - last_saved_trip_km >= 0.1)) {
      if (stopTime == 0) stopTime = now;
      if (now - stopTime > 5000) {  
          if (extEepromAvailable) { writeEEPROMFloat(ADDR_TRIP_KM, trip_km); writeEEPROMFloat(ADDR_TRIP_WH, trip_wh); } 
          else { 
              preferences.begin("cfg", false);
              preferences.putFloat("trip_km", trip_km); 
              preferences.putFloat("trip_wh", trip_wh); 
              preferences.end();
          }
          last_saved_trip_km = trip_km; 
          Serial.println("[MEMORY] Data Trip disimpan saat berhenti.");
          stopTime = 0;
      }
  } else if (speed_kmh > 0) stopTime = 0; 

  btnState = (digitalRead(BUTTON_PIN) == LOW);
  if (btnState && !lastBtnState) {
    btnPressTime = now; isBtnPressed = true; handled3s = false; handled5s = false; playBeep(50, 3500);
  } else if (!btnState && lastBtnState) {
    isBtnPressed = false; unsigned long duration = now - btnPressTime;
    if (duration < 3000 && duration > 50) {
      if (inSettingsMode) { 
          settingsCursor++; if (settingsCursor > 6) settingsCursor = 0; 
      } else { 
          if (dashboardRestricted) {
              if (currentPage == 1) currentPage = 8; else currentPage = 1;
          } 
          else if (!bleEnabled) { 
              currentPage++; if (currentPage > 7) currentPage = 1; 
          } 
          else { 
              if (currentPage == 1) currentPage = 5; else currentPage = 1; 
          }
          lastButtonPress = now; 
      }
    }
  } else if (isBtnPressed) {
    unsigned long duration = now - btnPressTime;
    if (inSettingsMode) {
      if (duration >= 3000 && !handled3s) { handled3s = true; executeSettingAction(); }
    } else {
      if (currentPage == 5 && duration >= 3000 && duration < 5000 && !handled3s) { 
          handled3s = true; handled5s = true; 
          trip_km = 0.0; trip_wh = 0.0; last_saved_trip_km = 0.0; 
          if (extEepromAvailable) { writeEEPROMFloat(ADDR_TRIP_KM, 0.0); writeEEPROMFloat(ADDR_TRIP_WH, 0.0); } 
          else { 
              preferences.begin("cfg", false);
              preferences.putFloat("trip_km", 0.0); 
              preferences.putFloat("trip_wh", 0.0); 
              preferences.end();
          }
          playBeep(300, 3000);
          if(oledOk) { display.clearDisplay(); display.setFont(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1); showCenteredText("TRIP RESET TO 0", 15); display.display(); delay(1500); }
      }
      else if (duration >= 5000 && !handled5s) { 
          handled5s = true; handled3s = true; inSettingsMode = true; settingsCursor = 0; 
          playBeep(80, 3500); delay(80); playBeep(80, 3500);
          btnPressTime += 240; 
      }
    }
  }
  lastBtnState = btnState;

  if (autoSleepEnable && !inSettingsMode && (now - lastButtonPress > 15000) && currentPage != 1 && !showModePopup && !isCharging && speed_kmh <= 70) { 
      currentPage = 1; 
  }
  if (now - lastDisplayUpdate > 100) { updateOLED(); lastDisplayUpdate = now; }
  delay(5); 
}