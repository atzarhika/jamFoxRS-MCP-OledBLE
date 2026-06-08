#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include "Config.h"

inline void showCenteredText(String text, int yPos) {
  if (!oledOk) return; 
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, yPos, &x1, &y1, &w, &h); 
  int xPos = (128 - w) / 2; 
  if (xPos < 0) xPos = 0; 
  display.setCursor(xPos, yPos);
  display.print(text);
}

inline void executeSettingAction() {
  if (settingsCursor == 0) { 
      soundEnabled = !soundEnabled; preferences.putBool("snd", soundEnabled); playBeep(100, 3000);
  } else if (settingsCursor == 1) { 
      buzzerIsPassive = !buzzerIsPassive; preferences.putBool("buzzPass", buzzerIsPassive); playBeep(150, 3000); 
  } else if (settingsCursor == 2) { 
      bleEnabled = !bleEnabled; preferences.putBool("ble", bleEnabled); 
      if(oledOk) { display.clearDisplay(); display.setFont(); display.setTextSize(1); showCenteredText("REBOOTING...", 15); display.display(); } 
      delay(1500); ESP.restart(); 
  } else if (settingsCursor == 3) { 
      if (bleEnabled) { if(oledOk) { display.clearDisplay(); display.setFont(); display.setTextSize(1); showCenteredText("TURN OFF BLE FIRST!", 15); display.display(); delay(2500); } } 
      else { performNtpSync(false); inSettingsMode = false; }
  } else if (settingsCursor == 4) { 
      modePopupEnable = !modePopupEnable; preferences.putBool("popMode", modePopupEnable); playBeep(100, 3000);
  } else if (settingsCursor == 5) { 
      autoSleepEnable = !autoSleepEnable; preferences.putBool("slpMode", autoSleepEnable); playBeep(100, 3000);
  } else if (settingsCursor == 6) { 
      inSettingsMode = false; lastButtonPress = millis(); playBeep(100, 2000);
  }
}

