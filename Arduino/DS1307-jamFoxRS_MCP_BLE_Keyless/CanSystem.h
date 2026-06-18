#ifndef CAN_SYSTEM_H
#define CAN_SYSTEM_H

#include "Config.h"

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

void readCAN() {
  while(CAN0.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int rxId; unsigned char len = 0; unsigned char rxBuf[8];
    CAN0.readMsgBuf(&rxId, &len, rxBuf); rxId = rxId & 0x1FFFFFFF; canMsgCount++;
    
    lastCanPacketTime = millis();

    if (rxId == 0x0A010810 && len >= 8) {
      uint8_t m = rxBuf[1];
      uint8_t highNibble = m & 0xF0;
      uint8_t lowNibble = m & 0x0F;

      if (highNibble == 0x00) currentMode = "PARK"; 
      else if (highNibble == 0x70) currentMode = "DRIVE"; 
      else if (highNibble == 0x50) currentMode = "REVERSE"; 
      else if (highNibble == 0xB0) currentMode = "SPORT"; 
      
      brakeActive = (lowNibble & 0x02) != 0 ? 1 : 0;
      
      int parsedCruise = (lowNibble & 0x04) != 0 ? 1 : 0;
      if (parsedCruise == 1 && cruiseActive == 0) {
          showCruisePopup = true;
          lastCruiseChange = millis();
          playBeep(150, 3500); 
      }
      cruiseActive = parsedCruise;
      
      standActive = (lowNibble & 0x08) != 0 ? 1 : 0;

      if (brakeActive == 1) currentMode = "BRAKE"; 
      else if (standActive == 1) currentMode = "STAND";

      if (currentMode != lastMode) { 
          lastMode = currentMode; 
          if (modePopupEnable) { showModePopup = true; lastModeChange = millis(); } 
      }
      rpm = rxBuf[2] | (rxBuf[3] << 8); 
      
      float std_speed = rpm * 0.1033f;
      speed_kmh = (int)std_speed; 
      calc_speed = std_speed * tripCalibration; 
      
      tempCtrl = rxBuf[4]; tempMotor = rxBuf[5];
    }
    if (rxId == 0x0E6C0D09 && len >= 5) tempBatt = (rxBuf[0] + rxBuf[1] + rxBuf[2] + rxBuf[3] + rxBuf[4]) / 5;
    if (rxId == 0x0A6D0D09 && len >= 8) {
      volts = ((rxBuf[0] << 8) | rxBuf[1]) * 0.1f; int16_t iRawS = (int16_t)((rxBuf[2] << 8) | rxBuf[3]); amps = iRawS * 0.1f; if (abs(amps) < 0.2) amps = 0.0; power_watt = volts * amps;
      valRemainingCapacity = ((rxBuf[4] << 8) | rxBuf[5]) * 0.1f; valFullCapacity = ((rxBuf[6] << 8) | rxBuf[7]) * 0.1f;
    }
    if (rxId == 0x0A6E0D09 && len >= 6) { soc = (int)getSoCFromLookup((uint16_t)((rxBuf[0] << 8) | rxBuf[1])); valSOH = (int)(((rxBuf[2] << 8) | rxBuf[3]) * 0.1f); if (valSOH > 100) valSOH = 100; valCycleCount = (rxBuf[4] << 8) | rxBuf[5]; }
    if (rxId == 0x0A6F0D09 && len >= 8) { valHighestCellVolt = (rxBuf[0] << 8) | rxBuf[1]; valHighestCellNum = rxBuf[2]; valLowestCellVolt = (rxBuf[3] << 8) | rxBuf[4];  valLowestCellNum = rxBuf[5]; valAvgCellVolt = (rxBuf[6] << 8) | rxBuf[7]; }
    if (rxId == 0x0A700D09 && len >= 6) { valMaxTemp = rxBuf[0]; valMaxTempCell = rxBuf[1]; valMinTemp = rxBuf[4]; valMinTempCell = rxBuf[5]; }
    if (rxId == 0x0A730D09 && len >= 6) { valBalanceMode = rxBuf[0]; valBalanceStatus = rxBuf[1]; valBalanceBits[0] = rxBuf[2]; valBalanceBits[1] = rxBuf[3]; valBalanceBits[2] = rxBuf[4]; valBalanceBits[3] = rxBuf[5]; }
    if ((rxId & 0xFFF0FFFF) == 0x0E600D09) {
      int baseIndex = -1; switch (rxId) { case 0x0E640D09: baseIndex = 0; break; case 0x0E650D09: baseIndex = 4; break; case 0x0E660D09: baseIndex = 8; break; case 0x0E670D09: baseIndex = 12; break; case 0x0E680D09: baseIndex = 16; break; case 0x0E690D09: baseIndex = 20; break; }
      if (baseIndex >= 0) { for (int i = 0; i < 4 && (baseIndex + i) < 23; i++) { int off = i * 2; if (off + 1 < len) valCells[baseIndex + i] = (rxBuf[off] << 8) | rxBuf[off + 1]; } }
    }
    if (rxId == 0x0AB40D09 && len >= 1) bmsChargingFlag = (rxBuf[0] == 0x01);
    if (rxId == 0x0A750D09 && len >= 5) { memcpy((void*)bmsHwVersion, rxBuf, 6); bmsHwVersion[6] = '\0'; }
    if (rxId == 0x0A760D09 && len >= 5) { memcpy((void*)bmsFwVersion, rxBuf, 6); bmsFwVersion[6] = '\0'; }
    if ((rxId == 0x1810D0F3 || rxId == 0x1811D0F3) && len >= 5) { chargerCurrent = ((uint16_t)((rxBuf[2] << 8) | rxBuf[3])) * 0.1f; chargerConnected = true; lastChargerMsg = millis(); }
    if (rxId == 0x10261041) { oriChargerDetected = true; lastOriChargerMsg = millis(); }
  }
  if (millis() - lastChargerMsg > 5000) { chargerConnected = false; chargerCurrent = 0.0f; }
  if (millis() - lastOriChargerMsg > 5000) oriChargerDetected = false;
  isCharging = (chargerConnected || oriChargerDetected);
  uint32_t nowSec = millis() / 1000; if (nowSec != lastSecond) { canMessagesPerSec = canMsgCount; canMsgCount = 0; lastSecond = nowSec; }
}

#endif