#ifndef WIND_H
#define WIND_H

#include "AS5600.h"
#include <Arduino.h>
#include <Wire.h>

// --- CONFIGURAZIONE ---
#define WIND_SAMPLES 10      // Numero campioni per media vettoriale
#define WIND_SAMPLE_DELAY 10 // ms tra un campione e l'altro

enum WindDirection {
  N = 0,
  NNE,
  NE,
  ENE,
  E,
  ESE,
  SE,
  SSE,
  S,
  SSW,
  SW,
  WSW,
  W,
  WNW,
  NW,
  NNW
};

class Wind {
private:
  AS5600 *_encoder;
  WindDirection _windDIR;
  float _currentDegrees;
  uint16_t _northOffset;
  bool _initialized;

  // --- Metodi Privati ---
  float rawToDegrees(uint16_t rawAngle);
  WindDirection degreesToCardinal(float degrees);
  float performVectorialRead();
  void debugStatus(uint8_t status); // Funzione di debug dettagliato

public:
  Wind();
  ~Wind();

  // Inizializza sensore con diagnostica completa su Serial
  bool init();

  // Legge N campioni, fa media vettoriale, aggiorna variabili
  void update();

  // Legge intensità vento (ADC) - Group A
  float getSpeed(); // Returns milliVolts or Raw Value

  // Getters
  WindDirection getDirection() const;
  const char *directionToString() const;
  float getDirectionDegrees() const;
  void setNorth(uint16_t offset);
};

static const char *WIND_DIR_STRINGS[] = {"N",  "NNE", "NE", "ENE", "E",  "ESE",
                                         "SE", "SSE", "S",  "SSW", "SW", "WSW",
                                         "W",  "WNW", "NW", "NNW"};

extern Wind wind;

#endif
