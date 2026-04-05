#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"

// === Konfigurácia pinov ===
#define I2S_DOUT    25    // DIN na PCM5102
#define I2S_BCLK    27    // BCLK na PCM5102
#define I2S_LRC     26    // LRC na PCM5102
#define PIR_PIN     34    // PIR senzor (INPUT only pin)
#define SD_CS       5     // Chip Select pre SD kartu

// === Globálne premenné ===
Audio audio;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long MOTION_TIMEOUT = 5000; // 5 sekúnd bez pohybu = stop

// === Callbacky pre audio knižnicu ===
void audio_info(const char *info) {
  Serial.print("🎵 Info: ");
  Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("🔚 EOF: ");
  Serial.println(info);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32 Audio PIR - Starting...");

  // === Nastavenie pinov ===
  pinMode(PIR_PIN, INPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // === Inicializácia SD karty ===
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD init failed!");
    while (true) delay(1000);
  }
  Serial.println("✅ SD card initialized");

  // === Zoznam súborov na SD ===
  File root = SD.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      Serial.print("📁 Found: ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
    root.close();
  }

  // === Inicializácia audio ===
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // 0...21
  audio.connecttoFS(SD, "/sirena.wav"); // Voliteľné: načítať hneď
  
  Serial.println("✅ Setup complete - waiting for motion...");
}

void loop() {
  // === Spracovanie audio streamu (KRITICKÉ!) ===
  audio.loop();

  // === Čítanie PIR senzora ===
  bool currentMotion = digitalRead(PIR_PIN) == HIGH;

  if (currentMotion && !motionDetected) {
    // 🟢 NOVÝ pohyb detekovaný
    Serial.println("🚨 MOTION DETECTED!");
    audio.stopSong();
    audio.connecttoFS(SD, "/sirena.wav");
    lastMotionTime = millis();
    motionDetected = true;
    
  } else if (!currentMotion && motionDetected) {
    // 🔴 Pohyb ustal - spusti timeout
    if (millis() - lastMotionTime > MOTION_TIMEOUT) {
      Serial.println("⏱ Timeout - stopping audio");
      audio.stopSong();
      motionDetected = false;
    }
    
  } else if (currentMotion && motionDetected) {
    // 🔄 Pohyb pokračuje - reset timer
    lastMotionTime = millis();
  }

  // === Voliteľné: debug každých 10s ===
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 10000) {
    Serial.printf("📊 Status: motion=%d, playing=%d\n", 
                  motionDetected, audio.isRunning());
    lastDebug = millis();
  }
}
