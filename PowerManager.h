#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include "Config.h"
#include "Globals.h"
#include <Arduino.h>
#include <Wire.h>

class PowerMes {
public:
  void initINA();
  void readINA();
  void readBattery();
  uint16_t readBatteryCompensated();
  uint8_t getBatteryPercent(uint16_t voltage_mv);

  void powerOUToff();
  void powerOUTon();

  // Controllo granulare gruppi
  void powerT1on();
  void powerT1off();
  void powerT2on();
  void powerT2off();
  void powerT3on();
  void powerT3off();

private:
  void setMuxChannel(byte channel);
  void writeReg16(byte addr, byte reg, uint16_t val);
  float readINA_mV();
  float readINA_mA();
  uint16_t readBatteryRaw();
};

#endif
