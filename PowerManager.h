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

  // New Group Control Methods
  void enableGroupA(); // Wind Speed (Check DEBUG_OLED)
  void enableGroupB(); // Wind Direction
  void enableGroupC(); // Env Sensors
  void disableAllGroups();

private:
  void setMuxChannel(byte channel);
  void writeReg16(byte addr, byte reg, uint16_t val);
  float readINA_mV();
  float readINA_mA();
  uint16_t readBatteryRaw();
};

#endif
