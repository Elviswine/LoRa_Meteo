#include "DisplayManager.h"
#include "LoRaPayloadManager.h"
#include "tca_i2c_manager.h"
#include <stdio.h>

extern volatile int16_t saved_rssi;
extern volatile int8_t saved_snr;

// ============================================
// COSTRUTTORE E INIT
// ============================================
DisplayManager::DisplayManager()
    : display(DISPLAY_ADDR, 500000, SDA, SCL, DISPLAY_GEOM, DISPLAY_RST),
      currentPage(0), pageStartTime(millis()), lastDrawTime(0) {}

void DisplayManager::init() {
  display.init();
  display.wakeup();
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.screenRotate(ANGLE_270_DEGREE);

  currentPage = 0;
  pageStartTime = millis();
  // Forziamo il primo disegno immediato settando il timer nel passato
  lastDrawTime = 0;
}

// ============================================
// REFRESH PRINCIPALE
// Ritorna: true = tutte le pagine completate
//          false = ancora sta visualizzando
// ============================================
bool DisplayManager::refresh() {
  unsigned long now = millis();

  // --- FIX CRITICO: Rilevamento automatico inizio ciclo ---
  // Se siamo a Pagina 0 E il timer 'pageStartTime' è nel passato remoto
  // (es. differenza > OLED_PAGE_TIME_MS * 2), significa che siamo appena
  // tornati dal Deep Sleep e dobbiamo resettare tutto.
  unsigned long timeDiff = now - pageStartTime;

  // Se il tempo trascorso è assurdo (es. > 10 secondi) o siamo appena partiti
  // (timeDiff enorme se contatore gira) Resettiamo l'orologio della pagina.
  if (currentPage == 0 && timeDiff > (OLED_PAGE_TIME_MS * 2)) {
    display.init();   // Riaccendi/Inizializza display
    display.wakeup(); // Per sicurezza
    display.screenRotate(ANGLE_270_DEGREE);
    display.setFont(ArialMT_Plain_10);

    pageStartTime = now; // <--- RESETTA IL TIMER ORA!
    lastDrawTime = 0;    // Forza disegno immediato
    timeDiff = 0;        // Aggiorna variabile locale
  }

  // Ricalcola timeInPage con il timer eventualmente corretto
  unsigned long timeInPage = now - pageStartTime;

  // --- LOGICA DI CAMBIO PAGINA ---
  if (timeInPage >= OLED_PAGE_TIME_MS) {

    // Prima di cambiare, disegniamo la barra al 100%
    display.drawProgressBar(1, 120, 60, 5, 100);
    display.display();

    currentPage++;
    pageStartTime = now; // Reset timer per la NUOVA pagina
    lastDrawTime = 0;

    // Se abbiamo finito l'ultima pagina (Pagina 3 -> 4)
    if (currentPage >= NUM_PAGES) {
      currentPage = 0;
      display.sleep();

      // TRUCCO: Spostiamo pageStartTime nel passato remoto
      // Così al prossimo ciclo il check (timeDiff > OLED_PAGE_TIME_MS * 2)
      // scatterà di nuovo!
      pageStartTime = now - 60000;

      return true; // ← ESCE QUI
    }
  }

  // --- THROTTLING (Evita sovraccarico I2C) ---
  if (now - lastDrawTime < 200 &&
      lastDrawTime != 0) { // Ridotto a 200ms per fluidità
    return false;
  }
  lastDrawTime = now;

  // --- DISEGNO NORMALE ---
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  switch (currentPage) {
  case 0:
    drawPageRainCounter();
    break;
  case 1:
    drawPageSolarPower();
    break;
  case 2:
    drawPageTCA();
    break;
  case 3:
    drawPageDS2482();
    break;
  case 4:
    drawPageLoRaWAN();
    break;
  default:
    currentPage = 0;
    break;
  }

  display.display();
  return false;
}

// ============================================
// FUNZIONE HELPER: Disegna barra progresso pagina
// ============================================
void DisplayManager::drawPageProgressBar() {
  unsigned long now = millis();
  unsigned long timeInPage = now - pageStartTime;

  // Calcola percentuale
  int progress = (timeInPage * 100) / OLED_PAGE_TIME_MS;
  if (progress > 100)
    progress = 100;

  // Schermo: 128x64 ruotato 270° -> Barra in basso
  display.drawProgressBar(1, 120, 60, 5, progress);
}

