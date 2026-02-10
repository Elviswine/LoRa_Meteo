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

// --- Batteria ---
uint16_t g_battery_mV = 0; // In mV
uint8_t g_battery_pct = 0; // Percentuale 0-100%

// --- Timing e Controllo ---
volatile bool g_wakeUpFlag = false;
SystemState g_currentState = STATE_IDLE;
uint32_t g_cycleCount = 0;
unsigned long g_timeElapsed = 0;

// --- Timer ---
TimerEvent_t g_sleepTimer;

// --- LoRaWAN Downlink Metadata ---
volatile int16_t saved_rssi = 0;
volatile int8_t saved_snr = 0;
volatile uint8_t saved_datarate = 0;
volatile uint8_t saved_status = 0xFF; // 0xFF = nessun downlink ricevuto ancora
