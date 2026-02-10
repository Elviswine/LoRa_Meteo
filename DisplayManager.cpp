#include "DisplayManager.h"
#include "tca_i2c_manager.h"
#include "LoRaPayloadManager.h" 

extern volatile int16_t saved_rssi;
extern volatile int8_t saved_snr;

// ============================================
// COSTRUTTORE E INIT
// ============================================
DisplayManager::DisplayManager()
    : display(DISPLAY_ADDR, 500000, SDA, SCL, DISPLAY_GEOM, DISPLAY_RST),
      currentPage(0),
      pageStartTime(millis()),
      lastDrawTime(0) {} 

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
    
    // Se il tempo trascorso è assurdo (es. > 10 secondi) o siamo appena partiti (timeDiff enorme se contatore gira)
    // Resettiamo l'orologio della pagina.
    if (currentPage == 0 && timeDiff > (OLED_PAGE_TIME_MS * 2)) {
        display.init();               // Riaccendi/Inizializza display
        display.wakeup();             // Per sicurezza
        display.screenRotate(ANGLE_270_DEGREE);
        display.setFont(ArialMT_Plain_10);
        
        pageStartTime = now;          // <--- RESETTA IL TIMER ORA!
        lastDrawTime = 0;             // Forza disegno immediato
        timeDiff = 0;                 // Aggiorna variabile locale
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
            // Così al prossimo ciclo il check (timeDiff > OLED_PAGE_TIME_MS * 2) scatterà di nuovo!
            pageStartTime = now - 60000; 
            
            return true; // ← ESCE QUI
        }
    }
    
    // --- THROTTLING (Evita sovraccarico I2C) ---
    if (now - lastDrawTime < 200 && lastDrawTime != 0) { // Ridotto a 200ms per fluidità
        return false;
    }
    lastDrawTime = now;

    // --- DISEGNO NORMALE ---
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    
    switch(currentPage) {
        case 0: drawPageRainCounter(); break;
        case 1: drawPageSolarPower(); break;
        case 2: drawPageTCA(); break;
        case 3: drawPageDS2482(); break;
        case 4: drawPageLoRaWAN(); break;
        default: currentPage = 0; break;
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
    if (progress > 100) progress = 100;

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
    
    display.setFont(ArialMT_Plain_16);
    display.drawString(2, y, String(g_currentCount));
    y += 18;
    
    display.setFont(ArialMT_Plain_10);
    display.drawString(2, y, "Last: " + String(g_lastCountValue));
    y += 11;
    display.drawString(2, y, "Cycle: " + String(g_cycleCount));

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
    display.setFont(ArialMT_Plain_10);
    display.drawString(2, y, "BT:");
    display.drawString(25, y, String(g_battery_mV) + "mV");
    y += 11;
    display.drawString(2, y, "% ");
    display.drawString(25, y, String(g_battery_pct) + "%");
    y += 11;
    
    display.drawLine(0, y, 64, y);
    y += 4;

    // --- PANNELLO SOLARE ---
    display.setFont(ArialMT_Plain_10);
    display.drawString(2, y, "SOLAR:");
    y += 11;
    display.drawString(2, y, "V: " + String(g_loadVoltage_mV) + "mV");
    y += 11;
    display.drawString(2, y, "I: " + String(g_loadCurrent_mA) + "mA");

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
    String type0 = "-";
    float t0 = 0; float h0 = 0;
    bool on0 = false;
    
    // Verifica quale sensore è online
    if (bme280_online[0]) { type0 = "BME280"; t0 = bme280_temperature[0]; h0 = bme280_humidity[0]; on0 = true; }
    else if (sht4x_online[0]) { type0 = "SHT4x"; t0 = sht4x_temperature[0]; h0 = sht4x_humidity[0]; on0 = true; }
    else if (sht3x_online[0]) { type0 = "SHT3x"; t0 = sht3x_temperature[0]; h0 = sht3x_humidity[0]; on0 = true; }

    display.drawString(0, y, "CH0: " + type0);
    y += 12;

    if (on0) {
        // Riga standard Temperatura e Umidità
        display.drawString(0, y, String(t0, 1) + "C " + String((int)round(h0)) + "%");
        
        // --- NUOVA AGGIUNTA PER PRESSIONE ---
        if (bme280_online[0]) {
            y += 11; // Vai a capo
            // Formattazione: P= 931.9hPa (1 decimale)
            display.drawString(0, y, "P=" + String(bme280_pressure[0], 1) + "hPa");
        }
        // ------------------------------------
    } 
    else {
        display.drawString(0, y, "--.- C -- %");
    }

    y += 16;
    display.drawLine(0, y-4, 40, y-4);

    // --- CH1 ---
    String type1 = "-";
    float t1 = 0; float h1 = 0;
    bool on1 = false;

    if (bme280_online[1]) { type1 = "BME280"; t1 = bme280_temperature[1]; h1 = bme280_humidity[1]; on1 = true; }
    else if (sht4x_online[1]) { type1 = "SHT4x"; t1 = sht4x_temperature[1]; h1 = sht4x_humidity[1]; on1 = true; }
    else if (sht3x_online[1]) { type1 = "SHT3x"; t1 = sht3x_temperature[1]; h1 = sht3x_humidity[1]; on1 = true; }

    display.drawString(0, y, "CH1: " + type1);
    y += 12;

    if (on1) {
        display.drawString(0, y, String(t1, 1) + "C " + String((int)round(h1)) + "%");
        
        // --- NUOVA AGGIUNTA PER PRESSIONE SU CH1 ---
        if (bme280_online[1]) {
            y += 11;
            display.drawString(0, y, "P=" + String(bme280_pressure[1], 1) + "hPa");
        }
        // -------------------------------------------
    } 
    else {
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
    bool ok1   = DS.sensors[IDX_T_3M].valid;
    float val2 = DS.sensors[IDX_T_1M].tempC;
    bool ok2   = DS.sensors[IDX_T_1M].valid;

    // --- AIR ---
    display.drawString(0, y, "AIR:");
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    
    if (ok1 && !isnan(val1)) {
        // Formatta in buffer statico: %.1f = 1 decimale
        snprintf(buffer, sizeof(buffer), "%.1fC", val1);
        display.drawString(64, y, buffer); // Usa 64 come margine destro (schermo ruotato)
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
    
    if(ok1 && ok2)       display.drawString(32, y, "[OK]");
    else if(!ok1 && !ok2) display.drawString(32, y, "[NO SENS]");
    else                 display.drawString(32, y, "[PARTIAL]");
    
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // 2. Controllo di sicurezza sulla Progress Bar
    // A volte disegnare al pixel 100% causa overflow nei driver grafici
    // Limitiamo il disegno se siamo proprio alla fine
    unsigned long now = millis();
    unsigned long timeInPage = now - pageStartTime;
    if (timeInPage < OLED_PAGE_TIME_MS - 100) { // Smetti di disegnarla 100ms prima della fine
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
  String status = PayloadMgr.isNetworkJoined() ? "JOINED" : "NO NET";
  display.drawString(0, y, status);
  y += 12;

  // 2. Recupera dati DAL MANAGER (Metodo pulito)
  int16_t rssi = PayloadMgr.getLastRSSI();
  int8_t snr = PayloadMgr.getLastSNR();

  // Se sono a 0, non abbiamo ricevuto nulla
  display.drawString(0, y, "RSSI: " + String(saved_rssi) + " (" + String(saved_snr) + ")");
  y += 12;

  // DEBUG: Mostra se la rete pensa di essere connessa
  String net = PayloadMgr.isNetworkJoined() ? "YES" : "NO";
  display.drawString(0, y, "Net: " + net);
  

  y += 14;
  
  // 3. Info Tecniche
  uint8_t sf = PayloadMgr.getConfiguredSF();
  int8_t pwr = PayloadMgr.getLastTxPower();
  // Scriviamo SF7 @14dBm (compatto)
  display.drawString(0, y, "S-" + String(sf) + " @" + String(pwr) + "dBm");
  y += 12;

  display.drawLine(0, y, 40, y);
  y += 3;
  
  // 4. Contatori
  display.drawString(0, y, "U:" + String(PayloadMgr.getUplinkCounter()) + 
                            " D:" + String(PayloadMgr.getDownlinkCounter()));

  drawPageProgressBar();
}



