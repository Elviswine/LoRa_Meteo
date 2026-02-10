#include "PowerManager.h"

void PowerMes::initINA() {
    // Init INA219 (I2C)
    Wire1.begin(SENSORS_SDA, SENSORS_SCL); 
    writeReg16(ADDR_INA219, INA219_REG_CONFIG, 0x399F);
    writeReg16(ADDR_INA219, INA219_REG_CALIB, INA219_CAL_VALUE);
}

void PowerMes::readINA() {
    // 1. Ricalibra
    writeReg16(ADDR_INA219, INA219_REG_CALIB, INA219_CAL_VALUE);
    delay(2);

    // 2. Aggiorna Variabili Globali 
    
    g_loadVoltage_mV = (uint16_t)readINA_mV();    // ← Cast a uint16_t
    g_loadCurrent_mA = (uint16_t)readINA_mA();    // ← Cast a uint16_t

}

void PowerMes::readBattery() {
    // 1. Leggi raw
    g_battery_mV = readBatteryRaw(); 
    g_battery_pct = getBatteryPercent(g_battery_mV);


}

    

void PowerMes::powerOUToff() {

    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, HIGH);  // ACCENSIONE PERIFERICHE
    digitalWrite(PIN_ALIM_t3, LOW); //spengo fotoacc.
    digitalWrite(PIN_ALIM_t1, LOW);
    digitalWrite(PIN_ALIM_t2, LOW);
}

void PowerMes::powerOUTon() {
    //pin di potenza
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);  // ACCENSIONE PERIFERICHE e sorgente fotoaccoppiatori
    delay(20);

    pinMode(PIN_ALIM_t3, OUTPUT);
    digitalWrite(PIN_ALIM_t3, HIGH);
    pinMode(PIN_ALIM_t1, OUTPUT);
    digitalWrite(PIN_ALIM_t1, HIGH);

    // Correzione confronto e sintassi Serial
    if (DEBUG_OLED != true) { 
        pinMode(PIN_ALIM_t2, OUTPUT); //pin GPIO10 in comune con schermo, gestione particolare. 
        digitalWrite(PIN_ALIM_t2, HIGH);
    } else {  Serial.println("[DBG OLED: ON");  }
  
    Wire1.begin(SENSORS_SDA, SENSORS_SCL); //init. I2c sensors.

}



// --- Private Helpers ---

void PowerMes::setMuxChannel(byte channel) {
    digitalWrite(MUX_S0, (channel & 0x01) ? HIGH : LOW);
    digitalWrite(MUX_S1, (channel & 0x02) ? HIGH : LOW);
    digitalWrite(MUX_S2, (channel & 0x04) ? HIGH : LOW);
    digitalWrite(MUX_S3, (channel & 0x08) ? HIGH : LOW);
    delay(5);
}

uint16_t PowerMes::readBatteryRaw() {
    // 1. Isola MUX esterno per non interferire
    setMuxChannel(MUX_EMPTY_CH);
    
    // 2. Accendi il partitore ADC (LOW = MOSFET ON per P-Channel)
    pinMode(VBAT_ADC_CTL, OUTPUT);
    digitalWrite(VBAT_ADC_CTL, LOW); 
    
    // 3. Attesa stabilizzazione filtro RC
    delay(10);  
    
    // 4. Lettura
    uint16_t reading = analogRead(ADC);
    
    // 5. Spegni il partitore (INPUT = Pull-up interno/esterno = OFF)
    pinMode(VBAT_ADC_CTL, INPUT); 
    
    // 6. Calcolo
    // Verifica che VOLTAGE_CALIB_FACTOR includa il x2 del partitore
    // Solitamente raw * 2 * (vRef/Resolution)
    return (uint16_t)((reading) * VOLTAGE_CALIB_FACTOR);
}


uint8_t PowerMes::getBatteryPercent(uint16_t voltage_mv) {
    // Definizione dei punti chiave della curva di scarica Li-Ion
    // Adattata per cutoff a 3400mV (Dropout LDO)
    const uint16_t voltageMap[] = {4200, 4060, 3980, 3830, 3740, 3650, 3550, 3450};
    const uint8_t percentMap[]  = { 100,   90,   80,   60,   40,   20,   10,    0};
    
    // 1. Limiti di sicurezza (Saturazione)
    if (voltage_mv >= voltageMap[0]) return 100;
    if (voltage_mv <= voltageMap[7]) return 0;

    // 2. Ricerca del segmento (Interpolazione Lineare)
    // Cerca tra quale intervallo cade il voltaggio letto
    for (int i = 0; i < 7; i++) {
        if (voltage_mv >= voltageMap[i+1]) {
            // Trovato l'intervallo [High, Low] -> [voltageMap[i], voltageMap[i+1]]
            uint16_t vHigh = voltageMap[i];
            uint16_t vLow = voltageMap[i+1];
            uint8_t pHigh = percentMap[i];
            uint8_t pLow = percentMap[i+1];
            
            // Formula Interpolazione: % = pLow + (v - vLow) * (pHigh - pLow) / (vHigh - vLow)
            // Moltiplicazione prima della divisione per mantenere precisione con interi
            return pLow + ( (uint32_t)(voltage_mv - vLow) * (pHigh - pLow) ) / (vHigh - vLow);
        }
    }
    return -1; // Fallback (non dovrebbe mai arrivarci)
}



void PowerMes::writeReg16(byte addr, byte reg, uint16_t val) {
    Wire1.beginTransmission(addr);
    Wire1.write(reg);
    Wire1.write((val >> 8) & 0xFF);
    Wire1.write(val & 0xFF);
    Wire1.endTransmission();
}

float PowerMes::readINA_mV() {
    Wire1.beginTransmission(ADDR_INA219);
    Wire1.write(INA219_REG_VOLT);
    if (Wire1.endTransmission() != 0) return 0.0;
    
    Wire1.requestFrom((int)ADDR_INA219, 2);
    if (Wire1.available() >= 2) {
        int16_t val = (Wire1.read() << 8) | Wire1.read();
        return (val >> 3) * 4.0;
    }
    return 0.0;
}

float PowerMes::readINA_mA() {
    Wire1.beginTransmission(ADDR_INA219);
    Wire1.write(INA219_REG_CURR);
    if (Wire1.endTransmission() != 0) return 0.0;
    
    Wire1.requestFrom((int)ADDR_INA219, 2);
    if (Wire1.available() >= 2) {
        int16_t raw = (Wire1.read() << 8) | Wire1.read();
        return raw * 0.1; 
    }
    return 0.0;
}
