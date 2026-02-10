#include "OneWireMgr.h"
#include "Config.h" // Se esiste, altrimenti rimuovere se non necessario
#include <Wire.h>

static Adafruit_DS248x driver;
OneWireManager DS;

// ============================================================================
// CONFIGURAZIONE HARDCODED
// ============================================================================
static const OneWireSlot CONST_CONFIG[ONEWIRE_SLOTS] = {
    // [0] T_3m
    {{0x28, 0x4B, 0x24, 0xBB, 0x00, 0x00, 0x00, 0x71}, 0, "T_3m", NAN, false},
    // [1] T_1m
    {{0x28, 0xFF, 0xA3, 0x6C, 0x00, 0x00, 0x00, 0xB7}, 0, "T_1m", NAN, false},
    // [2-7] Vuoti
    {{0}, 0, nullptr, NAN, false},
    {{0}, 0, nullptr, NAN, false},
    {{0}, 0, nullptr, NAN, false},
    {{0}, 0, nullptr, NAN, false},
    {{0}, 0, nullptr, NAN, false},
    {{0}, 0, nullptr, NAN, false}};

bool OneWireManager::initHardware() {
  // CORREZIONE: Adafruit_DS248x::begin(TwoWire *theWire, uint8_t address)
  // L'ordine era invertito nel tuo codice originale
  return driver.begin(&Wire1, DS2482_ADDR);
}

void OneWireManager::read() {
  loadConfig();

  if (!initHardware()) {
    DEBUG_PRINTLN(F("[DS2482] ERR: Chip not found (Check Wire1)"));
    return;
  }

  // Start Conversion (Broadcast)
  // Nota: selectChannel seleziona il canale sul MUX interno del DS2482-800
  driver.selectChannel(0);

  // Reset del BUS 1-Wire (non del chip)
  driver.OneWireReset();

  // Skip ROM (Broadcast) - 0xCC
  driver.OneWireWriteByte(0xCC);

  // Convert T command - 0x44
  driver.OneWireWriteByte(0x44);

  // Attesa conversione (750ms per 12-bit)
  // Nota: Se usassi delay(750) bloccherebbe tutto.
  // Assicurati che questo delay sia accettabile nel tuo flusso,
  // altrimenti dovresti gestire la lettura in uno stato separato.
  delay(750);

  // Lettura Scratchpad per ogni sensore
  for (int i = 0; i < ONEWIRE_SLOTS; i++) {
    if (sensors[i].label == nullptr)
      continue;

    driver.selectChannel(sensors[i].channel);

    // Reset Bus prima di selezionare il dispositivo
    driver.OneWireReset();

    // Match ROM (Select) - 0x55
    driver.OneWireWriteByte(0x55);

    // Scriviamo l'indirizzo a 64-bit (8 byte)
    for (int k = 0; k < 8; k++) {
      driver.OneWireWriteByte(sensors[i].address[k]);
    }

    // Read Scratchpad - 0xBE
    driver.OneWireWriteByte(0xBE);

    uint8_t data[9];
    for (int k = 0; k < 9; k++) {
      driver.OneWireReadByte(&data[k]);
    }

    int16_t raw = (data[1] << 8) | data[0];
    float t = (float)raw / 16.0;

    if (t > -55.0 && t < 125.0 && t != 85.0) {
      sensors[i].tempC = t;
      sensors[i].valid = true;

      // Aggiorna variabili rapide
      if (i == IDX_T_3M)
        t_3m = t;
      if (i == IDX_T_1M)
        t_1m = t;
    } else {
      sensors[i].valid = false;
    }
  }

  printResults();
}

void OneWireManager::scan() {
  if (!initHardware())
    return;

  DEBUG_PRINTLN(F("[DS2482] Scanning 1-Wire bus..."));
  driver.selectChannel(0);
  driver.OneWireReset();

  // Read ROM - 0x33 (Funziona solo se c'è UN solo dispositivo sul bus!)
  driver.OneWireWriteByte(0x33);

  uint8_t addr[8];
  for (int i = 0; i < 8; i++) {
    driver.OneWireReadByte(&addr[i]);
  }

  DEBUG_PRINT(F("Found ROM: "));
  for (int i = 0; i < 8; i++) {
    DEBUG_PRINTF(PSTR("%02X "), addr[i]);
  }
  DEBUG_PRINTLN();

  // Nota: Per una scan completa di più dispositivi servirebbe l'algoritmo di
  // Search ROM (0xF0) che è più complesso. La libreria Adafruit ha
  // 'OneWireSearch(addr)' se serve.
}

void OneWireManager::loadConfig() {
  // Copia la config hardcoded nella RAM
  for (int i = 0; i < ONEWIRE_SLOTS; i++) {
    sensors[i] = CONST_CONFIG[i];
  }
}

uint8_t OneWireManager::crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

void OneWireManager::printResults() {
  DEBUG_PRINT(F("[DS2482] Results: "));
  bool found = false;
  for (int i = 0; i < ONEWIRE_SLOTS; i++) {
    if (sensors[i].valid) {
      DEBUG_PRINTF(PSTR("%s: %.2fC | "), sensors[i].label, sensors[i].tempC);
      found = true;
    }
  }
  if (!found)
    DEBUG_PRINT(F("None valid."));
  DEBUG_PRINTLN();
}
