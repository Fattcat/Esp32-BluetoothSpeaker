/**
 * ESP32 PIR + Audio Alert System
 * Hardware: ESP32 + PIR HC-SR501 + MAX98357A (I2S) + SD Card (SPI)
 * Library: ESP32-audioI2S by schreibfaul1 (v3.0.x+)
 * 
 * Funkcia:
 * - Detekcia pohybu → prehrá alert.wav
 * - Ak pohyb trvá >10s → prehrá sirena.wav
 * - Po ustnutí pohybu → reset a pripravenosť na ďalší cyklus
 */

#include "Arduino.h"
#include "SD.h"
#include "Audio.h"  // ESP32-audioI2S library v3.x

// ========== KONFIGURÁCIA PINOV ==========
// SD Card (SPI)
#define SD_CS           5
#define SD_MOSI        23
#define SD_MISO        19
#define SD_SCLK        18

// I2S MAX98357A
#define I2S_BCLK       26
#define I2S_LRC        25
#define I2S_DOUT       22

// PIR Sensor
#define PIR_PIN        27

// ========== GLOBÁLNE PREMENNÉ ==========
Audio audio;  // Statický objekt

// Stavové premenné
unsigned long motionStartTime = 0;
bool isMotionActive = false;
bool alertPlayed = false;
bool sirenPlayed = false;

// Stavový stroj
enum SystemState {
  STATE_IDLE,
  STATE_ALERT_PLAYING,
  STATE_SIREN_PLAYING
};
SystemState currentState = STATE_IDLE;

// Audio súbory
const char* FILE_ALERT = "/alert.wav";
const char* FILE_SIREN = "/sirena.wav";

// ========== AUDIO CALLBACKS (v3.x - automatické rozpoznávanie) ==========
// Tieto funkcie knižnica nájde automaticky - NETREBA ich registrovať!

void audio_info(const char* info) {
  Serial.print("[AUDIO] INFO: ");
  Serial.println(info);
}

void audio_eof(const char* info) {
  Serial.print("[AUDIO] EOF: ");
  Serial.println(info);
  // Po dohratí môžeme resetovať flagy ak je to potrebné
}

void audio_log(const char* msg) {
  // Voliteľné detailné logovanie (odkomentujte pre debug)
  // Serial.print("[AUDIO] LOG: "); Serial.println(msg);
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println(F("\n=== PIR Audio Alert System ==="));
  
  // --- PIR Sensor ---
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  delay(100);  // Stabilizácia PIR
  
  // --- SD Card Initialization ---
  Serial.print(F("[SD] Initializing SD card... "));
  
  SPIClass* sdSPI = new SPIClass(HSPI);
  sdSPI->begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  
  if (!SD.begin(SD_CS, *sdSPI)) {
    Serial.println(F("FAILED!"));
    Serial.println(F("[ERROR] Check SD wiring, FAT32 format, and CS pin."));
    while (true) {
      Serial.println(F("[ERROR] No SD card. System halted."));
      delay(2000);
    }
  }
  Serial.println(F("OK"));
  
  // Test existencie súborov
  if (!SD.exists(FILE_ALERT)) {
    Serial.printf("[WARNING] File not found: %s\n", FILE_ALERT);
  }
  if (!SD.exists(FILE_SIREN)) {
    Serial.printf("[WARNING] File not found: %s\n", FILE_SIREN);
  }
  
  // --- I2S Audio Initialization (v3.x API) ---
  Serial.print(F("[I2S] Configuring MAX98357A... "));
  
  // setPinout vracia bool v novších verziách
  if (!audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
    Serial.println(F("FAILED!"));
  } else {
    Serial.println(F("OK"));
  }
  
  // Nastavenie hlasitosti (0-21)
  audio.setVolume(18);
  
  // Voliteľné: Nastavenie buffer size pre lepšiu stabilitu
  // audio.setBufSize(16384, 16384);  // ak potrebujete
  
  Serial.println(F("[OK] System ready. Waiting for motion..."));
  Serial.println(F("------------------------------------------------"));
}

