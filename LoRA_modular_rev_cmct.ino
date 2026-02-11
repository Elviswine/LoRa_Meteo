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

  DEBUG_PRINTLN("\n\n===== CubeCell AB02 Monitor =====");
  DEBUG_PRINTLN("Init Hardware...");
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
  DEBUG_PRINTLN("Init I2C Bus...");
  Wire1.begin(SENSORS_SDA, SENSORS_SCL);
  delay(200);

  // 3. INIT MODULI SENSORI
  if (DEBUG_OLED) {
    DEBUG_PRINT("Init Display...");
    displayUnit.init();
    delay(100);

    DEBUG_PRINTLN(" DONE!");
  }

  DEBUG_PRINTLN("Init Counter CD4040...");
  counterUnit.init();
  delay(100);
  DEBUG_PRINTLN(" Counter DONE!");

  DEBUG_PRINTLN("Init Power Measure INA 219...");
  powerUnit.initINA();
  delay(100);
  DEBUG_PRINTLN(" Power DONE!");

  DEBUG_PRINT("Init TCA9548A Multiplexer...");
  TCA.setWire(Wire1);
  delay(100);
  TCA.initAsync();
  delay(100);

  if (TCA.isInitialized()) {
    DEBUG_PRINTF("[TCA] DONE! (0x%02X)\n", TCA.getTcaAddress());
    delay(100);
    TCA.printDiscoveryResults();
    delay(100);
  } else {
    DEBUG_PRINTLN(" FAILED!");
  }

  DEBUG_PRINTLN("----- Init DS2482 Multiplexer...");
  if (DS_SCAN_NEW_PROBE) {

    DS.scan();
  }
  DEBUG_PRINTLN("------ DONE.");

  DEBUG_PRINT("Init Wind Sensor...");
  if (wind.init()) {
    DEBUG_PRINTLN(" DONE!");
  } else {
    DEBUG_PRINTLN(" FAILED (Check wiring/address)");
  }

  delay(2500);

  // 4. INIT LORAWAN
  DEBUG_PRINT("Init LoRaWAN Stack...");
  LoRaWAN.init(loraWanClass, loraWanRegion);
  DEBUG_PRINTLN(" DONE!");

  DEBUG_PRINTLN("---------- Setup completato! ----------\n");

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
    g_cycleCount++;

    // Calcolo X.Y.Z
    // X = g_txCount + 1 (Ciclo di invio attuale)
    // Y = Ciclo base all'interno del blocco TX (1 a LORA_TX_MULT)
    // Z = Ciclo base all'interno del blocco B (1 a GROUP_B_MULT)
    uint32_t X = g_txCount + 1;
    uint32_t Y = ((g_cycleCount - 1) % LORA_TX_MULT) + 1;
    uint32_t Z = ((g_cycleCount - 1) % GROUP_B_MULT) + 1;

    DEBUG_PRINTF("\n\n>>> WAKE UP! Counter %d.%d.%d (Total Cycles: %d) <<<\n",
                 X, Y, Z, g_cycleCount);

    if (!IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN("[LORA] Not Joined. Starting Join process...");
      LoRaWAN.join();
      g_currentState = STATE_WAIT_FOR_JOIN;
    } else {
      g_currentState = STATE_READ_GROUP_A;
    }
  } break;

  case STATE_WAIT_FOR_JOIN:
    static uint32_t joinStartTs = 0;

    // Inizializza il timer appena entriamo in questo stato
    if (joinStartTs == 0)
      joinStartTs = millis();

    // A. CASO SUCCESSO: Siamo connessi!
    if (IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN("[LORA] Join Success! Proceeding to sensors...");
      joinStartTs = 0; // Reset timer
      g_currentState =
          STATE_READ_GROUP_A; // ORA possiamo leggere i sensori sicuri
    }
    // B. CASO TIMEOUT: Se dopo 60 secondi non si collega (Gateway spento?),
    // dormiamo
    else if (millis() - joinStartTs > 60000) {
      DEBUG_PRINTLN("[LORA] Join Failed (Timeout). Sleep and retry later.");
      joinStartTs = 0; // Reset timer
      g_currentState =
          STATE_PREPARE_SLEEP; // Saltiamo tutto e riproviamo al prossimo ciclo
    }
    // C. CASO ATTESA: Rimaniamo qui (il loop chiamerà LoRaWAN.sleep())
    else {
      DEBUG_PRINT(".");
      // Non serve fare altro, il LoRaWAN.sleep() nel loop gestirà RX1/RX2
    }
    break;

  case STATE_READ_GROUP_A: {
    DEBUG_PRINTLN("---------------- READ GROUP A (T1: Wind Speed + ADC2) "
                  "---------------");

    powerUnit.powerT1on();

    // Misura velocità vento (Counter Hardware)
    counterUnit.measure();

    // Lettura ADC 2 (Anemometro analogico o Aux)
    uint16_t adcRaw = analogRead(ADC_WIND_S);
    g_adc2_mV = (uint16_t)((adcRaw * 2400.0) / 4096.0);

    // Accumulo per media
    g_adc2_sum += (float)g_adc2_mV;
    g_adc2_count++;

    DEBUG_PRINTF("[READ A] ADC2: %d mV (Accumulated: %.1f, Count: %d)\n",
                 g_adc2_mV, g_adc2_sum, g_adc2_count);

    powerUnit.powerT1off();

    // Decisione: vado in B?
    if (g_cycleCount % GROUP_B_MULT == 0) {
      g_currentState = STATE_READ_GROUP_B;
    } else if (g_cycleCount % GROUP_C_MULT == 0) {
      g_currentState = STATE_READ_GROUP_C;
    } else {
      // Niente B e niente C, verifichiamo se mandare LoRa o dormire
      if (g_cycleCount % LORA_TX_MULT == 0)
        g_currentState = STATE_LORA_PREPARE;
      else
        g_currentState = STATE_PREPARE_SLEEP;
    }
  } break;

  case STATE_READ_GROUP_B: {
    DEBUG_PRINTLN(
        "---------------- READ GROUP B (T2: Wind Direction) ---------------");

    powerUnit.powerT2on();

    // Lettura Direzione Vento (AS5600 I2C)
    wind.update();
    float deg = wind.getDirectionDegrees();

    // Accumulo vettoriale per media (seno e coseno)
    float rad = deg * PI / 180.0f;
    g_wind_sin_sum += sin(rad);
    g_wind_cos_sum += cos(rad);
    g_wind_count++;

    DEBUG_PRINTF("[READ B] Wind Dir: %.1f deg (Accumulated Sin/Cos sum, "
                 "Count: %d)\n",
                 deg, g_wind_count);

    powerUnit.powerT2off();

    // Decisione: vado in C?
    if (g_cycleCount % GROUP_C_MULT == 0) {
      g_currentState = STATE_READ_GROUP_C;
    } else {
      // Niente C, verifichiamo se mandare LoRa o dormire
      if (g_cycleCount % LORA_TX_MULT == 0)
        g_currentState = STATE_LORA_PREPARE;
      else
        g_currentState = STATE_PREPARE_SLEEP;
    }
  } break;

  case STATE_READ_GROUP_C: {
    DEBUG_PRINTLN(
        "---------------- READ GROUP C (T3: Sensors + INA) ---------------");

    powerUnit.powerT3on();

    // Lettura Sensori Ambiente e Potenza
    powerUnit.readINA();
    powerUnit.readBattery();
    TCA.read();
    DS.read();

    DEBUG_PRINTF("[READ C] Bat: %d mV, Solar: %d mV\n", g_battery_mV,
                 g_loadVoltage_mV);

    powerUnit.powerT3off();

    // Calcolo Medie Finali per Payload
    if (g_adc2_count > 0) {
      g_adc2_mV = (uint16_t)(g_adc2_sum / g_adc2_count);
    }
    if (g_wind_count > 0) {
      float avg_rad = atan2(g_wind_sin_sum, g_wind_cos_sum);
      float avg_deg = avg_rad * 180.0f / PI;
      if (avg_deg < 0)
        avg_deg += 360.0f;
      g_wind_dir_avg_deg = avg_deg;
    }

    // Dopo ogni C, mostriamo OLED (se attivo) e poi inviamo
    if (DEBUG_OLED) {
      g_currentState = STATE_DEBUG_OLED;
    } else {
      g_currentState = STATE_LORA_PREPARE;
    }
  } break;

  case STATE_DEBUG_OLED: {
    bool fin = true;
#if DEBUG_OLED
    fin = displayUnit.refresh();
#endif
    if (fin) {
      // Dopo l'OLED di fine Gruppo C, andiamo all'invio
      g_currentState = STATE_LORA_PREPARE;
    }
  } break;

  case STATE_LORA_PREPARE: {
    DEBUG_PRINTLN("---STATE_LORA_PREPARE----");

    DEBUG_PRINTLN("[LORA] Finalizing Payload with Average Data...");
    PayloadMgr.preparePayload();

    memcpy(appData, PayloadMgr.getBuffer(), PayloadMgr.getSize());
    appDataSize = PayloadMgr.getSize();

    // Reset degli accumulatori per il prossimo ciclo di medie
    g_adc2_sum = 0;
    g_adc2_count = 0;
    g_wind_sin_sum = 0;
    g_wind_cos_sum = 0;
    g_wind_count = 0;
    g_txCount++; // Incremento contatore invii (X)

    g_currentState = STATE_LORA_SEND;
  } break;

  case STATE_LORA_SEND:

    if (IsLoRaMacNetworkJoined) {
      DEBUG_PRINTLN("[LORA] Sending packet (Background)...");

      MlmeReq_t mlmeReq;
      mlmeReq.Type = MLME_LINK_CHECK;
      LoRaMacMlmeRequest(&mlmeReq);

      LoRaWAN.send();

    } else {
      DEBUG_PRINTLN("[LORA] Network not joined. Skip TX.");
      // Opzionale: lanciare un join se non è già in corso
      g_currentState = STATE_WAIT_FOR_JOIN;
      break;
    }

    g_currentState = STATE_PREPARE_SLEEP;
    break;

  case STATE_PREPARE_SLEEP: {
    DEBUG_PRINTLN("---PREPARE_SLEEP----");

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

    // g_cycleCount incrementato all'inizio del ciclo in STATE_IDLE
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
