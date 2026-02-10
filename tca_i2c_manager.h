#ifndef TCA_I2C_MANAGER_H
#define TCA_I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BME280.h>

// ============================================================================
// CONFIGURAZIONE
// ============================================================================

enum SensorType : uint8_t {
    SENS_NONE = 0,
    SENS_SHT3X,
    SENS_SHT4X,
    SENS_BME280
};

// Indirizzo di default del TCA9548A (può essere auto-detectato)
#define TCA_ADDR_DEFAULT 0x71
#define TCA_NUM_CHANNELS 8

// Possibili indirizzi sensori (auto-detect)
static const uint8_t SHT3X_ADDRESSES[] = {0x44, 0x45};
static const uint8_t SHT4X_ADDRESSES[] = {0x44, 0x45};  // SHT4X usa stessi indirizzi
static const uint8_t BME280_ADDRESSES[] = {0x76, 0x77};

// Config canali (MODIFICA QUI per cambiare sensori)
// Esempio: CH0=SHT3X, CH1=BME280, CH2=SHT4X, CH3=SHT3X (secondo)
static const SensorType TCA_CH_TYPE[TCA_NUM_CHANNELS] = {
    SENS_SHT3X,  // CH0
    SENS_BME280, // CH1
    SENS_NONE,   // CH2
    SENS_NONE,   // CH3
    SENS_NONE,   // CH4
    SENS_NONE,   // CH5
    SENS_NONE,   // CH6
    SENS_NONE    // CH7
};

// Indirizzi scoperti (riempiti da initAsync)
extern uint8_t TCA_CH_ADDR[TCA_NUM_CHANNELS];

// ============================================================================
// VARIABILI GLOBALI - DATI SENSORI (globali per accesso facile dal main)
// ============================================================================

// SHT3X (su qualsiasi canale configurato SENS_SHT3X)
extern float sht3x_temperature[TCA_NUM_CHANNELS];
extern float sht3x_humidity[TCA_NUM_CHANNELS];
extern bool  sht3x_online[TCA_NUM_CHANNELS];

// SHT4X (su qualsiasi canale configurato SENS_SHT4X)
extern float sht4x_temperature[TCA_NUM_CHANNELS];
extern float sht4x_humidity[TCA_NUM_CHANNELS];
extern bool  sht4x_online[TCA_NUM_CHANNELS];

// BME280 (su qualsiasi canale configurato SENS_BME280)
extern float bme280_temperature[TCA_NUM_CHANNELS];
extern float bme280_humidity[TCA_NUM_CHANNELS];
extern float bme280_pressure[TCA_NUM_CHANNELS];
extern bool  bme280_online[TCA_NUM_CHANNELS];

// ============================================================================
// CLASSE MANAGER
// ============================================================================

class TcaI2cManager {
public:
    TcaI2cManager();

    // Imposta il bus I2C da usare (es. Wire1)
    void setWire(TwoWire &w);

    // Esegue auto-detect del TCA e dei sensori su ciascun canale
    void initAsync();

    bool isInitialized() const;
    
    // Getter per l'indirizzo TCA (public, per stampa nel setup)
    uint8_t getTcaAddress() const { return _tca_addr; }

    // Legge i sensori configurati e aggiorna le variabili globali
    void read();

    // Stampa tabella dei sensori scoperti
    void printDiscoveryResults();

    // Stampa lo stato attuale (valori letti / OFFLINE)
    void printStatus();

    // Scansione "grezza" di tutti gli indirizzi su ogni canale (debug)
    void scanChannels();

private:
    // Basso livello TCA
    bool detectTcaAddress();
    bool selectChannel(uint8_t ch);
    bool probe(uint8_t addr);

    // Discovery per singolo canale
    uint8_t discoverSensor(uint8_t ch, SensorType type);

    // Lettura sensori
    void readSHT3X(uint8_t ch);
    void readSHT4X(uint8_t ch);
    void readBME280(uint8_t ch);

private:
    TwoWire *_wire;                         // puntatore al bus I2C in uso
    uint8_t  _tca_addr;                     // indirizzo TCA9548A trovato
    bool     _tca_initialized;              // true se initAsync completata
    bool     _channel_online[TCA_NUM_CHANNELS]; // true se sensore OK sul canale
};

#endif
