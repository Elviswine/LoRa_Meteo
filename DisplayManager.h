#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include "Config.h"
#include "Globals.h"
#include "HT_SH1107Wire.h"
#include "OneWireMgr.h"

// Costanti configurabili
#define OLED_PAGE_TIME_MS 5000 // Tempo per pagina (5 secondi)

class DisplayManager {
public:
    DisplayManager();
    void init();
    
    // Ritorna true quando tutte le pagine sono state visualizzate
    bool refresh(); 

private:
    SH1107Wire display;

    // Gestione pagine
    uint8_t currentPage;
    unsigned long pageStartTime;
    
    // --- FIX CONFLITTI: Timer per limitare FPS ---
    unsigned long lastDrawTime;
    // ---------------------------------------------

    static const uint8_t NUM_PAGES = 5;

    // Funzioni di disegno per ogni pagina
    void drawPageRainCounter();
    void drawPageSolarPower();
    void drawPageTCA();
    void drawPageDS2482();
    void drawPageLoRaWAN();

    
    // Helper
    void drawPageProgressBar();
};

#endif
