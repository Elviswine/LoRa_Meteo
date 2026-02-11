#include "Globals.h"

// ========================================
// INIZIALIZZAZIONE VARIABILI GLOBALI
// ========================================

// --- Contatore Pioggia ---
int g_currentCount = 0;
int g_lastCountValue = 0;

// --- Misure INA219 (Pannello Solare) in mV/mA ---
int16_t g_loadVoltage_mV = 0; // Convertito da V a mV
int16_t g_loadCurrent_mA = 0; // Già in mA

// --- Batteria e ADC ---
uint16_t g_battery_mV = 0; // In mV
uint8_t g_battery_pct = 0; // Percentuale 0-100%
uint16_t g_adc2_mV = 0;    // ADC2 in mV
uint16_t g_adc3_mV = 0;    // ADC3 in mV

// --- Accumulatori per Medie ---
float g_adc2_sum = 0.0f;
uint16_t g_adc2_count = 0;
float g_wind_sin_sum = 0.0f;
float g_wind_cos_sum = 0.0f;
uint16_t g_wind_count = 0;
float g_wind_dir_avg_deg = 0.0f;

// --- Timing e Controllo ---
volatile bool g_wakeUpFlag = false;
SystemState g_currentState = STATE_IDLE;
uint32_t g_cycleCount = 0;
uint32_t g_txCount = 0;
unsigned long g_timeElapsed = 0;

// --- Timer ---
TimerEvent_t g_sleepTimer;
