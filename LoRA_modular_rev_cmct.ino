/*
 * CubeCell AB02 - Main Script (Corretto e Validato)
 */

#include "Arduino.h"
#pragma GCC optimize("-Os")
#pragma GCC optimize("-flto")
#include "LoRaWan_APP.h" // <--- IMPORTANTE: Deve essere il primo include
#include <Wire.h>

#include "Config.h"
#include "CounterManager.h"
#include "DisplayManager.h"
#include "Globals.h"
#include "LoRaPayloadManager.h"
#include "OneWireMgr.h"
#include "PowerManager.h"
#include "Wind.h"
#include "tca_i2c_manager.h"

// ========================================
// CONFIGURAZIONE LORAWAN (GLOBAL VARIABLES)
// ========================================

// 1. CHIAVI OTAA (Device EUI, App Key, App EUI)
uint8_t devEui[] = {0x22, 0x32, 0x33, 0x00, 0x00, 0x88, 0x88, 0x02};
uint8_t appKey[] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
                    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
uint8_t appEui[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 2. CHIAVI ABP
uint8_t nwkSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t appSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t devAddr = (uint32_t)0x00000000;

// 3. PARAMETRI RADIO
uint16_t userChannelsMask[6] = {0x00FF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
/*
 * set LoraWan_Region to EU868
 */
LoRaMacRegion_t loraWanRegion = LORAMAC_REGION_EU868;
DeviceClass_t loraWanClass = CLASS_A;

uint32_t appTxDutyCycle = 15000;
bool overTheAirActivation = true;
bool loraWanAdr = true;
bool isTxConfirmed = true;
uint8_t appPort = 2;
uint8_t confirmedNbTrials = 4;
bool keepNet = true; // Salva la sessione in deep sleep
// Parametri TX correnti (da aggiornare manualmente)
uint8_t current_dr = 5;    // DR5 = SF7 (sarà aggiornato da ADR)
int8_t current_txpwr = 14; // 14 dBm EU868 default

// ========================================
// ISTANZE DEGLI OGGETTI
// ========================================
PowerMes powerUnit;
CounterManager counterUnit;
DisplayManager displayUnit;
TcaI2cManager TCA;

// ========================================
// CALLBACK TIMER
// ========================================
void onWakeUpTimer() { g_wakeUpFlag = true; }

// ========================================
// SETUP
// ========================================
void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  delay(500);
#endif

  DEBUG_PRINTLN(F("\n\n===== CubeCell AB02 Monitor ====="));
  DEBUG_PRINTLN(F("Init Hardware..."));
  // Radio.SetTxConfig(MODEM_LORA, 22, 0, 125000, 7, 4, 8, false, true, 0, 0,
  // false, 3000);  // TX=14 dBm max EU

  // 1. INIT POWER & GPIO
  powerUnit.powerOUTon();

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);

  pinMode(VBAT_ADC_CTL, OUTPUT);
  digitalWrite(VBAT_ADC_CTL, HIGH);

  // 2. INIT BUS
  DEBUG_PRINTLN(F("Init I2C Bus..."));
  Wire1.begin(SENSORS_SDA, SENSORS_SCL);
  delay(200);

  // 3. INIT MODULI SENSORI
  if (DEBUG_OLED) {
    DEBUG_PRINT(F("Init Display..."));
    displayUnit.init();
    delay(100);

    DEBUG_PRINTLN(F(" DONE!"));
  }

  DEBUG_PRINTLN(F("Init Counter CD4040..."));
  counterUnit.init();
  delay(100);
  DEBUG_PRINTLN(F(" Counter DONE!"));

  DEBUG_PRINTLN(F("Init Power Measure INA 219..."));
  powerUnit.initINA();
  delay(100);
  DEBUG_PRINTLN(F(" Power DONE!"));

  DEBUG_PRINT(F("Init TCA9548A Multiplexer..."));
  TCA.setWire(Wire1);
  delay(100);
  TCA.initAsync();
  delay(100);

  if (TCA.isInitialized()) {
    DEBUG_PRINTF("[TCA] DONE! (0x%02X)\n", TCA.getTcaAddress());
    delay(100);
#if DEBUG_SERIAL
    TCA.printDiscoveryResults();
#endif
    delay(100);
  } else {
    DEBUG_PRINTLN(F(" FAILED!"));
  }

  DEBUG_PRINTLN(F("----- Init DS2482 Multiplexer..."));
  if (DS_SCAN_NEW_PROBE) {

    DS.scan();
  }
  DEBUG_PRINTLN(F("------ DONE."));

  DEBUG_PRINT(F("Init Wind Sensor..."));
  if (wind.init()) {
    DEBUG_PRINTLN(F(" DONE!"));
  } else {
    DEBUG_PRINTLN(F(" FAILED (Check wiring/address)"));
  }

  delay(2500);

  // 4. INIT LORAWAN
  DEBUG_PRINT(F("Init LoRaWAN Stack..."));
  LoRaWAN.init(loraWanClass, loraWanRegion);
  DEBUG_PRINTLN(F(" DONE!"));

  DEBUG_PRINTLN(F("---------- Setup completato! ----------\n"));

  // Inizializza il timer
  TimerInit(&g_sleepTimer, onWakeUpTimer);

  // Avvia ciclo immediato
  g_wakeUpFlag = true;
}

