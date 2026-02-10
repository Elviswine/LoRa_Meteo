#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"

// --- I2C BUS ---
#define SENSORS_SDA GPIO9
#define SENSORS_SCL GPIO8

// --- INDIRIZZI I2C ---
#define ADDR_COUNTER 0x20 // PCF8574
#define ADDR_INA219 0x40
#define DISPLAY_ADDR 0x3c

// --- PIN CONTATORE (CD4040) ---
#define PIN_CD4040_RST GPIO6

// --- PIN MUX & POWER ---
// --- PIN MUX & POWER ---
#define MUX_S0 GPIO14
#define MUX_S1 GPIO13
#define MUX_S2 GPIO12
#define MUX_S3 GPIO11
#define MUX_EMPTY_CH 15

// --- POWER GROUPS ---
#define PIN_POWER_GRP_A GPIO10 // Wind Speed (Group A) - Shared with OLED RST
#define PIN_POWER_GRP_B GPIO4  // Wind Direction (Group B)
#define PIN_POWER_GRP_C GPIO7  // Environment Sensors (Group C) - TCA, etc.

//-----------
#define ADC_SOIL ADC3
#define ADC_WIND_S ADC2

// --- DISPLAY SETTINGS ---
// --- DISPLAY SETTINGS ---
#define DISPLAY_RST GPIO10
#define DISPLAY_GEOM GEOMETRY_128_64
// Se false, il codice del display NON VIENE COMPILATO (Risparmio Flash)
#define DEBUG_OLED true

// --- DEBUG SERIALE ---
// Se 0, tutte le Serial.print vengono rimosse dal compilatore
#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#endif

// --- TIMING ---
#define MEASURE_INTERVAL_MS 30000 // 30 Secondi

// --- CALIBRAZIONE BATTERIA ---
#define VOLTAGE_CALIB_FACTOR 1.882f // Partitore 1:2
#define SYS_LEAKAGE_MA 0.05f
#define INTERNAL_RESISTANCE 0.20f
#define RELAXATION_TIME_MS 2 * 1000 // 2 secondi

// --- INA219 CONSTANTS ---
#define INA219_REG_CONFIG 0x00
#define INA219_REG_CALIB 0x05
#define INA219_REG_VOLT 0x02
#define INA219_REG_CURR 0x04
#define INA219_CAL_VALUE 4096

#define TIME_UNIT_MS 10 * 1000 // t = 2000ms (Durata del Deep Sleep base)

// Moltiplicatori di ciclo (Ogni quanti 't' eseguire l'azione)
// Base Time Unit (t) = 30 seconds (defined by MEASURE_INTERVAL_MS in logic, or
// Timer)
#define CYCLES_PER_GROUP_B 10 // 30s * 10 = 5 min
#define CYCLES_PER_GROUP_C 30 // 30s * 30 = 15 min

#define RESET_INTERVAL_MULT 15 // 15 * 2s = 30 secondi per il reset HW

#endif
