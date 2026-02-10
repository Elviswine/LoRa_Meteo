#include "tca_i2c_manager.h"
#include "Config.h"

// ============================================================================
// VARIABILI GLOBALI - OGGETTI SENSORI (array per ogni canale)
// ============================================================================

static Adafruit_SHT31 *sht31_ptrs[TCA_NUM_CHANNELS] = {nullptr};
static Adafruit_SHT4x *sht4x_ptrs[TCA_NUM_CHANNELS] = {nullptr};
static Adafruit_BME280 *bme280_ptrs[TCA_NUM_CHANNELS] = {nullptr};

// Indirizzi scoperti sui singoli canali
uint8_t TCA_CH_ADDR[TCA_NUM_CHANNELS] = {0};

// Dati globali SHT3X (array per ogni canale)
float sht3x_temperature[TCA_NUM_CHANNELS] = {0.0f};
float sht3x_humidity[TCA_NUM_CHANNELS] = {0.0f};
bool sht3x_online[TCA_NUM_CHANNELS] = {false};

// Dati globali SHT4X (array per ogni canale)
float sht4x_temperature[TCA_NUM_CHANNELS] = {0.0f};
float sht4x_humidity[TCA_NUM_CHANNELS] = {0.0f};
bool sht4x_online[TCA_NUM_CHANNELS] = {false};

// Dati globali BME280 (array per ogni canale)
float bme280_temperature[TCA_NUM_CHANNELS] = {0.0f};
float bme280_humidity[TCA_NUM_CHANNELS] = {0.0f};
float bme280_pressure[TCA_NUM_CHANNELS] = {0.0f};
bool bme280_online[TCA_NUM_CHANNELS] = {false};

// ============================================================================
// COSTRUTTORE
// ============================================================================

TcaI2cManager::TcaI2cManager()
    : _wire(&Wire), // di default, sarà sovrascritto da setWire()
      _tca_addr(TCA_ADDR_DEFAULT), _tca_initialized(false) {
  // Gli oggetti sensori vengono creati in initAsync() quando _wire è settato

  for (int i = 0; i < TCA_NUM_CHANNELS; i++) {
    _channel_online[i] = false;
  }
}

// ============================================================================
// IMPOSTAZIONE BUS I2C
// ============================================================================

void TcaI2cManager::setWire(TwoWire &w) { _wire = &w; }

bool TcaI2cManager::isInitialized() const { return _tca_initialized; }

// ============================================================================
// FUNZIONI BASSO LIVELLO TCA
// ============================================================================

bool TcaI2cManager::detectTcaAddress() {
  // Scansiona gli indirizzi tipici del TCA9548A: 0x70..0x77
  for (uint8_t addr = 0x70; addr <= 0x77; addr++) {
    _wire->beginTransmission(addr);
    if (_wire->endTransmission() == 0) {
      _tca_addr = addr;
      DEBUG_PRINTF("[TCA] Found TCA9548A at 0x%02X on bus %p\n", addr,
                   (void *)_wire);
      return true;
    }
  }
  return false;
}

bool TcaI2cManager::selectChannel(uint8_t ch) {
  if (ch >= TCA_NUM_CHANNELS)
    return false;
  _wire->beginTransmission(_tca_addr);
  _wire->write(1 << ch);
  if (_wire->endTransmission() != 0) {
    DEBUG_PRINTF("[TCA] ERROR: selectChannel(%u) failed on addr 0x%02X\n", ch,
                 _tca_addr);
    return false;
  }
  delay(5);
  return true;
}

bool TcaI2cManager::probe(uint8_t addr) {
  _wire->beginTransmission(addr);
  return (_wire->endTransmission() == 0);
}

// ============================================================================
// DISCOVERY SENSORE SU CANALE
// ============================================================================

uint8_t TcaI2cManager::discoverSensor(uint8_t ch, SensorType type) {
  const uint8_t *list = nullptr;
  uint8_t count = 0;

  if (type == SENS_SHT3X) {
    list = SHT3X_ADDRESSES;
    count = sizeof(SHT3X_ADDRESSES);
  } else if (type == SENS_SHT4X) {
    list = SHT4X_ADDRESSES;
    count = sizeof(SHT4X_ADDRESSES);
  } else if (type == SENS_BME280) {
    list = BME280_ADDRESSES;
    count = sizeof(BME280_ADDRESSES);
  } else {
    return 0;
  }

  if (!selectChannel(ch)) {
    return 0;
  }
  delay(100);

  for (uint8_t i = 0; i < count; i++) {
    uint8_t addr = list[i];
    if (probe(addr)) {
      Serial.printf("[TCA] CH%u: I2C device responded at 0x%02X\n", ch, addr);
      return addr;
    }
    delay(5);
  }

  return 0;
}

