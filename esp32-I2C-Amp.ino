#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <SPI.h>
#include <BluetoothSerial.h>

// ESP8266Audio knižnice pre I2S + WAV
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ── Displej ──────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── SD karta ─────────────────────────────────────────────
#define SD_CS 5

// ── Tlačidlá ─────────────────────────────────────────────
#define BUTTON_PLAY     32
#define BUTTON_VOL_UP   33
#define BUTTON_VOL_DOWN 14  // ← zmenené z 34 (34 nemá pull-up!)

// ── I2S piny pre MAX98357 ─────────────────────────────────
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// ── Audio objekty ─────────────────────────────────────────
AudioGeneratorWAV  *wav  = nullptr;
AudioFileSourceSD  *file = nullptr;
AudioOutputI2S     *out  = nullptr;

float volume = 1.0f;  // Rozsah: 0.0 – 4.0

BluetoothSerial BT;

// ─────────────────────────────────────────────────────────
void updateDisplay(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (strlen(line2) > 0) display.println(line2);
  display.display();
}

// ─────────────────────────────────────────────────────────
void playSound(const char *filename) {
  // Zastav aktuálne prehrávanie
  if (wav && wav->isRunning()) {
    wav->stop();
    delete wav;  wav  = nullptr;
  }
  if (file) {
    file->close();
    delete file; file = nullptr;
  }

  // Súbory na SD karte musia mať lomku: "/nazov.wav"
  if (!SD.exists(filename)) {
    Serial.printf("[Audio] Súbor nenájdený: %s\n", filename);
    return;
  }

  file = new AudioFileSourceSD(filename);
  wav  = new AudioGeneratorWAV();
  wav->begin(file, out);

  Serial.printf("[Audio] Prehráva: %s\n", filename);
}

// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // OLED displej
  if (!display.begin(SSD1306_I2C_ADDRESS, OLED_RESET)) {
    Serial.println("OLED nenájdený!");
    for (;;);
  }
  updateDisplay("Spustam...");

  // SD karta
  if (!SD.begin(SD_CS)) {
    Serial.println("SD karta zlyhala!");
    updateDisplay("SD: CHYBA");
    while (1);
  }
  Serial.println("SD karta OK");

  // I2S výstup – MAX98357
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(volume);          // Počiatočná hlasitosť
  out->SetOutputModeMono(true);  // Mono výstup (MAX98357 je mono)

  // Bluetooth
  BT.begin("DIY-BT-Speaker");
  updateDisplay("BT: Hladam...");

  // Tlačidlá
  pinMode(BUTTON_PLAY,     INPUT_PULLUP);
  pinMode(BUTTON_VOL_UP,   INPUT_PULLUP);
  pinMode(BUTTON_VOL_DOWN, INPUT_PULLUP);  // GPIO14 má pull-up

  // Uvítací zvuk
  playSound("/GreetingSound.wav");

  delay(1000);
}

// ─────────────────────────────────────────────────────────
void loop() {
  // !! Toto musí byť volané každý cyklus – posiela I2S dáta !!
  if (wav && wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      file->close();
      delete wav;  wav  = nullptr;
      delete file; file = nullptr;
      Serial.println("[Audio] Prehrávanie skončilo");
    }
  }

  // Stav Bluetooth
  static bool lastBTState = false;
  bool btConnected = BT.hasClient();

  if (btConnected != lastBTState) {
    lastBTState = btConnected;
    if (btConnected) {
      updateDisplay("BT: Pripojeny");
      playSound("/Connected.wav");
    } else {
      updateDisplay("BT: Hladam...");
      playSound("/Disconnected.wav");
    }
  }

  // ── Tlačidlo PLAY / STOP ──────────────────────────────
  static bool lastPlay = HIGH;
  bool btnPlay = digitalRead(BUTTON_PLAY);
  if (btnPlay == LOW && lastPlay == HIGH) {
    delay(30);  // Debounce
    if (wav && wav->isRunning()) {
      wav->stop();
      Serial.println("[Audio] Zastavené");
    } else {
      playSound("/song.wav");
    }
  }
  lastPlay = btnPlay;

  // ── Tlačidlo VOLUME UP ────────────────────────────────
  static bool lastVolUp = HIGH;
  bool btnVolUp = digitalRead(BUTTON_VOL_UP);
  if (btnVolUp == LOW && lastVolUp == HIGH) {
    delay(30);
    volume = min(4.0f, volume + 0.25f);
    out->SetGain(volume);
    Serial.printf("[Audio] Hlasitosť: %.2f\n", volume);
  }
  lastVolUp = btnVolUp;

  // ── Tlačidlo VOLUME DOWN ──────────────────────────────
  static bool lastVolDown = HIGH;
  bool btnVolDown = digitalRead(BUTTON_VOL_DOWN);
  if (btnVolDown == LOW && lastVolDown == HIGH) {
    delay(30);
    volume = max(0.0f, volume - 0.25f);
    out->SetGain(volume);
    Serial.printf("[Audio] Hlasitosť: %.2f\n", volume);
  }
  lastVolDown = btnVolDown;
}
