#include "LoRaPayloadManager.h"

// Inclusione necessaria per variabili globali generali (g_battery_mV ecc.)
#include "Globals.h"
#include "Config.h"
#include "OneWireMgr.h"
#include "Wind.h" // *** NUOVO: Per accedere all'oggetto wind ***

// --- FIX PER ERRORE COMPILAZIONE ---
// Dichiarazione esplicita delle variabili esterne definite altrove (es. nel main o tca manager)
extern bool bme280_online[];
extern float bme280_temperature[];
extern float bme280_humidity[];
extern float bme280_pressure[];

extern bool sht4x_online[];
extern float sht4x_temperature[];
extern float sht4x_humidity[];

extern bool sht3x_online[];
extern float sht3x_temperature[];
extern float sht3x_humidity[];
// -----------------------------------

LoRaPayloadManager PayloadMgr;

LoRaPayloadManager::LoRaPayloadManager() {
  memset(&payload, 0, sizeof(AppPayload));
  last_datarate = 0;
  last_txpower = 14; // Default EU868
  last_rssi = 0;
  last_snr = 0;
  uplink_counter = 0;
  downlink_counter = 0;
  last_tx_success = false;
}

int16_t LoRaPayloadManager::encodeTemp(float val) {
  if (isnan(val)) return -32768; // Error code (int16 min)
  return (int16_t)(val * 100.0);
}

uint8_t LoRaPayloadManager::encodeHum(float val) {
  if (isnan(val)) return 255; // Error code
  if (val > 100) return 100;
  if (val < 0) return 0;
  return (uint8_t)round(val);
}

uint16_t LoRaPayloadManager::encodePres(float val) {
  if (isnan(val) || val <= 0) return 0;
  // Esempio: 931.9 hPa -> 9319
  return (uint16_t)(val * 10.0);
}

// *** NUOVO: Encoding Direzione Vento ***
uint8_t LoRaPayloadManager::encodeWindDir(uint8_t direction) {
  // Direzione già codificata come enum 0-15
  // Valore 255 = errore/non disponibile
  if (direction > 15) return 255;
  return direction;
}

void LoRaPayloadManager::getTcaSensorData(uint8_t channel, float &t, float &h, float &p) {
  // Init valori a NAN di default
  t = NAN; h = NAN; p = NAN;

  if (channel > 1) return;

  if (bme280_online[channel]) {
    t = bme280_temperature[channel];
    h = bme280_humidity[channel];
    p = bme280_pressure[channel]; // <--- BME ha la pressione
  }
  else if (sht4x_online[channel]) {
    t = sht4x_temperature[channel];
    h = sht4x_humidity[channel];
    p = NAN; // SHT4x non ha pressione
  }
  else if (sht3x_online[channel]) {
    t = sht3x_temperature[channel];
    h = sht3x_humidity[channel];
    p = NAN; // SHT3x non ha pressione
  }
}

void LoRaPayloadManager::preparePayload() {
  float t, h, p;

  // 1. Sensori I2C (CH0)
  getTcaSensorData(0, t, h, p);
  payload.temp1 = encodeTemp(t);
  payload.hum1 = encodeHum(h);

  // 2. Sensori I2C (CH1)
  getTcaSensorData(1, t, h, p);
  payload.temp2 = encodeTemp(t);
  payload.hum2 = encodeHum(h);
  payload.pres2 = encodePres(p);

  // 3. Terza coppia (Placeholder)
  payload.temp3 = -32768;
  payload.hum3 = 255;

  // 4. Sensori OneWire (DS18B20)
  // Usa l'oggetto DS globale definito in OneWireMgr
  float t_air = DS.sensors[IDX_T_3M].valid ? DS.sensors[IDX_T_3M].tempC : NAN;
  float t_gnd = DS.sensors[IDX_T_1M].valid ? DS.sensors[IDX_T_1M].tempC : NAN;
  payload.tempDS_Air = encodeTemp(t_air);
  payload.tempDS_Gnd = encodeTemp(t_gnd);

  // 5. Contatore Pioggia
  payload.rainCount = (uint8_t)g_currentCount;

  // *** 6. DIREZIONE VENTO (NUOVO) ***
  payload.windDirection = encodeWindDir((uint8_t)wind.getDirection());

  // 7. Power Management (mV / mA)
  payload.batt_mV = (uint16_t)g_battery_mV;
  payload.solar_mV = (uint16_t)g_loadVoltage_mV;
  payload.solar_mA = (uint16_t)g_loadCurrent_mA;

  // 8. ADC Aux (Dummy values 0xFFFF = 65535)
  // Valore 'safe' per indicare "non connesso" su unsigned int
  payload.adc2_mV = 65535;
  payload.adc3_mV = 65535;
}

