#ifndef EEPROM_SYSTEM_H
#define EEPROM_SYSTEM_H

#include "Config.h"

void writeEEPROMFloat(unsigned int addr, float val) {
  byte* p = (byte*)(void*)&val;
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(addr >> 8)); Wire.write((int)(addr & 0xFF));
  for (byte i = 0; i < 4; i++) { Wire.write(*p++); }
  Wire.endTransmission(); delay(10); 
}

float readEEPROMFloat(unsigned int addr) {
  float val = 0.0; byte* p = (byte*)(void*)&val;
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)(addr >> 8)); Wire.write((int)(addr & 0xFF));
  if(Wire.endTransmission() != 0) return 0.0; 
  
  Wire.requestFrom((uint16_t)EEPROM_ADDR, (uint8_t)4);
  int timeout = 0;
  while(Wire.available() < 4 && timeout < 100) { delay(1); timeout++; } 
  
  if (Wire.available() >= 4) {
      for (byte i = 0; i < 4; i++) { *p++ = Wire.read(); }
  }
  uint32_t *check = (uint32_t*)&val;
  if(*check == 0xFFFFFFFF || isnan(val)) return 0.0;
  return val;
}

#endif