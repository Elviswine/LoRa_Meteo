#ifndef COUNTERMANAGER_H
#define COUNTERMANAGER_H

#include "Config.h"
#include "Globals.h"
#include <Wire.h>

class CounterManager {
  public:
    void init();
    void update();
    void resetHardware(); // Reset fisico del CD4040
    void measure();       // Legge I2C e aggiorna g_currentCount
};

#endif
