#ifndef BUZZER_SYSTEM_H
#define BUZZER_SYSTEM_H

#include "Config.h"

inline void setBuzzer(bool state, int freq) {
  if (!soundEnabled) { 
      if (isBuzzerOn) { digitalWrite(BUZZER_PIN, LOW); noTone(BUZZER_PIN); isBuzzerOn = false; }
      return; 
  }
  if (state && !isBuzzerOn) { 
      if (buzzerIsPassive) tone(BUZZER_PIN, freq); else digitalWrite(BUZZER_PIN, HIGH);
      isBuzzerOn = true;
  } 
  else if (!state && isBuzzerOn) {
      if (buzzerIsPassive) noTone(BUZZER_PIN); else digitalWrite(BUZZER_PIN, LOW);
      isBuzzerOn = false;
  }
}

inline void playBeep(int duration, int freq) {
  if (!soundEnabled) return;
  if (buzzerIsPassive) { 
      tone(BUZZER_PIN, freq); delay(duration); noTone(BUZZER_PIN); 
  } else { 
      digitalWrite(BUZZER_PIN, HIGH); delay(duration); digitalWrite(BUZZER_PIN, LOW); 
  }
}

inline void handleBuzzer() {
  if (!soundEnabled) { setBuzzer(false); return; }
  unsigned long now = millis();
  
  if (beepEndTime > now) { setBuzzer(true, 3000); return; } 
  if (buzzerSpeedWarnEnable && speed_kmh >= buzzerSpeedWarnLimit) { setBuzzer(((now % 300) < 100), 3000); return; }
  if (currentMode == "REVERSE") { setBuzzer(((now % 1000) < 300), 2000); return; }
  
  setBuzzer(false);
}

#endif