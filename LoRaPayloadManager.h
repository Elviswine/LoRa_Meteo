#ifndef LORA_PAYLOAD_MANAGER_H
#define LORA_PAYLOAD_MANAGER_H

#include <Arduino.h>
#include "LoRaWan_APP.h"

// Struttura dei dati ottimizzata (Packed)
// Totale: 25 Byte (aggiunto windDirection)
typedef struct __attribute__((packed)) {
  int16_t temp1;       // T1 x100 (I2C CH0)
  uint8_t hum1;        // H1 (I2C CH0)
  int16_t temp2;       // T2 x100 (I2C CH1)
  uint8_t hum2;        // H2 (I2C CH1)
  uint16_t pres2;      // P2 (bme280) x10
  int16_t temp3;       // T3 x100 (Aux)
  uint8_t hum3;        // H3 (Aux)
  int16_t tempDS_Air;  // DS18B20 3m (x100)
  int16_t tempDS_Gnd;  // DS18B20 1m (x100)
  uint8_t rainCount;   // Rain Counter Raw
  uint8_t windDirection; // Wind Direction (0-15: N, NNE, NE, ..., NNW) *** NUOVO ***
  uint16_t batt_mV;    // Battery mV
  uint16_t solar_mV;   // Solar Panel mV
  uint16_t solar_mA;   // Solar Panel mA
  uint16_t adc2_mV;    // ADC2 mV (Aux)
  uint16_t adc3_mV;    // ADC3 mV (Aux)
} AppPayload;

// ========================================
// CLASSE UNIFICATA
// ========================================

class LoRaPayloadManager {
public:
  LoRaPayloadManager();

  // Raccoglie i dati dalle globali e prepara il pacchetto
  void preparePayload();

  // Ritorna il puntatore al buffer dati (per LoRaWAN.send)
  uint8_t* getBuffer();

  // Ritorna la dimensione del buffer
  uint8_t getSize();

  // Stampa di debug su Serial
  void debugPrint();

  // ========================================
  // FUNZIONI PER DEBUG LORAWAN
  // ========================================

  // Callback da chiamare quando arriva un downlink
  void handleDownlink(McpsIndication_t *mcpsIndication);

  // Callback da chiamare dopo TX completata
  void handleTxDone();

  // Callback da chiamare in caso di TX timeout
  void handleTxTimeout();

  // Callback da chiamare quando ricevi ACK (confirmed message)
  void handleTxConfirmed();

  // Cattura parametri TX prima dell'invio
  void captureTxParams(uint8_t datarate, int8_t txPower);

  // Leggi parametri dalla LoRaMAC
  void updateTxParamsFromMAC();

  // ========================================
  // GETTERS per i parametri
  // ========================================

  // Getters per debug TX/RX (usano variabili private)
  uint8_t getLastDataRate() const { return last_datarate; }
  int8_t getLastTxPower() const { return last_txpower; }
  int16_t getLastRSSI() const { return last_rssi; }
  int8_t getLastSNR() const { return last_snr; }
  uint32_t getUplinkCounter() const { return uplink_counter; }
  uint32_t getDownlinkCounter() const { return downlink_counter; }
  bool getLastTxSuccess() const { return last_tx_success; }
  bool isNetworkJoined() const { return IsLoRaMacNetworkJoined; }

  // Getters dalle configurazioni globali del sistema
  uint8_t getConfiguredDataRate() const {
    extern uint8_t current_dr;
    return current_dr;
  }

  uint8_t getConfiguredSF() const {
    return 12 - getConfiguredDataRate();
  }

  int8_t getConfiguredTxPower() const {
    extern int8_t current_txpwr;
    return current_txpwr;
  }

  bool getAdrEnabled() const {
    extern bool loraWanAdr;
    return loraWanAdr;
  }

  DeviceClass_t getDeviceClass() const {
    extern DeviceClass_t loraWanClass;
    return loraWanClass;
  }

  uint16_t getDutyCycle() const {
    extern uint32_t appTxDutyCycle;
    return appTxDutyCycle/1000;
  }

private:
  AppPayload payload;

  // Helper interni per encoding
  int16_t encodeTemp(float val);
  uint8_t encodeHum(float val);
  uint16_t encodePres(float val);
  uint8_t encodeWindDir(uint8_t direction); // *** NUOVO ***
  void getTcaSensorData(uint8_t channel, float &t, float &h, float &p);

  // ========================================
  // VARIABILI PRIVATE PER DEBUG LORAWAN
  // ========================================
  uint8_t last_datarate;       // DR0-DR15
  int8_t last_txpower;          // Potenza TX in dBm
  int16_t last_rssi;            // RSSI downlink in dBm
  int8_t last_snr;              // SNR downlink in dB
  uint32_t uplink_counter;      // Frame counter uplink
  uint32_t downlink_counter;    // Frame counter downlink
  bool last_tx_success;         // Stato ultima TX
};

// Istanza globale accessibile ovunque
extern LoRaPayloadManager PayloadMgr;

#endif