// ========================================
// LOOP PRINCIPALE
// ========================================
void loop() {
  // 1. Esegue la macchina a stati
  runStateMachine();

  // 2. Gestione Low Power Radio
  if (g_currentState == STATE_SLEEP_WAIT ||
      g_currentState == STATE_WAIT_FOR_JOIN) {
    LoRaWAN.sleep();
  }
}

// ========================================
// MACCHINA A STATI
// ========================================
// Variabile globale per accumulare la pioggia DURANTE il ciclo attuale
// Nota: g_currentCount useremo questa per tenere il parziale del ciclo
int g_rain_cycle_delta = 0;

void runStateMachine() {

  // Variabile statica per ricordare l'ultimo conteggio tra una chiamata e
  // l'altra
  static int lastDebugCount = 0;

  if (g_currentState == STATE_SLEEP_WAIT && !g_wakeUpFlag)
    return;

  if (g_wakeUpFlag && g_currentState == STATE_SLEEP_WAIT) {
    g_wakeUpFlag = false;
    g_currentState = STATE_IDLE;
    return;
  }

  switch (g_currentState) {

  case STATE_IDLE: {

    powerUnit.powerOUTon();
    delay(50);

    if (!IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN(F("[LORA] Not Joined. Starting Join process..."));
      LoRaWAN.join();
      g_currentState = STATE_WAIT_FOR_JOIN; // Vai alla "dogana" e aspetta
    } else {
      DEBUG_PRINTLN(F("[LORA] Already Joined."));
      g_currentState = STATE_READ_GROUP_A; // Vai diretto ai sensori
    }
  } break;

  case STATE_WAIT_FOR_JOIN:
    static uint32_t joinStartTs = 0;

    // Inizializza il timer appena entriamo in questo stato
    if (joinStartTs == 0)
      joinStartTs = millis();

    // A. CASO SUCCESSO: Siamo connessi!
    if (IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN(F("[LORA] Join Success! Proceeding to sensors..."));
      joinStartTs = 0; // Reset timer
      g_currentState =
          STATE_READ_GROUP_A; // ORA possiamo leggere i sensori sicuri
    }
    // B. CASO TIMEOUT: Se dopo 60 secondi non si collega (Gateway spento?),
    // dormiamo
    else if (millis() - joinStartTs > 60000) {
      DEBUG_PRINTLN(F("[LORA] Join Failed (Timeout). Sleep and retry later."));
      joinStartTs = 0; // Reset timer
      g_currentState =
          STATE_PREPARE_SLEEP; // Saltiamo tutto e riproviamo al prossimo ciclo
    }
    // C. CASO ATTESA: Rimaniamo qui (il loop chiamerà LoRaWAN.sleep())
    else {
      DEBUG_PRINT(F("."));
      // Non serve fare altro, il LoRaWAN.sleep() nel loop gestirà RX1/RX2
    }
    break;

  case STATE_READ_GROUP_A: {
    DEBUG_PRINTLN(F("---------------- READ GROUP A. ---------------"));

    powerUnit.readINA();
    powerUnit.readBattery();
    TCA.read();
    DS.read();
    // Sincronizziamo il debug
    lastDebugCount = g_currentCount;
    counterUnit.measure();
    DEBUG_PRINTF("[_^_ DEBUG: READ A] loop #%d (Count is: %d, was %d)\n",
                 g_cycleCount, g_currentCount, lastDebugCount);
    // Lettura Vento (AGGIUNGI QUESTO)
    wind.update();

    g_currentState = STATE_READ_GROUP_B;
  } break;

  case STATE_READ_GROUP_B:
    g_currentState = STATE_READ_GROUP_C;
    break;

  case STATE_READ_GROUP_C:
    if (DEBUG_OLED)
      g_currentState = STATE_DEBUG_OLED;
    else
      g_currentState = STATE_LORA_PREPARE;
    break;

  case STATE_DEBUG_OLED: {
    bool fin = true;
#if DEBUG_OLED
    fin = displayUnit.refresh();
#endif
    if (fin) {
      if (g_cycleCount % 2 != 0) {
        g_currentState = STATE_LORA_PREPARE;
      } else {
        DEBUG_PRINTLN(F("[DBG] Skip TX (Test)."));
        g_currentState = STATE_PREPARE_SLEEP;
      }
    }
  } break;

  case STATE_LORA_PREPARE: {
    DEBUG_PRINTLN(F("---STATE_LORA_PREPARE----"));

    DEBUG_PRINTLN(F("[LORA] Finalizing Payload..."));
    PayloadMgr.preparePayload();

    // Aggiorna parametri TX (per visualizzazione su OLED)
    PayloadMgr.updateTxParamsFromMAC();

    memcpy(appData, PayloadMgr.getBuffer(), PayloadMgr.getSize());
    appDataSize = PayloadMgr.getSize();

    g_currentState = STATE_LORA_SEND;
  } break;

  case STATE_LORA_SEND:

    if (IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN(F("[LORA] Sending packet (Background)..."));

      MlmeReq_t mlmeReq;
      mlmeReq.Type = MLME_LINK_CHECK;
      LoRaMacMlmeRequest(&mlmeReq);
      PayloadMgr.captureTxParams(current_dr, current_txpwr);

      LoRaWAN.send();

    } else {
      DEBUG_PRINTLN(F("[LORA] Network not joined. Skip TX."));
      // Opzionale: lanciare un join se non è già in corso
      PayloadMgr.handleTxTimeout();
      g_currentState = STATE_WAIT_FOR_JOIN;
      break;
    }

    g_currentState = STATE_PREPARE_SLEEP;
    break;

  case STATE_PREPARE_SLEEP: {
    DEBUG_PRINTLN(F("---PREPARE_SLEEP----"));

    // Sincronizziamo il debug all'avvio
    lastDebugCount = g_currentCount;
    counterUnit.measure();
    DEBUG_PRINTF("[_^_ DEBUG: PEEP. SLEEP] loop #%d (Count is: %d, was %d)\n",
                 g_cycleCount, g_currentCount, lastDebugCount);

    // Spegnimento periferiche
    powerUnit.powerOUToff();
    delay(50);

    // Sincronizziamo il debug all'avvio
    lastDebugCount = g_currentCount;
    counterUnit.measure();
    DEBUG_PRINTF(
        "[_^_ DEBUG: SLEEP (pow. off)] loop #%d (Count is: %d, was %d)\n",
        g_cycleCount, g_currentCount, lastDebugCount);

    g_cycleCount++;
    TimerSetValue(&g_sleepTimer, TIME_UNIT_MS);
    TimerStart(&g_sleepTimer);
    g_wakeUpFlag = false;
    g_currentState = STATE_SLEEP_WAIT;
  } break;

  case STATE_SLEEP_WAIT:
    break;

  default:
    g_currentState = STATE_IDLE;
    break;
  }
}