// ========== MAIN LOOP ==========
void loop() {
  // 1. KRITICKÉ: audio.loop() musí bežať neustále
  audio.loop();
  
  // 2. Uvoľniť CPU pre iné úlohy
  vTaskDelay(1);
  
  // 3. Čítanie PIR senzora
  bool pirDetected = digitalRead(PIR_PIN) == HIGH;
  
  // ========== STAVOVÝ STROJ ==========
  switch (currentState) {
    
    case STATE_IDLE:
      if (pirDetected) {
        // >>> PRVÝ ZÁCHYT POHYBU <<<
        Serial.println(F("[PIR] Motion detected! Playing alert.wav"));
        
        motionStartTime = millis();
        isMotionActive = true;
        alertPlayed = false;
        sirenPlayed = false;
        currentState = STATE_ALERT_PLAYING;
        
        // Spustiť alert.wav
        if (!audio.connecttoFS(SD, FILE_ALERT)) {
          Serial.println(F("[ERROR] Failed to start alert.wav"));
          currentState = STATE_IDLE;
        }
      }
      break;
      
    case STATE_ALERT_PLAYING:
      if (!pirDetected) {
        Serial.println(F("[PIR] Motion stopped during alert."));
        resetSystem();
        break;
      }
      
      // Kontrola: uplynulo 10 sekúnd?
      if (!sirenPlayed && (millis() - motionStartTime >= 10000)) {
        Serial.println(F("[PIR] Motion >10s! Playing sirena.wav"));
        
        sirenPlayed = true;
        currentState = STATE_SIREN_PLAYING;
        
        // Spustiť sirénu (connecttoFS automaticky zastaví predchádzajúci súbor)
        if (!audio.connecttoFS(SD, FILE_SIREN)) {
          Serial.println(F("[ERROR] Failed to start sirena.wav"));
        }
      }
      break;
      
    case STATE_SIREN_PLAYING:
      if (!pirDetected) {
        Serial.println(F("[PIR] Motion stopped during siren."));
        resetSystem();
        break;
      }
      // Siréna hrá ďalej, kým je pohyb detekovaný
      break;
  }
  
  // Voliteľný status každých 5 sekúnd
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= 5000) {
    lastStatus = millis();
    printStatus();
  }
}

// ========== POMOCNÉ FUNKCIE ==========

void resetSystem() {
  Serial.println(F("[SYSTEM] Resetting to IDLE state."));
  
  isMotionActive = false;
  alertPlayed = false;
  sirenPlayed = false;
  motionStartTime = 0;
  currentState = STATE_IDLE;
  
  // Voliteľné: Zastaviť audio pri resete
  // audio.stopSong();
}

void printStatus() {
  static int counter = 0;
  counter++;
  
  Serial.printf("[STATUS] Loop: %d | State: %d | PIR: %s | Audio: %s\n",
    counter,
    currentState,
    digitalRead(PIR_PIN) == HIGH ? "HIGH" : "LOW",
    audio.isRunning() ? "PLAYING" : "IDLE"
  );
  
  // Info o aktuálne prehrávanom súbore (v3.x API)
  if (audio.isRunning()) {
    Serial.printf("         File: %s | Time: %lus/%lus | Vol: %d/21\n",
      audio.getCodecname(),
      audio.getAudioCurrentTime(),  // opravené: getAudioCurrentTime() namiesto getAudioFilePosition()
      audio.getAudioFileDuration(),
      audio.getVolume()
    );
  }
}

// Voliteľné: Serial command handler pre testovanie
void handleSerialCommand() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.equalsIgnoreCase("play alert")) {
      Serial.println(F("[CMD] Playing alert.wav"));
      audio.connecttoFS(SD, FILE_ALERT);
    }
    else if (cmd.equalsIgnoreCase("play siren")) {
      Serial.println(F("[CMD] Playing sirena.wav"));
      audio.connecttoFS(SD, FILE_SIREN);
    }
    else if (cmd.equalsIgnoreCase("stop")) {
      Serial.println(F("[CMD] Stopping audio"));
      audio.stopSong();
      resetSystem();
    }
    else if (cmd.equalsIgnoreCase("status")) {
      printStatus();
    }
  }
}