// ============================================================================
// INIZIALIZZAZIONE COMPLETA (AUTO-DETECT TCA + SENSORS)
// ============================================================================

void TcaI2cManager::initAsync() {
  _tca_initialized = false;

  if (_wire == nullptr) {
    DEBUG_PRINTLN("[TCA] ERROR: Wire not set!");
    return;
  }

  // Trova TCA (scan 0x70..0x77)
  if (!detectTcaAddress()) {
    DEBUG_PRINTLN(F("[TCA] ERROR: TCA not found (0x70..0x77)"));
    return;
  }

  // Reset stati canali
  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    _channel_online[ch] = false;
    TCA_CH_ADDR[ch] = 0x00;
    sht3x_online[ch] = false;
    sht4x_online[ch] = false;
    bme280_online[ch] = false;
  }

  // Discovery e init sensori
  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    if (TCA_CH_TYPE[ch] == SENS_NONE)
      continue;

    // Trova indirizzo sul canale
    uint8_t addr = discoverSensor(ch, TCA_CH_TYPE[ch]);
    if (addr == 0) {
      Serial.printf("[TCA] CH%u: No response for expected sensor type\n", ch);
      continue;
    }

    TCA_CH_ADDR[ch] = addr;
    Serial.printf("[TCA] CH%u: Found device @ 0x%02X (type=%s)\n", ch, addr,
                  (TCA_CH_TYPE[ch] == SENS_SHT3X   ? "SHT3X"
                   : TCA_CH_TYPE[ch] == SENS_SHT4X ? "SHT4X"
                                                   : "BME280"));

    // ====================================================================
    // INIT SHT3X
    // ====================================================================
    if (TCA_CH_TYPE[ch] == SENS_SHT3X) {
      if (!selectChannel(ch)) {
        Serial.printf("[TCA] CH%u: selectChannel failed before SHT3X init\n",
                      ch);
        continue;
      }
      delay(30);

      // Crea oggetto SHT31 per questo canale (se non esiste)
      if (sht31_ptrs[ch] == nullptr) {
        sht31_ptrs[ch] = new Adafruit_SHT31(_wire);
        Serial.printf("[TCA] CH%u: SHT31 object created\n", ch);
      }

      bool ok = false;
      if (sht31_ptrs[ch] != nullptr) {
        ok = sht31_ptrs[ch]->begin(addr);
        Serial.printf("[TCA] CH%u: SHT31 begin(0x%02X) = %s\n", ch, addr,
                      ok ? "OK" : "FAIL");
      }

      if (ok) {
        sht31_ptrs[ch]->heater(false);
        _channel_online[ch] = true;
        sht3x_online[ch] = true;
        Serial.printf("[TCA] CH%u: SHT3X ONLINE @ 0x%02X\n", ch, addr);
      } else {
        Serial.printf("[TCA] CH%u: SHT3X init FAILED @ 0x%02X\n", ch, addr);
      }
    }
    // ====================================================================
    // INIT SHT4X
    // ====================================================================
    else if (TCA_CH_TYPE[ch] == SENS_SHT4X) {
      if (!selectChannel(ch)) {
        Serial.printf("[TCA] CH%u: selectChannel failed before SHT4X init\n",
                      ch);
        continue;
      }
      delay(30);

      // Crea oggetto SHT4x per questo canale (se non esiste)
      if (sht4x_ptrs[ch] == nullptr) {
        sht4x_ptrs[ch] = new Adafruit_SHT4x();
        Serial.printf("[TCA] CH%u: SHT4x object created\n", ch);
      }

      bool ok = false;
      if (sht4x_ptrs[ch] != nullptr) {
        ok = sht4x_ptrs[ch]
                 ->begin(); // SHT4x non supporta TwoWire nel costruttore
        Serial.printf("[TCA] CH%u: SHT4x begin(0x%02X) = %s\n", ch, addr,
                      ok ? "OK" : "FAIL");
      }

      if (ok) {
        sht4x_ptrs[ch]->setPrecision(SHT4X_HIGH_PRECISION);
        sht4x_ptrs[ch]->setHeater(SHT4X_NO_HEATER);
        _channel_online[ch] = true;
        sht4x_online[ch] = true;
        Serial.printf("[TCA] CH%u: SHT4X ONLINE @ 0x%02X\n", ch, addr);
      } else {
        Serial.printf("[TCA] CH%u: SHT4X init FAILED @ 0x%02X\n", ch, addr);
      }
    }
    // ====================================================================
    // INIT BME280
    // ====================================================================
    else if (TCA_CH_TYPE[ch] == SENS_BME280) {
      if (!selectChannel(ch)) {
        Serial.printf("[TCA] CH%u: selectChannel failed before BME280 init\n",
                      ch);
        continue;
      }
      delay(20);

      // Crea oggetto BME280 per questo canale (se non esiste)
      if (bme280_ptrs[ch] == nullptr) {
        bme280_ptrs[ch] = new Adafruit_BME280();
        Serial.printf("[TCA] CH%u: BME280 object created\n", ch);
      }

      bool ok = false;
      if (bme280_ptrs[ch] != nullptr) {
        ok = bme280_ptrs[ch]->begin(addr, _wire);
        Serial.printf("[TCA] CH%u: BME280 begin(0x%02X) = %s\n", ch, addr,
                      ok ? "OK" : "FAIL");
      }

      if (ok) {
        bme280_ptrs[ch]->setSampling(
            Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X2,
            Adafruit_BME280::SAMPLING_X2, Adafruit_BME280::SAMPLING_X2,
            Adafruit_BME280::FILTER_OFF);
        _channel_online[ch] = true;
        bme280_online[ch] = true;
        Serial.printf("[TCA] CH%u: BME280 ONLINE @ 0x%02X\n", ch, addr);
      } else {
        Serial.printf("[TCA] CH%u: BME280 init FAILED @ 0x%02X\n", ch, addr);
      }
    }
  }

  _tca_initialized = true;
  DEBUG_PRINTLN(F("[TCA] initAsync completed."));
}