// ============================================
// PAGINA 1: CONTEGGIO PIOGGIA
// ============================================
void DisplayManager::drawPageRainCounter() {
  int y = 2;
  char buf[20];

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, y, "RAIN COUNT");
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  display.setFont(ArialMT_Plain_16);
  snprintf(buf, sizeof(buf), "%d", g_currentCount);
  display.drawString(2, y, buf);
  y += 18;

  display.setFont(ArialMT_Plain_10);
  snprintf(buf, sizeof(buf), "Last: %d", g_lastCountValue);
  display.drawString(2, y, buf);
  y += 11;
  snprintf(buf, sizeof(buf), "Cycle: %d", g_cycleCount);
  display.drawString(2, y, buf);

  drawPageProgressBar();
}

// ============================================
// PAGINA 2: PANNELLO SOLARE
// ============================================
void DisplayManager::drawPageSolarPower() {
  int y = 2;
  char buf[20];

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, y, "SOLAR + BT");
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  // --- BATTERIA ---
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, y, "BT:");
  snprintf(buf, sizeof(buf), "%dmV", g_battery_mV);
  display.drawString(25, y, buf);
  y += 11;
  display.drawString(2, y, "% ");
  snprintf(buf, sizeof(buf), "%d%%", g_battery_pct);
  display.drawString(25, y, buf);
  y += 11;

  display.drawLine(0, y, 64, y);
  y += 4;

  // --- PANNELLO SOLARE ---
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, y, "SOLAR:");
  y += 11;
  snprintf(buf, sizeof(buf), "V: %dmV", (int)g_loadVoltage_mV);
  display.drawString(2, y, buf);
  y += 11;
  snprintf(buf, sizeof(buf), "I: %dmV", (int)g_loadCurrent_mA);
  display.drawString(2, y, buf);

  drawPageProgressBar();
}

// ============================================
// PAGINA 3: SENSORI TCA
// ============================================
// Cerca la funzione void DisplayManager::drawPageTCA() e modificala così:

void DisplayManager::drawPageTCA() {
  int y = 5;
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, y, "TCA STATUS");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  // --- CH0 ---
  const char *type0 = "-";
  float t0 = 0;
  float h0 = 0;
  bool on0 = false;
  char buf[32];

  // Verifica quale sensore è online
  if (bme280_online[0]) {
    type0 = "BME280";
    t0 = bme280_temperature[0];
    h0 = bme280_humidity[0];
    on0 = true;
  } else if (sht4x_online[0]) {
    type0 = "SHT4x";
    t0 = sht4x_temperature[0];
    h0 = sht4x_humidity[0];
    on0 = true;
  } else if (sht3x_online[0]) {
    type0 = "SHT3x";
    t0 = sht3x_temperature[0];
    h0 = sht3x_humidity[0];
    on0 = true;
  }

  snprintf(buf, sizeof(buf), "CH0: %s", type0);
  display.drawString(0, y, buf);
  y += 12;

  if (on0) {
    // Riga standard Temperatura e Umidità
    snprintf(buf, sizeof(buf), "%.1fC %d%%", t0, (int)round(h0));
    display.drawString(0, y, buf);

    // --- NUOVA AGGIUNTA PER PRESSIONE ---
    if (bme280_online[0]) {
      y += 11; // Vai a capo
      // Formattazione: P= 931.9hPa (1 decimale)
      snprintf(buf, sizeof(buf), "P=%.1fhPa", bme280_pressure[0]);
      display.drawString(0, y, buf);
    }
    // ------------------------------------
  } else {
    display.drawString(0, y, "--.- C -- %");
  }

  y += 16;
  display.drawLine(0, y - 4, 40, y - 4);

  // --- CH1 ---
  const char *type1 = "-";
  float t1 = 0;
  float h1 = 0;
  bool on1 = false;

  if (bme280_online[1]) {
    type1 = "BME280";
    t1 = bme280_temperature[1];
    h1 = bme280_humidity[1];
    on1 = true;
  } else if (sht4x_online[1]) {
    type1 = "SHT4x";
    t1 = sht4x_temperature[1];
    h1 = sht4x_humidity[1];
    on1 = true;
  } else if (sht3x_online[1]) {
    type1 = "SHT3x";
    t1 = sht3x_temperature[1];
    h1 = sht3x_humidity[1];
    on1 = true;
  }

  snprintf(buf, sizeof(buf), "CH1: %s", type1);
  display.drawString(0, y, buf);
  y += 12;

  if (on1) {
    snprintf(buf, sizeof(buf), "%.1fC %d%%", t1, (int)round(h1));
    display.drawString(0, y, buf);

    // --- NUOVA AGGIUNTA PER PRESSIONE SU CH1 ---
    if (bme280_online[1]) {
      y += 11;
      snprintf(buf, sizeof(buf), "P=%.1fhPa", bme280_pressure[1]);
      display.drawString(0, y, buf);
    }
    // -------------------------------------------
  } else {
    display.drawString(0, y, "--.- C -- %");
  }

  drawPageProgressBar();
}

