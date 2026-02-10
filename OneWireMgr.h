#ifndef ONEWIREMGR_H
#define ONEWIREMGR_H

#include "Arduino.h"
#include <Wire.h>
#include <Adafruit_DS248x.h>

// Definizione per la scansione (mancante nel tuo codice originale)
#define DS_SCAN_NEW_PROBE true 

// Configurazione indirizzo DS2482
#define DS2482_ADDR 0x18
#define ONEWIRE_SLOTS 8

// Indici per accesso rapido
#define IDX_T_3M 0
#define IDX_T_1M 1

// Struttura per configurare gli slot fissi
struct OneWireSlot {
    uint8_t address[8];
    uint8_t channel;
    const char* label;
    float tempC;
    bool valid;
};

class OneWireManager {
public:
    void read();
    void scan();
    void scanI2C();

    // Variabili pubbliche per accesso facile
    float t_3m = NAN;
    float t_1m = NAN;

    // Rendiamo l'array pubblico per il DisplayManager e PayloadManager
    OneWireSlot sensors[ONEWIRE_SLOTS];

private:
    bool initHardware();
    void loadConfig();
    void printResults();
    uint8_t crc8(const uint8_t *addr, uint8_t len);
};

extern OneWireManager DS;

#endif
