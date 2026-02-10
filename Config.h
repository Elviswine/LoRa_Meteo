#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"

// --- I2C BUS ---
#define SENSORS_SDA   GPIO9
#define SENSORS_SCL   GPIO8

// --- INDIRIZZI I2C ---
#define ADDR_COUNTER  0x20 // PCF8574
#define ADDR_INA219   0x40
#define DISPLAY_ADDR  0x3c

// --- PIN CONTATORE (CD4040) ---
#define PIN_CD4040_RST GPIO6 

// --- PIN MUX & POWER ---
#define MUX_S0        GPIO14  
#define MUX_S1        GPIO13  
#define MUX_S2        GPIO12  
#define MUX_S3        GPIO11  
#define MUX_EMPTY_CH  15
#define PIN_ALIM_t3   GPIO7
#define PIN_ALIM_t1   GPIO4  
#define PIN_ALIM_t2   GPIO10
//----------- 
#define ADC_SOIL      ADC3   
#define ADC_WIND_S    ADC2   


// --- DISPLAY SETTINGS ---
#define DISPLAY_RST   GPIO10
#define DISPLAY_GEOM  GEOMETRY_128_64
#define DEBUG_OLED true

// --- TIMING ---
#define MEASURE_INTERVAL_MS 30000 // 30 Secondi

// --- CALIBRAZIONE BATTERIA ---
#define VOLTAGE_CALIB_FACTOR 1.882f  // Partitore 1:2
#define SYS_LEAKAGE_MA       0.05f
#define INTERNAL_RESISTANCE  0.20f
#define RELAXATION_TIME_MS   2*1000    // 2 secondi

// --- INA219 CONSTANTS ---
#define INA219_REG_CONFIG 0x00
#define INA219_REG_CALIB  0x05
#define INA219_REG_VOLT   0x02
#define INA219_REG_CURR   0x04
#define INA219_CAL_VALUE  4096

#define TIME_UNIT_MS 10*1000      // t = 2000ms (Durata del Deep Sleep base)

// Moltiplicatori di ciclo (Ogni quanti 't' eseguire l'azione)
#define GROUP_A_MULT 1         // Gruppo A: Ogni ciclo (t)
#define GROUP_B_MULT 2         // Gruppo B: Ogni 3 cicli (3t)
#define GROUP_C_MULT 3        // Gruppo C: Ogni 10 cicli (10t)
#define LORA_TX_MULT 30        // Invio LoRa: Ogni 30 cicli

#define RESET_INTERVAL_MULT 15   // 15 * 2s = 30 secondi per il reset HW


#endif
