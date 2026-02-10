#include "CounterManager.h"

void CounterManager::init() {
  Wire1.begin(SENSORS_SDA, SENSORS_SCL);

  pinMode(PIN_CD4040_RST, OUTPUT);
  digitalWrite(PIN_CD4040_RST, LOW);
  // resetHardware();
}

void CounterManager::resetHardware() {
  digitalWrite(PIN_CD4040_RST, HIGH);
  delay(10);
  digitalWrite(PIN_CD4040_RST, LOW);
}

void CounterManager::measure() {
  Wire1.requestFrom(ADDR_COUNTER, 1);

  if (Wire1.available()) {
    byte val = Wire1.read();

    // Salviamo il vecchio valore prima di sovrascriverlo
    g_lastCountValue = g_currentCount;
    g_currentCount = (int)val;

    // Logghiamo il cambio di stato
    DEBUG_PRINTF(PSTR("[RAIN] Counter is: %d was: %d\n"), g_currentCount,
                 g_lastCountValue);

  } else {
    DEBUG_PRINTLN(F("[RAIN] Error: I2C Not Available!"));
    g_currentCount = -1; // Codice errore
  }
}
