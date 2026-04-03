// ── MUST BE FIRST: Enable legacy I2S API ───────────────────
#define A2DP_LEGACY_I2S_SUPPORT 1
#include "BluetoothA2DPSink.h"

// ── I2S piny pre MAX98357A ─────────────────────────────────
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// ── Tlačidlá (INPUT_PULLUP – aktívne LOW) ──────────────────
#define BUTTON_PLAY     32
#define BUTTON_VOL_UP   33
#define BUTTON_VOL_DOWN 14

// ── Bluetooth A2DP Sink ────────────────────────────────────
BluetoothA2DPSink a2dp_sink;


int volume = 16;

// ───────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Setup] Start Bluetooth reproduktora");

  // ── Tlačidlá s pull-up ───────────────────────────────────
  pinMode(BUTTON_PLAY,     INPUT_PULLUP);
  pinMode(BUTTON_VOL_UP,   INPUT_PULLUP);
  pinMode(BUTTON_VOL_DOWN, INPUT_PULLUP);

  // ── Konfigurácia I2S pinov pre MAX98357A ─────────────────
  i2s_pin_config_t my_pin_config = {
    .mck_io_num   = I2S_PIN_NO_CHANGE,
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);

  // ── Nastavenie hlasitosti ────────────────────────────────
  a2dp_sink.set_volume(volume);

  // ── Štart Bluetooth ──────────────────────────────────────
  a2dp_sink.start("DIY-BT-Speaker");

  Serial.println("[Setup] Done. Pair with 'DIY-BT-Speaker'");
}

// ───────────────────────────────────────────────────────────
void loop() {
  // ── Tlačidlo PLAY/PAUSE ──────────────────────────────────
  static bool lastPlay = HIGH;
  bool btnPlay = digitalRead(BUTTON_PLAY);
  if (btnPlay == LOW && lastPlay == HIGH) {
    delay(30);
    if (a2dp_sink.is_connected()) {
      a2dp_sink.play();  // Pošle AVRCP play/pause na telefón
      Serial.println("[BT] Play/Pause");
    }
  }
  lastPlay = btnPlay;

  // ── Tlačidlo VOLUME UP ───────────────────────────────────
  static bool lastVolUp = HIGH;
  bool btnVolUp = digitalRead(BUTTON_VOL_UP);
  if (btnVolUp == LOW && lastVolUp == HIGH) {
    delay(30);
    volume = min(63, volume + 4);
    a2dp_sink.set_volume(volume);
    Serial.printf("[Vol] %d\n", volume);
  }
  lastVolUp = btnVolUp;

  // ── Tlačidlo VOLUME DOWN ─────────────────────────────────
  static bool lastVolDown = HIGH;
  bool btnVolDown = digitalRead(BUTTON_VOL_DOWN);
  if (btnVolDown == LOW && lastVolDown == HIGH) {
    delay(30);
    volume = max(0, volume - 4);
    a2dp_sink.set_volume(volume);
    Serial.printf("[Vol] %d\n", volume);
  }
  lastVolDown = btnVolDown;

  delay(10);
}
