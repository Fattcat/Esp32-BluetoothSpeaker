#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include <SPI.h>
#include <BluetoothSerial.h>
#include <TMRpcm.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SD_CS 5 // Pin pre CS SD karty

// Definície tlačidiel
#define BUTTON_PLAY 32
#define BUTTON_VOL_UP 33
#define BUTTON_VOL_DOWN 34

// Inicializácia displeja
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Inicializácia Bluetooth
BluetoothSerial BT;

// Inicializácia prehrávača zvuku
TMRpcm audio;

int volume = 30;  // Default hlasitosť (0-100)

void setup() {
  // Inicializácia sériového portu
  Serial.begin(115200);
  
  // Inicializácia displeja
  if (!display.begin(SSD1306_I2C_ADDRESS, OLED_RESET)) {
    Serial.println(F("OLED not found!"));
    for(;;);
  }
  display.clearDisplay();
  
  // Inicializácia SD karty
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    display.println("SD Init Failed");
    display.display();
    while (1);
  }
  
  // Inicializácia Bluetooth
  BT.begin("DIY-BT-Speaker");  // Nastavte názov zariadenia
  display.println("Bluetooth: Searching...");
  display.display();
  
  // Inicializácia prehrávača zvuku
  audio.speakerPin = 25;  // Zvoľte pin pre výstup zvuku (GPIO 25)
  audio.begin();
  
  // Prehranie GreetingSound.wav po zapnutí
  playSound("GreetingSound.wav");
  
  // Nastavenie tlačidiel
  pinMode(BUTTON_PLAY, INPUT_PULLUP);
  pinMode(BUTTON_VOL_UP, INPUT_PULLUP);
  pinMode(BUTTON_VOL_DOWN, INPUT_PULLUP);
  
  delay(3000);  // Počkajte 3 sekundy pred pokusom o pripojenie Bluetooth
}

void loop() {
  // Zobrazenie stavu Bluetooth na displeji
  if (BT.hasClient()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Bluetooth Connected");
    display.display();
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Bluetooth Searching...");
    display.display();
  }
  
  // Kontrola tlačidiel
  if (digitalRead(BUTTON_PLAY) == LOW) {
    Serial.println("Play/Stop button pressed");
    // Implementujte spustenie/zastavenie prehrávania
    playSound("Disonnected.wav");
  }

  if (digitalRead(BUTTON_VOL_UP) == LOW) {
    Serial.println("Volume UP pressed");
    volume = min(100, volume + 10);
    audio.setVolume(volume);  // Nastavte hlasitosť
  }

  if (digitalRead(BUTTON_VOL_DOWN) == LOW) {
    Serial.println("Volume DOWN pressed");
    volume = max(0, volume - 10);
    audio.setVolume(volume);  // Nastavte hlasitosť
  }

  // Ak sa Bluetooth odpojí, prehráme zvuk
  if (!BT.hasClient()) {
    if (audio.isPlaying()) {
      audio.stopPlayback();
    }
    playSound("Disconnected.wav");
    delay(2000); // Čakajte 2 sekundy pred ďalším pokusom o pripojenie
  }

  delay(100);
}

void playSound(const char *filename) {
  Serial.print("Playing sound: ");
  Serial.println(filename);
  
  // Prehrávanie zvuku zo SD karty
  if (SD.exists(filename)) {
    audio.play(filename);  // Prehrávanie zvukového súboru
  } else {
    Serial.println("File not found!");
  }
}