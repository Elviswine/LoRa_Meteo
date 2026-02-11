#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// ========================================
// DEFINIZIONI STATI MACCHINA
// ========================================
enum SystemState {
  STATE_IDLE,
  STATE_WAIT_FOR_JOIN,
  STATE_READ_GROUP_A,
  STATE_READ_GROUP_B,
  STATE_READ_GROUP_C,
  STATE_DEBUG_OLED,
  STATE_LORA_PREPARE,
  STATE_PREPARE_SLEEP,
  STATE_LORA_SEND,
  STATE_SLEEP_WAIT
};

// ========================================
// VARIABILI GLOBALI (INT - tutte in mV/mA)
// ========================================

// --- Contatore Pioggia ---
extern int g_currentCount;
extern int g_lastCountValue;

// --- Misure INA219 (Pannello Solare) ---
extern int16_t g_loadVoltage_mV; // Voltaggio in mV (int16_t: -32V a +32V)
extern int16_t g_loadCurrent_mA; // Corrente in mA (int16_t: -32A a +32A)

// --- Batteria e ADC ---
extern uint16_t g_battery_mV; // Voltaggio batteria in mV
extern uint8_t g_battery_pct; // Percentuale batteria (0-100%)
extern uint16_t g_adc2_mV;    // ADC2 in mV
extern uint16_t g_adc3_mV;    // ADC3 in mV

// --- Accumulatori per Medie ---
extern float g_adc2_sum;
extern uint16_t g_adc2_count;
extern float g_wind_sin_sum;
extern float g_wind_cos_sum;
extern uint16_t g_wind_count;
extern float g_wind_dir_avg_deg; // Media vettoriale finale

// --- Timing e Controllo ---
extern volatile bool g_wakeUpFlag;
extern SystemState g_currentState;
extern uint32_t g_cycleCount;
extern uint32_t g_txCount; // Numero totale di invii LoRa (X)
extern unsigned long g_timeElapsed;

// --- Timer ---
extern TimerEvent_t g_sleepTimer;

#endif
