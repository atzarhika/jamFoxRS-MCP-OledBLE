#ifndef BLE_KEYLESS_H
#define BLE_KEYLESS_H

#include "Config.h"

// Forward declaration of scanCompleteCB
inline void scanCompleteCB(BLEScanResults results);

// Callback koneksi server BLE
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; }
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

// Callback deteksi beacon tag Keyless sekitar secara realtime
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String address = advertisedDevice.getAddress().toString().c_str();
        address.toUpperCase();
        int rssi = advertisedDevice.getRSSI();
        
        if (keylessEnabled && rssi >= keylessRssiThreshold) {
            if ((registeredTag1.length() > 0 && address == registeredTag1) ||
                (registeredTag2.length() > 0 && address == registeredTag2)) {
                lastTagSeenTime = millis(); // Perbarui waktu deteksi Tag instan secara langsung
            }
        }

        if (webScanActive && pCharacteristic) {
            String name = advertisedDevice.getName().c_str();
            if (name.length() == 0) name = "Tag BLE";
            
            char dBuf[150];
            snprintf(dBuf, sizeof(dBuf), "{\"disc\":{\"name\":\"%s\",\"mac\":\"%s\",\"rssi\":%d}}\n", name.c_str(), address.c_str(), rssi);
            pCharacteristic->setValue((uint8_t*)dBuf, strlen(dBuf));
            pCharacteristic->notify();
        }
    }
};

// Callback penerima perintah dari Web UI HP
class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
        String rxStr = pChar->getValue(); 
        if (rxStr.length() > 0) {
            rxStr.trim();
            if (rxStr.startsWith("TIME,")) {
                int y, m, d, h, mn, s;
                if (sscanf(rxStr.c_str(), "TIME,%d,%d,%d,%d,%d,%d", &y, &m, &d, &h, &mn, &s) == 6) {
                    rtc.adjust(DateTime(y, m, d, h, mn, s));
                    playBeep(150, 2500);
                }
            } 
            else if (rxStr.startsWith("SPLASH,")) {
                String newSplash = rxStr.substring(7);
                if (newSplash.length() > 10) newSplash = newSplash.substring(0, 10);
                preferences.putString("splash", newSplash);
                splashText = newSplash; 
                playBeep(150, 2500);
            }
            else if (rxStr.startsWith("ALARM,")) {
                int comma1 = rxStr.indexOf(',', 0); int comma2 = rxStr.indexOf(',', comma1 + 1); int comma3 = rxStr.indexOf(',', comma2 + 1);
                if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
                    String type = rxStr.substring(comma1 + 1, comma2);
                    int en = rxStr.substring(comma2 + 1, comma3).toInt();
                    int limit = rxStr.substring(comma3 + 1).toInt();

                    if (type == "OLED") {
                        oledSpeedWarnEnable = (en == 1); oledSpeedWarnLimit = limit;
                        preferences.putBool("oledSpdEn", oledSpeedWarnEnable); preferences.putInt("oledSpd", oledSpeedWarnLimit);
                    } else if (type == "BUZZ") {
                        buzzerSpeedWarnEnable = (en == 1); buzzerSpeedWarnLimit = limit;
                        preferences.putBool("buzzSpdEn", buzzerSpeedWarnEnable); preferences.putInt("buzzSpd", buzzerSpeedWarnLimit);
                    }
                    playBeep(150, 2500);
                }
            }
            else if (rxStr.startsWith("SET,")) {
                int comma1 = rxStr.indexOf(',', 0); int comma2 = rxStr.indexOf(',', comma1 + 1);
                if (comma1 > 0 && comma2 > 0) {
                    String type = rxStr.substring(comma1 + 1, comma2);
                    int val = rxStr.substring(comma2 + 1).toInt();
                    
                    if (type == "MODE") {
                        modePopupEnable = (val == 1);
                        preferences.putBool("popMode", modePopupEnable);
                    } else if (type == "SLEEP") {
                        autoSleepEnable = (val == 1);
                        preferences.putBool("slpMode", autoSleepEnable);
                    } else if (type == "KEYLESS") {
                        keylessEnabled = (val == 1);
                        preferences.putBool("keylessEn", keylessEnabled);
                        if (!keylessEnabled) {
                            digitalWrite(RELAY_PIN, HIGH); 
                            relayState = true;
                        } else {
                            lastTagSeenTime = millis(); // Set jangkauan awal saat diaktifkan lewat HP
                        }
                    }
                    playBeep(150, 2500);
                }
            }
            else if (rxStr.startsWith("CALIB,")) {
                float newCal = rxStr.substring(6).toFloat();
                if(newCal > 0.5f && newCal < 2.0f) {
                    tripCalibration = newCal;
                    preferences.putFloat("tripCal", tripCalibration);
                    playBeep(200, 3000);
                }
            }
            else if (rxStr.startsWith("SCANBLE")) {
                triggerWebScan = true;
                playBeep(100, 2000);
            }
            else if (rxStr.startsWith("ADDTAG,")) {
                int c1 = rxStr.indexOf(',');
                int c2 = rxStr.indexOf(',', c1 + 1);
                if (c1 > 0 && c2 > 0) {
                    int slot = rxStr.substring(c1 + 1, c2).toInt();
                    String mac = rxStr.substring(c2 + 1);
                    mac.trim(); mac.toUpperCase();
                    if (slot == 1) {
                        registeredTag1 = mac;
                        preferences.putString("tag1", registeredTag1);
                    } else if (slot == 2) {
                        registeredTag2 = mac;
                        preferences.putString("tag2", registeredTag2);
                    }
                    playBeep(150, 3000);
                }
            }
            else if (rxStr.startsWith("DELTAG,")) {
                int slot = rxStr.substring(7).toInt();
                if (slot == 1) {
                    registeredTag1 = "";
                    preferences.putString("tag1", "");
                } else if (slot == 2) {
                    registeredTag2 = "";
                    preferences.putString("tag2", "");
                }
                playBeep(150, 1500);
            }
            else if (rxStr.startsWith("KEYRSSI,")) {
                int newRssi = rxStr.substring(8).toInt();
                if (newRssi >= -100 && newRssi <= -40) {
                    keylessRssiThreshold = newRssi;
                    preferences.putInt("keyRssi", keylessRssiThreshold);
                    playBeep(150, 3000);
                }
            }
        }
    }
};