// ============================================
// PAGINA 4: SENSORI 1-WIRE
// ============================================
void DisplayManager::drawPageDS2482() {
  // 1. Usa un buffer statico per evitare allocazioni dinamiche (String)
  char buffer[16];
  int y = 2;

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, y, "ONE WIRE");

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 5;

  // Recupera valori (Copia locale per sicurezza)
  float val1 = DS.sensors[IDX_T_3M].tempC;
  bool ok1 = DS.sensors[IDX_T_3M].valid;
  float val2 = DS.sensors[IDX_T_1M].tempC;
  bool ok2 = DS.sensors[IDX_T_1M].valid;

  // --- AIR ---
  display.drawString(0, y, "AIR:");
  display.setTextAlignment(TEXT_ALIGN_RIGHT);

  if (ok1 && !isnan(val1)) {
    // Formatta in buffer statico: %.1f = 1 decimale
    snprintf(buffer, sizeof(buffer), "%.1fC", val1);
    display.drawString(64, y,
                       buffer); // Usa 64 come margine destro (schermo ruotato)
  } else {
    display.drawString(64, y, "--.-");
  }

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  y += 15;

  // --- SOIL ---
  display.drawString(0, y, "SOIL:");
  display.setTextAlignment(TEXT_ALIGN_RIGHT);

  if (ok2 && !isnan(val2)) {
    snprintf(buffer, sizeof(buffer), "%.1fC", val2);
    display.drawString(64, y, buffer);
  } else {
    display.drawString(64, y, "--.-");
  }

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  y += 16;

  // --- INFO STATUS ---
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  if (ok1 && ok2)
    display.drawString(32, y, "[OK]");
  else if (!ok1 && !ok2)
    display.drawString(32, y, "[NO SENS]");
  else
    display.drawString(32, y, "[PARTIAL]");

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // 2. Controllo di sicurezza sulla Progress Bar
  // A volte disegnare al pixel 100% causa overflow nei driver grafici
  // Limitiamo il disegno se siamo proprio alla fine
  unsigned long now = millis();
  unsigned long timeInPage = now - pageStartTime;
  if (timeInPage <
      OLED_PAGE_TIME_MS - 100) { // Smetti di disegnarla 100ms prima della fine
    drawPageProgressBar();
  }
}

// ============================================
// PAGINA 5: STATO LORAWAN - DOWNLINK METADATA
// ============================================
void DisplayManager::drawPageLoRaWAN() {
  int y = 0;
  char buf[32];

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, y, "LORA STATUS");
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  y += 12;
  display.drawLine(0, y, 64, y);
  y += 3;

  // 1. Stato Rete (JOIN)
  display.drawString(0, y, PayloadMgr.isNetworkJoined() ? "JOINED" : "NO NET");
  y += 11;

  // 2. DOWNLINK METADATA - RSSI
  if (saved_status != 0xFF) { // 0xFF = nessun downlink ricevuto
    snprintf(buf, sizeof(buf), "RSSI:%ddBm", saved_rssi);
    display.drawString(0, y, buf);
    y += 11;

    // 3. SNR
    snprintf(buf, sizeof(buf), "SNR:%ddB", saved_snr);
    display.drawString(0, y, buf);
    y += 11;

    // 4. Datarate (DR)
    snprintf(buf, sizeof(buf), "DR:%d", saved_datarate);
    display.drawString(0, y, buf);
    y += 11;

    // 5. Status del downlink
    if (saved_status == 0) {
      display.drawString(0, y, "St:OK");
    } else {
      snprintf(buf, sizeof(buf), "St:ERR:%d", saved_status);
      display.drawString(0, y, buf);
    }
  } else {
    // Nessun downlink ricevuto ancora
    display.drawString(0, y, "No DL yet");
    y += 11;
    display.drawString(0, y, "Waiting...");
  }

  y += 13;
  display.drawLine(0, y, 64, y);
  y += 3;

  // 6. Contatori Uplink/Downlink
  snprintf(buf, sizeof(buf), "UP:%d", PayloadMgr.getUplinkCounter());
  display.drawString(0, y, buf);
  y += 11;
  snprintf(buf, sizeof(buf), "DN:%d", PayloadMgr.getDownlinkCounter());
  display.drawString(0, y, buf);

  drawPageProgressBar();
}