// ============================================================================
// LETTURA
// ============================================================================

void TcaI2cManager::read() {
  if (!_tca_initialized) {
    DEBUG_PRINTLN(F("[TCA] read() called but not initialized."));
    return;
  }

  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    if (!_channel_online[ch])
      continue;

    if (TCA_CH_TYPE[ch] == SENS_SHT3X) {
      readSHT3X(ch);
    } else if (TCA_CH_TYPE[ch] == SENS_SHT4X) {
      readSHT4X(ch);
    } else if (TCA_CH_TYPE[ch] == SENS_BME280) {
      readBME280(ch);
    }
  }
}

void TcaI2cManager::readSHT3X(uint8_t ch) {
  if (sht31_ptrs[ch] == nullptr)
    return;
  if (!selectChannel(ch)) {
    Serial.printf("[TCA] readSHT3X: selectChannel(%u) failed\n", ch);
    return;
  }
  delay(20);

  float t = sht31_ptrs[ch]->readTemperature();
  float h = sht31_ptrs[ch]->readHumidity();

  // Retry se NaN
  if (isnan(t) || isnan(h)) {
    Serial.printf("[TCA] CH%u SHT3X first read NaN, retrying...\n", ch);
    delay(50);
    selectChannel(ch);
    delay(10);
    t = sht31_ptrs[ch]->readTemperature();
    h = sht31_ptrs[ch]->readHumidity();
  }

  Serial.printf("[TCA] CH%u SHT3X raw: T=%.2fC H=%.2f%%\n", ch, t, h);

  // Valida risultati
  if (!isnan(t) && !isnan(h) && t > -40.0f && t < 125.0f) {
    sht3x_temperature[ch] = t;
    sht3x_humidity[ch] = h;
    sht3x_online[ch] = true;
  } else {
    sht3x_online[ch] = false;
  }
}

void TcaI2cManager::readSHT4X(uint8_t ch) {
  if (sht4x_ptrs[ch] == nullptr)
    return;
  if (!selectChannel(ch)) {
    Serial.printf("[TCA] readSHT4X: selectChannel(%u) failed\n", ch);
    return;
  }
  delay(20);

  sensors_event_t humidity, temp;
  sht4x_ptrs[ch]->getEvent(&humidity, &temp);

  float t = temp.temperature;
  float h = humidity.relative_humidity;

  // Valida risultati
  if (!isnan(t) && !isnan(h) && t > -40.0f && t < 125.0f) {
    Serial.printf("[TCA] CH%u SHT4X raw: T=%.2fC H=%.2f%%\n", ch, t, h);
    sht4x_temperature[ch] = t;
    sht4x_humidity[ch] = h;
    sht4x_online[ch] = true;
  } else {
    Serial.printf("[TCA] CH%u SHT4X read FAILED\n", ch);
    sht4x_online[ch] = false;
  }
}

