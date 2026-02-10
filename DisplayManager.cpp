// (No Serial calls found in this file to replace directly, ensuring pattern
// match for future)
#include "DisplayManager.h"
#include "LoRaPayloadManager.h"
#include "tca_i2c_manager.h"

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

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, y, "RAIN COUNT");
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  display.setFont(ArialMT_Plain_10); // Risparmio flash: uso solo un font

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", g_currentCount);
  display.drawString(2, y, buffer);
  y += 18;

  display.setFont(ArialMT_Plain_10);
  snprintf(buffer, sizeof(buffer), "Last: %d", g_lastCountValue);
  display.drawString(2, y, buffer);
  y += 11;
  snprintf(buffer, sizeof(buffer), "Cycle: %d", g_cycleCount);
  display.drawString(2, y, buffer);

  drawPageProgressBar();
}

// ============================================
// PAGINA 2: PANNELLO SOLARE
// ============================================
void DisplayManager::drawPageSolarPower() {
  int y = 2;

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, y, "SOLAR + BT");
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  // --- BATTERIA ---
  char buffer[32];
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, y, "BT:");
  snprintf(buffer, sizeof(buffer), "%dmV", g_battery_mV);
  display.drawString(25, y, buffer);
  y += 11;
  display.drawString(2, y, "% ");
  snprintf(buffer, sizeof(buffer), "%d%%", g_battery_pct);
  display.drawString(25, y, buffer);
  y += 11;

  display.drawLine(0, y, 64, y);
  y += 4;

  // --- PANNELLO SOLARE ---
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, y, "SOLAR:");
  y += 11;
  snprintf(buffer, sizeof(buffer), "V: %dmV", g_loadVoltage_mV);
  display.drawString(2, y, buffer);
  y += 11;
  snprintf(buffer, sizeof(buffer), "I: %dmA", g_loadCurrent_mA);
  display.drawString(2, y, buffer);

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

  char buffer[64];
  for (int ch = 0; ch < 2; ch++) { // Mostra solo CH0 e CH1 per spazio
    snprintf(buffer, sizeof(buffer), "CH%d: ", ch);
    // label base "CHx: "

    if (tca_sensors[ch].online) {
      // Determina tipo stringa
      const char *typeName = "-";
      SensorType t = tca_sensors[ch].type;
      if (t == SENS_SHT3X)
        typeName = "SHT3x";
      else if (t == SENS_SHT4X)
        typeName = "SHT4x";
      else if (t == SENS_BME280)
        typeName = "BME280";

      // "CHx: SHT3x"
      char lineBuffer[32];
      snprintf(lineBuffer, sizeof(lineBuffer), "%s%s", buffer, typeName);
      display.drawString(0, y, lineBuffer);
      y += 12;

      float temp = tca_sensors[ch].temperature;
      float hum = tca_sensors[ch].humidity;

      // "25.1C 50%"
      snprintf(lineBuffer, sizeof(lineBuffer), "%.1fC %d%%", temp,
               (int)round(hum));
      display.drawString(0, y, lineBuffer);

      // Pressione se presente
      if (!isnan(tca_sensors[ch].pressure)) {
        y += 11;
        snprintf(lineBuffer, sizeof(lineBuffer), "P=%.1fhPa",
                 tca_sensors[ch].pressure);
        display.drawString(0, y, lineBuffer);
      }
    } else {
      char lineBuffer[32];
      snprintf(lineBuffer, sizeof(lineBuffer), "%sOFFLINE", buffer);
      display.drawString(0, y, lineBuffer);
      y += 12;
      display.drawString(0, y, "--.- C -- %");
    }

    y += 16;
    if (ch == 0)
      display.drawLine(0, y - 4, 40, y - 4); // Separatore
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
// PAGINA 4: STATO LORAWAN (RSSI / SNR)
// ============================================
void DisplayManager::drawPageLoRaWAN() {
  int y = 2;

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(32, y, "LORA LINK");

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  y += 14;
  display.drawLine(0, y, 64, y);
  y += 4;

  // 1. Stato Rete
  const char *status = PayloadMgr.isNetworkJoined() ? "JOINED" : "NO NET";
  display.drawString(0, y, status);
  y += 12;

  // 2. Recupera dati DAL MANAGER (Metodo pulito)
  // int16_t rssi = PayloadMgr.getLastRSSI(); // Unused variable warning fix
  // int8_t snr = PayloadMgr.getLastSNR();    // Unused variable warning fix

  // Se sono a 0, non abbiamo ricevuto nulla
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "RSSI: %d (%d)", saved_rssi, saved_snr);
  display.drawString(0, y, buffer);
  y += 12;

  // DEBUG: Mostra se la rete pensa di essere connessa
  const char *net = PayloadMgr.isNetworkJoined() ? "YES" : "NO";
  snprintf(buffer, sizeof(buffer), "Net: %s", net);
  display.drawString(0, y, buffer);

  y += 14;

  // 3. Info Tecniche
  uint8_t sf = PayloadMgr.getConfiguredSF();
  int8_t pwr = PayloadMgr.getLastTxPower();
  // Scriviamo SF7 @14dBm (compatto)
  snprintf(buffer, sizeof(buffer), "S-%d @%ddBm", sf, pwr);
  display.drawString(0, y, buffer);
  y += 12;

  display.drawLine(0, y, 40, y);
  y += 3;

  // 4. Contatori
  snprintf(buffer, sizeof(buffer), "U:%d D:%d", PayloadMgr.getUplinkCounter(),
           PayloadMgr.getDownlinkCounter());
  display.drawString(0, y, buffer);

  drawPageProgressBar();
}