uint8_t* LoRaPayloadManager::getBuffer() {
  return (uint8_t*)&payload;
}

uint8_t LoRaPayloadManager::getSize() {
  return sizeof(AppPayload);
}

void LoRaPayloadManager::debugPrint() {
  Serial.println("\n--- LORA PAYLOAD DEBUG ---");
  Serial.printf("[LORA] CH0 T: %d, H: %d\n", payload.temp1, payload.hum1);
  Serial.printf("[LORA] CH1 T: %d, H: %d\n", payload.temp2, payload.hum2);
  Serial.printf("[LORA] DS Air: %d, Gnd: %d\n", payload.tempDS_Air, payload.tempDS_Gnd);
  Serial.printf("[LORA] Rain: %d\n", payload.rainCount);
  
  // *** NUOVO: Stampa Direzione Vento ***
  Serial.printf("[LORA] Wind: %s (%d)\n", wind.directionToString(), payload.windDirection);
  
  Serial.printf("[LORA] Pwr: B=%d mV, S=%d mV, I=%d mA\n",
                payload.batt_mV, payload.solar_mV, payload.solar_mA);
  Serial.printf("[LORA] Total Size: %d bytes\n", sizeof(AppPayload));
  Serial.println("--------------------------\n");
}

void LoRaPayloadManager::handleDownlink(McpsIndication_t *mcpsIndication) {
  Serial.printf("[LoRa] DOWNLINK: port %d, rssi %d, snr %d\n",
                mcpsIndication->Port,
                mcpsIndication->Rssi,
                mcpsIndication->Snr);

  // Cattura parametri downlink
  last_rssi = mcpsIndication->Rssi;
  last_snr = mcpsIndication->Snr;
  downlink_counter++;

  // Processa payload se necessario
  if(mcpsIndication->BufferSize > 0) {
    Serial.print("[LoRa] RX Data: ");
    for(uint8_t i = 0; i < mcpsIndication->BufferSize; i++) {
      Serial.printf("%02X ", mcpsIndication->Buffer[i]);
    }
    Serial.println();
    // QUI puoi aggiungere logica per processare comandi downlink
    // Es: if(mcpsIndication->Port == 10) { ... }
  }
}

void LoRaPayloadManager::handleTxDone() {
  Serial.println("[LoRa] TX Done");
  last_tx_success = true;
  uplink_counter++;
}

void LoRaPayloadManager::handleTxTimeout() {
  Serial.println("[LoRa] TX Timeout");
  last_tx_success = false;
}

void LoRaPayloadManager::handleTxConfirmed() {
  Serial.println("[LoRa] TX Confirmed (ACK received)");
  // Opzionale: potresti incrementare un contatore separato per ACK
}

void LoRaPayloadManager::captureTxParams(uint8_t datarate, int8_t txPower) {
  last_datarate = datarate;
  last_txpower = txPower;
  last_tx_success = false; // Reset, verrà impostato da handleTxDone()
  Serial.printf("[LoRa] TX Params: DR=%d, TxPower=%d dBm\n", datarate, txPower);
}

void LoRaPayloadManager::updateTxParamsFromMAC() {
  MibRequestConfirm_t mibReq;

  // 1. Chiedi al sistema: Quale DR stai usando ORA?
  mibReq.Type = MIB_CHANNELS_DATARATE;
  if(LoRaMacMibGetRequestConfirm(&mibReq) == LORAMAC_STATUS_OK) {
    last_datarate = mibReq.Param.ChannelsDatarate;
  }

  // 2. Chiedi al sistema: A che potenza trasmetti?
  mibReq.Type = MIB_CHANNELS_TX_POWER;
  if(LoRaMacMibGetRequestConfirm(&mibReq) == LORAMAC_STATUS_OK) {
    last_txpower = mibReq.Param.ChannelsTxPower;
  }
}