void TcaI2cManager::readBME280(uint8_t ch) {
  if (bme280_ptrs[ch] == nullptr)
    return;
  if (!selectChannel(ch)) {
    Serial.printf("[TCA] readBME280: selectChannel(%u) failed\n", ch);
    return;
  }
  delay(10);

  // Misura FORZATA
  bme280_ptrs[ch]->takeForcedMeasurement();

  float t = bme280_ptrs[ch]->readTemperature();
  float p = bme280_ptrs[ch]->readPressure() / 100.0f;
  float h = bme280_ptrs[ch]->readHumidity();

  Serial.printf("[TCA] CH%u BME280 raw: T=%.2fC P=%.2fhPa H=%.2f%%\n", ch, t, p,
                h);

  if (!isnan(t) && !isnan(p) && !isnan(h)) {
    bme280_temperature[ch] = t;
    bme280_pressure[ch] = p;
    bme280_humidity[ch] = h;
    bme280_online[ch] = true;
  } else {
    bme280_online[ch] = false;
  }
}

// ============================================================================
// DEBUG
// ============================================================================

#if DEBUG_SERIAL
void TcaI2cManager::printDiscoveryResults() {
  Serial.println(F("[TCA] Discovery:"));
  Serial.printf(" TCA addr: 0x%02X (bus=%p)\n", _tca_addr, (void *)_wire);

  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    if (TCA_CH_TYPE[ch] == SENS_NONE)
      continue;

    const char *name = (TCA_CH_TYPE[ch] == SENS_SHT3X   ? "SHT3X"
                        : TCA_CH_TYPE[ch] == SENS_SHT4X ? "SHT4X"
                                                        : "BME280");
    if (_channel_online[ch]) {
      Serial.printf(" CH%u: %s @ 0x%02X ONLINE\n", ch, name, TCA_CH_ADDR[ch]);
    } else {
      Serial.printf(" CH%u: %s OFFLINE (addr 0x%02X)\n", ch, name,
                    TCA_CH_ADDR[ch]);
    }
  }
}

void TcaI2cManager::printStatus() {
  Serial.println(F("[TCA] Status:"));

  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    if (TCA_CH_TYPE[ch] == SENS_NONE)
      continue;

    if (TCA_CH_TYPE[ch] == SENS_SHT3X && sht3x_online[ch]) {
      Serial.printf(" CH%u [SHT3X] T=%.2fC H=%.1f%%\n", ch,
                    sht3x_temperature[ch], sht3x_humidity[ch]);
    } else if (TCA_CH_TYPE[ch] == SENS_SHT3X) {
      Serial.printf(" CH%u [SHT3X] OFFLINE\n", ch);
    }

    if (TCA_CH_TYPE[ch] == SENS_SHT4X && sht4x_online[ch]) {
      Serial.printf(" CH%u [SHT4X] T=%.2fC H=%.1f%%\n", ch,
                    sht4x_temperature[ch], sht4x_humidity[ch]);
    } else if (TCA_CH_TYPE[ch] == SENS_SHT4X) {
      Serial.printf(" CH%u [SHT4X] OFFLINE\n", ch);
    }

    if (TCA_CH_TYPE[ch] == SENS_BME280 && bme280_online[ch]) {
      Serial.printf(" CH%u [BME280] T=%.2fC P=%.1fhPa H=%.1f%%\n", ch,
                    bme280_temperature[ch], bme280_pressure[ch],
                    bme280_humidity[ch]);
    } else if (TCA_CH_TYPE[ch] == SENS_BME280) {
      Serial.printf(" CH%u [BME280] OFFLINE\n", ch);
    }
  }
}

void TcaI2cManager::scanChannels() {
  Serial.println(F("\n[TCA] Deep Channel Scan:"));

  for (uint8_t ch = 0; ch < TCA_NUM_CHANNELS; ch++) {
    if (!selectChannel(ch)) {
      Serial.printf(" CH%u: selectChannel FAILED\n", ch);
      continue;
    }
    delay(20);

    Serial.printf(" CH%u: ", ch);
    bool found = false;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      if (probe(addr)) {
        Serial.printf("0x%02X ", addr);
        found = true;
      }
    }

    if (!found)
      Serial.print(F("(empty)"));
    Serial.println();
  }
}
#endif // DEBUG_SERIAL
