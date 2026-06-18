#ifndef BLE_KEYLESS_H
#define BLE_KEYLESS_H

#include "Config.h"

// Forward declaration of scanCompleteCB
void scanCompleteCB(BLEScanResults results);

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
                    playBeep(150, 3500);
                }
            } 
            else if (rxStr.startsWith("SPLASH,")) {
                String newSplash = rxStr.substring(7);
                if (newSplash.length() > 10) newSplash = newSplash.substring(0, 10);
                
                preferences.begin("cfg", false);
                preferences.putString("splash", newSplash);
                preferences.end();
                
                splashText = newSplash; 
                playBeep(150, 3500);
            }
            else if (rxStr.startsWith("ALARM,")) {
                int comma1 = rxStr.indexOf(',', 0); int comma2 = rxStr.indexOf(',', comma1 + 1); int comma3 = rxStr.indexOf(',', comma2 + 1);
                if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
                    String type = rxStr.substring(comma1 + 1, comma2);
                    int en = rxStr.substring(comma2 + 1, comma3).toInt();
                    int limit = rxStr.substring(comma3 + 1).toInt();

                    preferences.begin("cfg", false);
                    if (type == "OLED") {
                        oledSpeedWarnEnable = (en == 1); oledSpeedWarnLimit = limit;
                        preferences.putBool("oledSpdEn", oledSpeedWarnEnable); preferences.putInt("oledSpd", oledSpeedWarnLimit);
                    } else if (type == "BUZZ") {
                        buzzerSpeedWarnEnable = (en == 1); buzzerSpeedWarnLimit = limit;
                        preferences.putBool("buzzSpdEn", buzzerSpeedWarnEnable); preferences.putInt("buzzSpd", buzzerSpeedWarnLimit);
                    }
                    preferences.end();
                    playBeep(150, 3500);
                }
            }
            else if (rxStr.startsWith("SET,")) {
                int comma1 = rxStr.indexOf(',', 0); int comma2 = rxStr.indexOf(',', comma1 + 1);
                if (comma1 > 0 && comma2 > 0) {
                    String type = rxStr.substring(comma1 + 1, comma2);
                    int val = rxStr.substring(comma2 + 1).toInt();
                    
                    preferences.begin("cfg", false);
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
                            lastTagSeenTime = millis(); 
                        }
                    }
                    preferences.end();
                    playBeep(150, 3500);
                }
            }
            else if (rxStr.startsWith("CALIB,")) {
                float newCal = rxStr.substring(6).toFloat();
                if(newCal > 0.5f && newCal < 2.0f) {
                    tripCalibration = newCal;
                    preferences.begin("cfg", false);
                    preferences.putFloat("tripCal", tripCalibration);
                    preferences.end();
                    playBeep(200, 3500);
                }
            }
            else if (rxStr.startsWith("SCANBLE")) {
                triggerWebScan = true;
                playBeep(100, 3500);
            }
            else if (rxStr.startsWith("ADDTAG,")) {
                int c1 = rxStr.indexOf(',');
                int c2 = rxStr.indexOf(',', c1 + 1);
                if (c1 > 0 && c2 > 0) {
                    int slot = rxStr.substring(c1 + 1, c2).toInt();
                    String mac = rxStr.substring(c2 + 1);
                    mac.trim(); mac.toUpperCase();
                    
                    preferences.begin("cfg", false);
                    if (slot == 1) {
                        registeredTag1 = mac;
                        preferences.putString("tag1", registeredTag1);
                    } else if (slot == 2) {
                        registeredTag2 = mac;
                        preferences.putString("tag2", registeredTag2);
                    }
                    preferences.end();
                    playBeep(150, 3500);
                }
            }
            else if (rxStr.startsWith("DELTAG,")) {
                int slot = rxStr.substring(7).toInt();
                preferences.begin("cfg", false);
                if (slot == 1) {
                    registeredTag1 = "";
                    preferences.putString("tag1", "");
                } else if (slot == 2) {
                    registeredTag2 = "";
                    preferences.putString("tag2", "");
                }
                preferences.end();
                playBeep(150, 3500);
            }
            else if (rxStr.startsWith("KEYRSSI,")) {
                int newRssi = rxStr.substring(8).toInt();
                if (newRssi >= -100 && newRssi <= -40) {
                    keylessRssiThreshold = newRssi;
                    preferences.begin("cfg", false);
                    preferences.putInt("keyRssi", keylessRssiThreshold);
                    preferences.end();
                    playBeep(150, 3500);
                }
            }
        }
    }
};

