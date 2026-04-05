// NOT WORKING ! it randomly starts playing song without human body moving, or when human moves, it cant start play sound
#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"

// === Konfigurácia pinov ===
#define I2S_DOUT    25    
#define I2S_BCLK    27    
#define I2S_LRC     26    
#define PIR_PIN     34    // PIR senzor
#define SD_CS       5     

// === Globálne premenné ===
Audio audio;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long MOTION_TIMEOUT = 5000; // 5 sekúnd

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32 Audio PIR - Starting...");

  pinMode(PIR_PIN, INPUT); // Pri pine 34 nie je potrebný PULLUP (je len vstupný)
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD init failed!");
    while (true) delay(1000);
  }
  Serial.println("✅ SD card initialized");

  // === Inicializácia audio ===
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15); 

  // ODSTRÁNENÉ: audio.connecttoFS(SD, "/sirena.wav"); 
  // Súbor sa spustí až pri detekcii pohybu v loop()

  Serial.println("✅ Setup complete - waiting for motion...");
  
  // Malá pauza na stabilizáciu PIR senzora po zapnutí
  delay(2000); 
}

void loop() {
  audio.loop();

  // Čítanie stavu senzora
  bool currentPIRState = (digitalRead(PIR_PIN) == HIGH);

  // 🟢 LOGIKA: Ak je detekovaný pohyb a hudba ešte nehrá
  if (currentPIRState && !motionDetected) {
    Serial.println("🚨 MOTION DETECTED! Starting playback...");
    
    // Spustíme prehrávanie len v tomto momente
    audio.connecttoFS(SD, "/sirena.wav");
    
    motionDetected = true;
    lastMotionTime = millis();
  }

  // 🔄 Ak pohyb trvá, stále aktualizujeme čas posledného pohybu
  if (currentPIRState && motionDetected) {
    lastMotionTime = millis();
  }

  // 🔴 LOGIKA: Ak pohyb ustal a uplynul timeout
  if (!currentPIRState && motionDetected) {
    if (millis() - lastMotionTime > MOTION_TIMEOUT) {
      Serial.println("⏱ Timeout - stopping audio");
      audio.stopSong(); // Úplne zastaví prehrávanie
      motionDetected = false;
    }
  }
}
