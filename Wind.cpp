#include "Wind.h"

// Istanza globale
Wind wind;

Wind::Wind() : _encoder(nullptr), _windDIR(N), _currentDegrees(0), _northOffset(0), _initialized(false) {}

Wind::~Wind() {
  if (_encoder) delete _encoder;
}

// --- DIAGNOSTICA DETTAGLIATA (Presa dal tuo codice originale) ---
void Wind::debugStatus(uint8_t status) {
  Serial.println("\n--- AS5600 Status Register Debug ---");
  Serial.print("Raw Status Byte: 0x"); Serial.print(status, HEX);
  Serial.print(" (Binary: "); Serial.print(status, BIN); Serial.println(")");

  // Bit 5: MD (Magnet Detected)
  if (status & 0x20) {
    Serial.println("✓ MD (bit 5): Magnet DETECTED");
  } else {
    Serial.println("✗ MD (bit 5): Magnet NOT detected - Check magnet presence!");
  }

  // Bit 4: ML (Magnet too Low/Weak)
  if (status & 0x10) {
    Serial.println("⚠ ML (bit 4): Magnet TOO WEAK - Move magnet closer!");
  } else {
    Serial.println("✓ ML (bit 4): Magnet signal OK");
  }

  // Bit 3: MH (Magnet too High/Strong)
  if (status & 0x08) {
    Serial.println("⚠ MH (bit 3): Magnet TOO STRONG - Move magnet away!");
  } else {
    Serial.println("✓ MH (bit 3): Magnet distance OK");
  }
  Serial.println("-----------------------------------\n");
}

bool Wind::init() {
  Serial.println("\n[WIND] Starting AS5600 initialization...");
  
  // Nota: Assumiamo Wire1 già avviata nel Main a 400kHz
  
  if (_encoder) delete _encoder;
  _encoder = new AS5600(&Wire1);
  
  Serial.println("[WIND] Calling encoder.begin()...");
  _encoder->begin(); 

  // Verifica connessione I2C
  if (!_encoder->isConnected()) {
    Serial.println("[WIND] ✗ FAILED: AS5600 not responding on I2C");
    Serial.println(" -> Check SDA/SCL wiring & Pullups");
    Serial.println(" -> Check I2C address (should be 0x36)");
    return false;
  }
  
  Serial.println("[WIND] ✓ AS5600 found on I2C bus");

  // Diagnostica Magnete
  uint8_t status = _encoder->readStatus();
  debugStatus(status);

  // Controllo critico: Magnete rilevato?
  if (!(status & 0x20)) {
     Serial.println("[WIND] ✗ CRITICAL: Magnet NOT detected. Init failed.");
     return false; // Blocchiamo l'init se non c'è il magnete
  }

  // Controllo distanza (Warning ma non bloccante)
  if ((status & 0x10) || (status & 0x08)) {
     Serial.println("[WIND] ⚠ WARNING: Magnet distance not optimal (see above)");
  } else {
     Serial.println("[WIND] ✓ Magnet alignment OPTIMAL");
  }

  _initialized = true;
  Serial.println("[WIND] Initialization COMPLETE\n");
  return true;
}

void Wind::update() {
  if (!_initialized) {
    Serial.println("[WIND] Skipped update: Not Initialized");
    return;
  }

  // Media vettoriale bloccante (breve durata: 10 * 10ms = 100ms)
  float meanDeg = performVectorialRead();
  
  _currentDegrees = meanDeg;
  _windDIR = degreesToCardinal(meanDeg);
}

float Wind::performVectorialRead() {
  float sumSin = 0.0;
  float sumCos = 0.0;

  for (int i = 0; i < WIND_SAMPLES; i++) {
    uint16_t raw = _encoder->rawAngle();
    float deg = rawToDegrees(raw);
    
    // Offset e normalizzazione
    deg = fmod(deg - _northOffset + 360.0, 360.0);
    
    float rad = deg * PI / 180.0;
    sumSin += sin(rad);
    sumCos += cos(rad);
    
    delay(WIND_SAMPLE_DELAY); 
  }

  float avgRad = atan2(sumSin / WIND_SAMPLES, sumCos / WIND_SAMPLES);
  float avgDeg = avgRad * 180.0 / PI;
  if (avgDeg < 0) avgDeg += 360.0;
  
  return avgDeg;
}

float Wind::rawToDegrees(uint16_t rawAngle) {
  return (rawAngle * 360.0) / 4096.0;
}

WindDirection Wind::degreesToCardinal(float degrees) {
  float normalized = fmod(degrees + 11.25, 360.0);
  uint8_t sector = (uint8_t)(normalized / 22.5);
  if (sector > 15) sector = 0;
  return (WindDirection)sector;
}

WindDirection Wind::getDirection() const { return _windDIR; }
const char* Wind::directionToString() const { return WIND_DIR_STRINGS[_windDIR]; }
float Wind::getDirectionDegrees() const { return _currentDegrees; }
void Wind::setNorth(uint16_t offset) { _northOffset = offset; }
