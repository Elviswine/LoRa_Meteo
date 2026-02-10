/*
 * CubeCell AB02 - Main Script (Corretto e Validato)
 */

#include "Arduino.h"
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
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
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

volatile int16_t saved_rssi;
volatile int8_t saved_snr;

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
  Serial.begin(115200);
  delay(500);

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
    DEBUG_PRINTF(PSTR("[TCA] DONE! (0x%02X)\n"), TCA.getTcaAddress());
    delay(100);
    TCA.printDiscoveryResults();
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

// Variabile statica per accumulare i cicli globali - Global scope or func
// scope? Original was func scope static. Let's make it func scope.
void runStateMachine() {
  static uint32_t g_master_cycle_count = 1; // Start from 1

  switch (g_currentState) {

  case STATE_IDLE: {
    powerUnit.powerOUTon(); // Global Power check (Vext)
    delay(50);

    if (!IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN(F("[LORA] Not Joined. Starting Join process..."));
      LoRaWAN.join();
      g_currentState = STATE_WAIT_FOR_JOIN;
    } else {
      DEBUG_PRINTLN(F("[LORA] Joined. Starting Measurement Cycle."));
      g_currentState = STATE_READ_GROUP_A;
    }
  } break;

  case STATE_WAIT_FOR_JOIN:
    static uint32_t joinStartTs = 0;
    if (joinStartTs == 0)
      joinStartTs = millis();

    if (IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN(F("[LORA] Join Success!"));
      joinStartTs = 0;
      g_currentState = STATE_READ_GROUP_A;
    } else if (millis() - joinStartTs > 60000) {
      DEBUG_PRINTLN(F("[LORA] Join Failed. Sleep."));
      joinStartTs = 0;
      g_currentState = STATE_PREPARE_SLEEP;
    }
    break;

  case STATE_READ_GROUP_A: {
    // --- GROUP A: Every 30s (Every Cycle) ---
    // Includes: Wind Speed + Rain Chain (CD4040)
    DEBUG_PRINTF(PSTR("\n=== CYCLE %d [GROUP A] ===\n"), g_master_cycle_count);

    // 1. Power ON Group A
    powerUnit.enableGroupA();
    delay(10); // Settling time

    // 2. Measure
    float wSpeed = wind.getSpeed(); // ADC
    counterUnit.measure();          // Rain

    DEBUG_PRINTF(PSTR("  >> Wind Speed Raw: %.0f\n"), wSpeed);
    DEBUG_PRINTF(PSTR("  >> Rain Count: %d\n"), g_currentCount);

    // 3. Power OFF
    powerUnit.disableAllGroups(); // Save power immediately

    // 4. Next Step?
    if (g_master_cycle_count % CYCLES_PER_GROUP_B == 0) {
      g_currentState = STATE_READ_GROUP_B;
    } else {
      g_currentState = STATE_PREPARE_SLEEP;
    }
  } break;

  case STATE_READ_GROUP_B: {
    // --- GROUP B: Every 5 min (10 Cycles) ---
    // Includes: Wind Direction
    DEBUG_PRINTLN(F("=== [GROUP B] Wind Direction ==="));

    // 1. Power ON Group B
    powerUnit.enableGroupB();
    delay(10);

    // 2. Measure
    wind.update(); // I2C AS5600
    DEBUG_PRINTF(PSTR("  >> Wind Dir: %s (%.0f deg)\n"),
                 wind.directionToString(), wind.getDirectionDegrees());

    // 3. Power OFF
    powerUnit.disableAllGroups();

    // 4. Next Step?
    if (g_master_cycle_count % CYCLES_PER_GROUP_C == 0) {
      g_currentState = STATE_READ_GROUP_C;
    } else {
      g_currentState = STATE_PREPARE_SLEEP;
    }
  } break;

  case STATE_READ_GROUP_C: {
    // --- GROUP C: Every 15 min (30 Cycles) ---
    // Includes: TCA (SHT, BME), Power Monitor (INA), DS18B20
    DEBUG_PRINTLN(F("=== [GROUP C] Environment & System ==="));

    // 1. Power ON Group C
    powerUnit.enableGroupC();
    delay(100); // More settling time for sensors

    // 2. Measure
    TCA.read();
    DS.read();
    powerUnit.readINA();
    powerUnit.readBattery();

    DEBUG_PRINTLN(F("  >> Sensors Read Done."));

    // 3. Power OFF
    powerUnit.disableAllGroups();

    // 4. Next Step
    if (DEBUG_OLED)
      g_currentState = STATE_DEBUG_OLED;
    else
      g_currentState = STATE_LORA_PREPARE;
  } break;

  case STATE_DEBUG_OLED: {
    bool fin = true;
#if DEBUG_OLED
    fin = displayUnit.refresh();
#endif
    if (fin) {
      // Only TX if we just did Group C (which we did if we are here)
      g_currentState = STATE_LORA_PREPARE;
    }
  } break;

  case STATE_LORA_PREPARE: {
    DEBUG_PRINTLN(F("[LORA] Preparing Payload..."));
    PayloadMgr.preparePayload();
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
    DEBUG_PRINTF(
        PSTR("[_^_ DEBUG: PEEP. SLEEP] loop #%d (Count is: %d, was %d)\n"),
        g_cycleCount, g_currentCount, lastDebugCount);

    // Spegnimento periferiche
    powerUnit.powerOUToff();
    delay(50);

    // Sincronizziamo il debug all'avvio
    lastDebugCount = g_currentCount;
    counterUnit.measure();
    DEBUG_PRINTF(
        PSTR("[_^_ DEBUG: SLEEP (pow. off)] loop #%d (Count is: %d, was %d)\n"),
        g_cycleCount, g_currentCount, lastDebugCount);

    g_cycleCount++;
    g_master_cycle_count++;
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
  DEBUG_PRINTF(PSTR("CATTURATO! RSSI: %d\n"),
               mcpsIndication->Rssi); // DEBUG FORTE
  saved_rssi = mcpsIndication->Rssi;
  saved_snr = mcpsIndication->Snr;
  // PayloadMgr...
}
