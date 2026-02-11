#ifndef LORA_PAYLOAD_MANAGER_H
#define LORA_PAYLOAD_MANAGER_H

#include "LoRaWan_APP.h"
#include <Arduino.h>

// Struttura dei dati ottimizzata (Packed)
// Totale: 25 Byte (aggiunto windDirection)
typedef struct __attribute__((packed)) {
  int16_t temp1;         // T1 x100 (I2C CH0)
  uint8_t hum1;          // H1 (I2C CH0)
  int16_t temp2;         // T2 x100 (I2C CH1)
  uint8_t hum2;          // H2 (I2C CH1)
  uint16_t pres2;        // P2 (bme280) x10
  int16_t temp3;         // T3 x100 (Aux)
  uint8_t hum3;          // H3 (Aux)
  int16_t tempDS_Air;    // DS18B20 3m (x100)
  int16_t tempDS_Gnd;    // DS18B20 1m (x100)
  uint8_t rainCount;     // Rain Counter Raw
  uint8_t windDirection; // Wind Direction (0-15: N, NNE, NE, ..., NNW) ***
  uint16_t batt_mV;      // Battery mV
  uint16_t solar_mV;     // Solar Panel mV
  uint16_t solar_mA;     // Solar Panel mA
  uint16_t adc2_mV;      // ADC2 mV (Aux)
  uint16_t adc3_mV;      // ADC3 mV (Aux)
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
  uint8_t *getBuffer();

  // Ritorna la dimensione del buffer
  uint8_t getSize();

  // Stampa di debug su Serial
  void debugPrint();

  // ========================================
  // FUNZIONI PER DEBUG LORAWAN
  // ========================================

  // ========================================
  // GETTERS per i parametri
  // ========================================

  // Getters per debug TX/RX (usano variabili private)
  uint32_t getUplinkCounter() const { return uplink_counter; }
  uint32_t getDownlinkCounter() const { return downlink_counter; }
  bool getLastTxStatus() const { return last_tx_success; }
  bool isNetworkJoined() const { return IsLoRaMacNetworkJoined; }

private:
  AppPayload payload;

  // Helper interni per encoding
  int16_t encodeTemp(float val);
  uint8_t encodeHum(float val);
  uint16_t encodePres(float val);
  uint8_t encodeWindDir(float degrees); // *** NUOVO: Gradi 0-360 ***
  void getTcaSensorData(uint8_t channel, float &t, float &h, float &p);

  // ========================================
  // VARIABILI PRIVATE PER DEBUG LORAWAN
  // ========================================
  uint32_t uplink_counter;   // Frame counter uplink
  uint32_t downlink_counter; // Frame counter downlink
  bool last_tx_success;      // Stato ultima TX
};

// Istanza globale accessibile ovunque
extern LoRaPayloadManager PayloadMgr;

#endif