void initBLE() {
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
  
  pBLEScan->setInterval(160); 
  pBLEScan->setWindow(80);    
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

void handleKeylessScan() {
    if (!bleEnabled) {
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        inShutdownWarning = false;
        return;
    }

    unsigned long now = millis();
    bool isMotorMoving = (speed_kmh > 0 || rpm > 0);

    // =========================================================================
    //  DURASI KEYLESS BISA DIEDIT DI SINI (SATUAN MILIDETIK)
    // =========================================================================
    // TAG_TIMEOUT_MS     : Durasi jangkauan tag hilang (default: 25000ms / 25 detik)
    // SHUTDOWN_GRACE_MS   : Durasi hitung mundur visual oled (default: 10000ms / 10 detik)
    // =========================================================================

    if (keylessEnabled) {
        if (now - lastTagSeenTime < TAG_TIMEOUT_MS) {
            digitalWrite(RELAY_PIN, HIGH);
            relayState = true;
            inShutdownWarning = false;
            shutdownWarningStartTime = 0;
        }
        else if (now - lastTagSeenTime >= TAG_TIMEOUT_MS && !isMotorMoving && !inShutdownWarning) {
            inShutdownWarning = true;
            shutdownWarningStartTime = now;
            Serial.println("[KEYLESS] Tag keluar jangkauan & motor diam. Memulai hitung mundur...");
        }

        if (inShutdownWarning) {
            if (isMotorMoving) {
                digitalWrite(RELAY_PIN, HIGH);
                relayState = true;
                inShutdownWarning = false;
                shutdownWarningStartTime = 0;
            } else {
                unsigned long elapsed = now - shutdownWarningStartTime;
                if (elapsed < SHUTDOWN_GRACE_MS) { 
                    digitalWrite(RELAY_PIN, HIGH);
                    relayState = true;
                } else {
                    digitalWrite(RELAY_PIN, LOW); 
                    relayState = false;
                }
            }
        }
    } else {
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        inShutdownWarning = false;
        shutdownWarningStartTime = 0;
    }

    if (triggerWebScan) {
        triggerWebScan = false;
        isScanningActive = true;
        webScanActive = true;
        if (pCharacteristic) {
            pCharacteristic->setValue((uint8_t*)"{\"scan_status\":\"START\"}\n", 23);
            pCharacteristic->notify();
        }
        BLEDevice::getScan()->stop(); 
        BLEDevice::getScan()->start(4, &scanCompleteCB, false); 
        lastScanTime = millis();
        return;
    }

    if (keylessEnabled && !isScanningActive && !webScanActive) {
        isScanningActive = true;
        BLEDevice::getScan()->start(3, &scanCompleteCB, false);
        lastScanTime = now;
    }

    if (keylessEnabled && isScanningActive && (now - lastScanTime > 6000)) {
        Serial.println("[KEYLESS] Watchdog terpicu! Mengatur ulang modul pemindai BLE...");
        BLEDevice::getScan()->stop();
        isScanningActive = false;
    }
}

void buildJsonInto() {
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

void handleBLE() {
  if (!bleEnabled) return;
  if (!deviceConnected && oldDeviceConnected) { delay(50); BLEDevice::startAdvertising(); oldDeviceConnected = deviceConnected; }
  if (deviceConnected && !oldDeviceConnected) { oldDeviceConnected = deviceConnected; }

  if (deviceConnected) {
    uint32_t now = millis();
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

void scanCompleteCB(BLEScanResults results) {
    isScanningActive = false;
    if (webScanActive && pCharacteristic) {
        webScanActive = false;
        pCharacteristic->setValue((uint8_t*)"{\"scan_status\":\"END\"}\n", 21);
        pCharacteristic->notify();
    }
    BLEDevice::getScan()->clearResults(); 
}

#endif