void downLinkDataHandle(McpsIndication_t *mcpsIndication) {
  // Verifica che il puntatore sia valido
  if (mcpsIndication == nullptr) {
    DEBUG_PRINTLN(F("[DOWNLINK] ERROR: mcpsIndication is NULL"));
    return;
  }

  // Log del downlink ricevuto
  DEBUG_PRINTLN(F("\n[DOWNLINK] ========== DOWNLINK RECEIVED =========="));
  DEBUG_PRINTF("[DOWNLINK] Status: %d\n", mcpsIndication->Status);
  DEBUG_PRINTF("[DOWNLINK] RSSI: %d dBm\n", mcpsIndication->Rssi);
  DEBUG_PRINTF("[DOWNLINK] SNR: %d dB\n", mcpsIndication->Snr);
  DEBUG_PRINTF("[DOWNLINK] RxDatarate: DR%d\n", mcpsIndication->RxDatarate);

  // Salva SEMPRE i metadati (anche se lo status non è OK, per debug)
  saved_rssi = mcpsIndication->Rssi;
  saved_snr = mcpsIndication->Snr;
  saved_datarate = mcpsIndication->RxDatarate;
  saved_status = mcpsIndication->Status;

  // Verifica se lo status è OK (0 = LORAMAC_EVENT_INFO_STATUS_OK)
  if (mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_OK) {
    DEBUG_PRINTLN(F("[DOWNLINK] Status: OK - Downlink valido!"));

    // Se ci sono dati nel payload, puoi processarli qui
    if (mcpsIndication->BufferSize > 0) {
      DEBUG_PRINTF("[DOWNLINK] Payload size: %d bytes\n",
                   mcpsIndication->BufferSize);
    }
  } else {
    DEBUG_PRINTF("[DOWNLINK] Status: ERROR (%d)\n", mcpsIndication->Status);
  }

  DEBUG_PRINTLN(F("[DOWNLINK] =========================================\n"));
}