inline void initBLE() {
  if (pServer != nullptr) return; 
  BLEDevice::init("Votol_BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  
  pRxCharacteristic = pService->createCharacteristic(RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  pService->start();
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  
  // Optimasi parameter scanner agar responsif dan hemat daya
  pBLEScan->setInterval(160); 
  pBLEScan->setWindow(80);    
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

inline void handleKeylessScan() {
    if (!bleEnabled) {
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        inShutdownWarning = false;
        return;
    }

    unsigned long now = millis();

    // =========================================================================
    // INTEGRASI INDIKATOR SISTEM BERGERAK (MOTION SAFETY LOCK GANDA)
    // =========================================================================
    // Deteksi apakah motor terdeteksi sedang melaju atau dinamo berputar (berdasarkan CAN Votol)
    bool isMotorMoving = (speed_kmh > 0 || rpm > 0);

    // 1. Logika Pengunci Relay (Hanya mengunci jika Keyless AKTIF)
    if (keylessEnabled) {
        // JIKA TAG TERDETEKSI ATAU MOTOR SEDANG BERJALAN:
        // Segera paksa relay HIGH (menyala) dan matikan seluruh hitung mundur shutdown
        if (now - lastTagSeenTime < 25000 || isMotorMoving) {
            digitalWrite(RELAY_PIN, HIGH);
            relayState = true;
            inShutdownWarning = false;
            shutdownWarningStartTime = 0;
        }
        // JIKA TAG TIDAK TERDETEKSI > 1 DETIK, MOTOR DIAM TOTAL, DAN BELUM MASUK WARNING:
        // Masuk ke mode warning (Masa Tenggang Grace Period)
        else if (now - lastTagSeenTime >= 1000 && !isMotorMoving && !inShutdownWarning) {
            inShutdownWarning = true;
            shutdownWarningStartTime = now;
            Serial.println("[KEYLESS] Tag keluar jangkauan & motor diam. Memulai hitung mundur...");
        }

        // EVALUASI MASA TENGGANG KEPUTUSAN (GRACE PERIOD)
        if (inShutdownWarning) {
            // Pengaman Tambahan: Jika motor tiba-tiba digas atau bergerak selama hitung mundur,
            // batalkan instan status shutdown warning demi keselamatan berkendara!
            if (isMotorMoving) {
                digitalWrite(RELAY_PIN, HIGH);
                relayState = true;
                inShutdownWarning = false;
                shutdownWarningStartTime = 0;
            } else {
                unsigned long elapsed = now - shutdownWarningStartTime;
                if (elapsed < 9000) { // Masa tenggang 9 detik penuh
                    digitalWrite(RELAY_PIN, HIGH);
                    relayState = true;
                } else {
                    // Hanya jika motor DIAM total selama 10 detik dan Tag benar-benar hilang, 
                    // putus aliran SSR kelistrikan secara aman (Off saat parkir)
                    digitalWrite(RELAY_PIN, LOW); 
                    relayState = false;
                }
            }
        }
    } else {
        // Jika Keyless dimatikan, paksa Relay selalu ON (Bypass)
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        inShutdownWarning = false;
        shutdownWarningStartTime = 0;
    }

    // 2. Logika Manual Scan dari Web UI (Mengecek dan menghentikan background scan terlebih dahulu)
    if (triggerWebScan) {
        triggerWebScan = false;
        isScanningActive = true;
        webScanActive = true;
        if (pCharacteristic) {
            pCharacteristic->setValue((uint8_t*)"{\"scan_status\":\"START\"}\n", 23);
            pCharacteristic->notify();
        }
        BLEDevice::getScan()->stop(); // Hentikan background scan
        BLEDevice::getScan()->start(4, &scanCompleteCB, false); // Jalankan scan manual 4 detik
        lastScanTime = millis();
        return;
    }

    // 3. SELEF-HEALING INDESTRUCTIBLE SCAN LOOP (AUTO-RESTART CONTINUOUS SCAN)
    if (keylessEnabled && !isScanningActive && !webScanActive) {
        isScanningActive = true;
        BLEDevice::getScan()->start(3, &scanCompleteCB, false);
        lastScanTime = now;
    }

    // WATCHDOG PROTECTION FAILSAFE: 
    if (keylessEnabled && isScanningActive && (now - lastScanTime > 6000)) {
        Serial.println("[KEYLESS] Watchdog terpicu! Mengatur ulang modul pemindai BLE...");
        BLEDevice::getScan()->stop();
        isScanningActive = false;
    }
}

inline void buildJsonInto() {
  int cellDelta = 0; uint16_t minCell = 9999, maxCell = 0;
  for (int i = 0; i < 23; i++) { if (valCells[i] > 0 && valCells[i] < minCell) minCell = valCells[i]; if (valCells[i] > maxCell) maxCell = valCells[i]; }
  if (maxCell >= minCell) cellDelta = (int)maxCell - (int)minCell;

  char balanceCells[100] = {0}; int bpos = 0;
  for (int i = 0; i < 23; i++) { bool isBalancing = (valBalanceBits[i / 8] & (1 << (i % 8))) != 0; int remain = sizeof(balanceCells) - bpos; if (remain > 0) bpos += snprintf(balanceCells + bpos, remain, "%d%s", isBalancing ? 1 : 0, (i < 22) ? "," : ""); }

  char cellsStr[256] = {0}; int cpos = 0;
  for (int i = 0; i < 23; i++) { int remain = sizeof(cellsStr) - cpos; if (remain > 0) cpos += snprintf(cellsStr + cpos, remain, "%u%s", valCells[i], (i < 22) ? "," : ""); }

  float avg_wh = (trip_km > 0.5) ? (trip_wh / trip_km) : 0.0;
  int est_range = 0;
  if (avg_wh > 1.0) {
      float dynamic_1_percent_wh = (valFullCapacity > 10.0) ? (valFullCapacity * 0.72) : 39.6; 
      est_range = (int)((dynamic_1_percent_wh * soc) / avg_wh);
      if (est_range > 130) est_range = 130; 
  }

  // Mengirim data ke Web UI
  bleTxLen = snprintf(bleTxBuf, sizeof(bleTxBuf),
    "{\"rpm\":%d,\"speed\":%d,\"mode\":\"%s\",\"volts\":%.1f,\"amps\":%.1f,\"power\":%.0f,\"soc\":%d,"
    "\"trip\":{\"km\":%.1f,\"avg\":%.1f,\"range\":%d},\"cal\":%.3f,"
    "\"temps\":{\"ctrl\":%d,\"motor\":%d,\"batt\":%d},\"cells\":[%s],\"cellDelta\":%d,"
    "\"canRate\":%lu,\"odometer\":%lu,\"health\":{\"soh\":%d,\"cycles\":%u,\"remainCap\":%.1f,\"fullCap\":%.1f},"
    "\"cellVoltStats\":{\"highest\":%u,\"highestCell\":%u,\"lowest\":%u,\"lowestCell\":%u,\"avg\":%u},"
    "\"tempStats\":{\"max\":%u,\"maxCell\":%u,\"min\":%u,\"minCell\":%u},"
    "\"balance\":{\"mode\":%u,\"status\":%u,\"cells\":[%s]},"
    "\"charger\":{\"on\":%d,\"v\":%.1f,\"a\":%.1f,\"ori\":%d},"
    "\"set\":{\"pop\":%d,\"slp\":%d,\"olE\":%d,\"olL\":%d,\"bzE\":%d,\"bzL\":%d},"
    "\"keyless\":{\"en\":%d,\"t1\":\"%s\",\"t2\":\"%s\",\"relay\":%d,\"rssiTh\":%d},"
    "\"cruise\":%d,\"stand\":%d,\"brake\":%d,\"bms\":{\"hw\":\"%s\",\"fw\":\"%s\"},\"hb\":%lu}\n",
    rpm, speed_kmh, currentMode.c_str(), volts, amps, power_watt, soc,
    trip_km, avg_wh, est_range, tripCalibration,
    tempCtrl, tempMotor, tempBatt, cellsStr, cellDelta,
    (unsigned long)canMessagesPerSec, (unsigned long)valOdometer,
    valSOH, valCycleCount, valRemainingCapacity, valFullCapacity,
    valHighestCellVolt, valHighestCellNum, valLowestCellVolt, valLowestCellNum, valAvgCellVolt,
    valMaxTemp, valMaxTempCell, valMinTemp, valMinTempCell,
    valBalanceMode, valBalanceStatus, balanceCells,
    bmsChargingFlag ? 1 : 0, volts, (chargerCurrent > 0.1f) ? chargerCurrent : fabs(amps),
    oriChargerDetected ? 1 : 0,
    modePopupEnable ? 1 : 0, autoSleepEnable ? 1 : 0, oledSpeedWarnEnable ? 1 : 0, oledSpeedWarnLimit, buzzerSpeedWarnEnable ? 1 : 0, buzzerSpeedWarnLimit,
    keylessEnabled ? 1 : 0, registeredTag1.c_str(), registeredTag2.c_str(), relayState ? 1 : 0, keylessRssiThreshold,
    cruiseActive, standActive, brakeActive,
    bmsHwVersion, bmsFwVersion, (unsigned long)heartbeatCounter++
  );
}

inline void handleBLE() {
  if (!bleEnabled) return;
  if (!deviceConnected && oldDeviceConnected) { delay(50); BLEDevice::startAdvertising(); oldDeviceConnected = deviceConnected; }
  if (deviceConnected && !oldDeviceConnected) { oldDeviceConnected = deviceConnected; }

  if (deviceConnected) {
    uint32_t now = millis();
    
    // PEMBATASAN TRAFIK (CONGESTION CONTROL):
    uint32_t telemetryPacing = webScanActive ? 1000 : 200;

    if (!bleTxInProgress && (now - lastDataSend >= telemetryPacing)) { 
        lastDataSend = now; 
        buildJsonInto(); 
        bleTxOffset = 0; 
        bleTxInProgress = true; 
    }
    
    if (bleTxInProgress) {
      const uint32_t startUs = micros(); int sent = 0;
      while (bleTxInProgress && sent < 4 && (micros() - startUs) < 10000) {
        int remain = bleTxLen - bleTxOffset; if (remain <= 0) { bleTxInProgress = false; break; }
        int chunkLen = (remain > 200) ? 200 : remain; pCharacteristic->setValue((uint8_t*)(bleTxBuf + bleTxOffset), chunkLen);
        pCharacteristic->notify(); bleTxOffset += chunkLen; sent++;
      }
      if (bleTxOffset >= bleTxLen) bleTxInProgress = false;
    }
  }
}

inline void scanCompleteCB(BLEScanResults results) {
    isScanningActive = false;
    if (webScanActive && pCharacteristic) {
        webScanActive = false;
        pCharacteristic->setValue((uint8_t*)"{\"scan_status\":\"END\"}\n", 21);
        pCharacteristic->notify();
    }
    BLEDevice::getScan()->clearResults(); 
}

#endif