inline void updateOLED() {
  if (!oledOk) return; 

  // --- HIBERNATE LAYAR FISIK HANYA BERDASARKAN STATUS RELAY STATE (SINKRON DENGAN SISTEM PARKIR) ---
  static bool screenWakeState = true;
  if (keylessEnabled && !relayState) {
      if (screenWakeState) {
          display.clearDisplay();
          display.display();
          display.ssd1306_command(SSD1306_DISPLAYOFF); // Matikan layar fisik sepenuhnya
          screenWakeState = false;
      }
      return; 
  } else {
      if (!screenWakeState) {
          display.ssd1306_command(SSD1306_DISPLAYON); // Nyalakan layar kembali
          screenWakeState = true;
      }
  }

  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);

  // --- POPUP WARNING GRACE PERIOD (COUNTDOWN DELAY):
  // Memberikan tanda visual hitung mundur berkedip cepat (300ms) sebelum mematikan kelistrikan utama
  if (keylessEnabled && inShutdownWarning) {
      unsigned long elapsed = millis() - shutdownWarningStartTime;
      int secondsLeft = 9 - (elapsed / 1000);
      if (secondsLeft < 0) secondsLeft = 0;

      if ((millis() / 300) % 2 == 0) { // Efek kedip cepat
          display.setFont(&FreeSansBold9pt7b);
          showCenteredText("ALERT", 14);
          display.setFont(); 
          display.setTextSize(1);
          display.setCursor(0, 24);
          display.printf("LOCKING SYSTEM IN %dS", secondsLeft);
      } else {
          display.clearDisplay(); 
      }
      display.display();
      return; // Kunci layar OLED selama masa tenggang
  }

  // --- WARNING STANDAR SAMPING BERKEDIP DURASI PENUH ---
  if (standActive == 1) {
      if ((millis() / 450) % 2 == 0) { 
          display.setFont(&FreeSansBold9pt7b);
          showCenteredText("STAND DOWN!", 16);
          display.setFont(); 
          display.setTextSize(1);
          showCenteredText("LIFT TO RIDE", 25);
      } else {
          display.clearDisplay(); 
      }
      display.display();
      return; 
  }

  // --- POPUP INTERAKTIF CRUISE CONTROL 3 DETIK ---
  if (showCruisePopup) {
      if (millis() - lastCruiseChange < 3000) {
          display.setFont(&FreeSansBold9pt7b);
          showCenteredText("CRUISE ON", 16);
          display.setFont();
          display.setTextSize(1);
          showCenteredText("SPEED LOCKED", 25);
          display.display();
          return;
      } else {
          showCruisePopup = false; 
      }
  }

  if (inSettingsMode) {
    display.setFont(); display.setTextSize(1); 
    String opt[7] = {
        "SOUND  : " + String(soundEnabled ? "ON" : "OFF"), 
        "TYPE   : " + String(buzzerIsPassive ? "PASSIVE" : "ACTIVE"),
        "BLE OUT: " + String(bleEnabled ? "ON" : "OFF"), 
        "SYNC NTP (WIFI)", 
        "POPUP  : " + String(modePopupEnable ? "ON" : "OFF"),
        "SLEEP  : " + String(autoSleepEnable ? "ON" : "OFF"),
        "EXIT SETTINGS"
    };
    int startIdx = 0;
    if (settingsCursor >= 4) startIdx = settingsCursor - 3;
    for(int i = 0; i < 4; i++) { 
        int idx = startIdx + i;
        if(idx < 7) {
            display.setCursor(0, i * 8); 
            if (settingsCursor == idx) display.print(">"); else display.print(" "); 
            display.print(opt[idx]); 
        }
    }
    display.display(); return;
  }

  if (isCharging) {
    display.setFont(); bool showAmps = (millis() / 2000) % 2 == 0; display.setTextSize(1); display.setCursor(0, 0); display.print(oriChargerDetected ? "ORI CHARGER:" : "FAST CHARGER:");
    display.setTextSize(2); display.setCursor(0, 15); 
    if (showAmps) { display.print("IN: "); display.print((chargerCurrent > 0.1f) ? chargerCurrent : fabs(amps), 1); display.print("A"); } else { display.print("BAT: "); display.print(soc); display.print("%"); }
    display.display(); return;
  }

  if (oledSpeedWarnEnable && speed_kmh >= oledSpeedWarnLimit) {
    display.setFont(&FreeSansBold18pt7b); display.setCursor(0, 28); display.printf("%d", speed_kmh); display.setFont(); display.setTextSize(1); display.setCursor(88, 15); display.print("KM/H"); display.display(); return;
  }

  if (showModePopup) {
    if (millis() - lastModeChange < 3000) { display.setFont(&FreeSansBold9pt7b); showCenteredText(currentMode, 22); display.display(); return; } else { showModePopup = false; currentPage = 1; }
  }

  display.setFont(); 
  switch (currentPage) {
    case 1: { 
      DateTime dt = rtc.now(); display.setFont(&FreeSansBold18pt7b); display.setCursor(0, 28); 
      if (cruiseActive == 1 && (millis() / 500) % 2 == 0) {
          display.printf("%02d:%02d", dt.hour(), dt.minute());
          display.setFont(); display.setTextSize(1); display.setCursor(88, 25); display.print("CRUISE");
      } else {
          display.printf("%02d:%02d", dt.hour(), dt.minute());
          display.setFont(); display.setTextSize(1); display.setCursor(88, 25); if (bleEnabled) display.print("BLE ON"); else display.printf("%04d", dt.year());
      }
      display.setFont(); display.setTextSize(1); display.setCursor(88, 5);  display.print(DAY_NAMES[dt.dayOfTheWeek()]); display.setCursor(88, 15); display.printf("%d %s", dt.day(), MONTH_NAMES[dt.month() - 1]);
      break;
    }
    case 2: { 
      display.setTextSize(1); display.setCursor(0, 4); display.print("ECU"); display.setCursor(43, 4); display.print("MOTOR"); display.setCursor(86, 4); display.print("BATT");
      display.setTextSize(2); display.setCursor(0, 16); display.print(tempCtrl); display.setCursor(43, 16); display.print(tempMotor); display.setCursor(86, 16); display.print(tempBatt); break;
    }
    case 3: { 
      display.setTextSize(1); display.setCursor(8, 4); display.print("VOLT"); display.setCursor(86, 4); display.print("ARUS");
      display.setTextSize(2); display.setCursor(0, 16); display.print(volts, 1); display.setTextSize(1); display.setCursor(55, 22); display.print("V");
      display.setTextSize(2); display.setCursor(78, 16); display.print(fabs(amps), 1); display.setTextSize(1); display.setCursor(120, 22); display.print("A"); break;
    }
    case 4: { 
      int pwr = abs((int)power_watt); display.setTextSize(2); display.setCursor(10, 4); display.print((power_watt > 0.1f) ? "+" : "-"); display.setTextSize(3); display.setCursor(32, 2); display.print(pwr); display.setTextSize(1); display.setCursor(102, 22); display.print("watt"); break;
    }
    case 5: { 
      float avg_wh = (trip_km > 0.5) ? (trip_wh / trip_km) : 0.0;
      int est_range = 0;
      if (avg_wh > 1.0) {
          float dynamic_1_percent_wh = (valFullCapacity > 10.0) ? (valFullCapacity * 0.72) : 39.6; 
          est_range = (int)((dynamic_1_percent_wh * soc) / avg_wh);
          if (est_range > 130) est_range = 130; 
      }
      int16_t x1, y1; uint16_t w, h;
      display.setTextSize(1); display.setCursor(0, 0); display.print("TRIP(KM)"); 
      String rightHeader = "AVG(Wh/km)"; display.getTextBounds(rightHeader, 0, 0, &x1, &y1, &w, &h); display.setCursor(128 - w, 0); display.print(rightHeader);
      display.setTextSize(2); display.setCursor(0, 8); display.print(trip_km, 1); 
      String rightValue = String(avg_wh, 1); display.getTextBounds(rightValue, 0, 0, &x1, &y1, &w, &h); display.setCursor(128 - w, 8); display.print(rightValue);
      display.setTextSize(1); 
      String bottomText = "";
      if (trip_km < 1.0) bottomText = "CALCULATING..."; else bottomText = "EST RANGE: " + String(est_range) + " KM";
      display.getTextBounds(bottomText, 0, 0, &x1, &y1, &w, &h); display.setCursor((128 - w) / 2, 24); display.print(bottomText);
      break;
    }
    case 6: { 
      int16_t x1, y1; uint16_t w, h;
      display.setTextSize(1); display.setCursor(0, 0); display.print("HEALTH"); 
      String rightHeader = "CAPACITY"; display.getTextBounds(rightHeader, 0, 0, &x1, &y1, &w, &h); display.setCursor(128 - w, 0); display.print(rightHeader);
      display.setTextSize(2); String leftValue = String(valSOH) + "%"; display.setCursor(0, 10); display.print(leftValue); 
      String rightValue = String(valFullCapacity, 1) + "Ah"; display.getTextBounds(rightValue, 0, 0, &x1, &y1, &w, &h); display.setCursor(128 - w, 10); display.print(rightValue);
      break;
    }
    case 7: { 
      display.setTextSize(1); 
      display.setCursor(0, 0);  display.printf("WIFI: %s", ssid.c_str());
      display.setCursor(0, 8);  display.printf("MEM : %s", memoryInUse.c_str());
      display.setCursor(0, 16); display.printf("NAME: %s", splashText.c_str()); 
      display.setCursor(0, 24); display.printf("FW  : %s", FW_VERSION); 
      break;
    }
    case 8: { 
      display.setTextSize(1); display.setCursor(0, 0); 
      display.println("== DIAGNOSTIC ==");
      display.print("RTC : "); display.println(rtcOk ? "OK" : "ERROR (Cek I2C)");
      display.print("MEM : "); display.println(extEepromAvailable ? "EXT(OK)" : "INT(FB)");
      display.print("CAN : "); display.println(canOk ? "OK" : "FAIL (Cek SPI)");
      break;
    }
  }
  display.display();
}

#endif