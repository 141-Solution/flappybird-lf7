// ============================================================================
// Flappy Bird fuer D1 mini (ESP8266) mit OLED 0.66" (SSD1306), ADXL345, Buzzer
// Lib: Adafruit SSD1306 Wemos Mini OLED, Adafruit GFX
// ============================================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Display (64x48, kein Offset noetig mit Wemos OLED Lib) ---
#define W 64
#define H 48
Adafruit_SSD1306 display(W, H, &Wire, -1);

// --- Pins ---
#define SDA_PIN D5                    // I2C SDA
#define SCL_PIN D7                    // I2C SCL
#define BUZZER_PIN D8                 // Buzzer (active-low)
#define ADXL_INT2 D6                  // ADXL345 INT2 (reserviert)
#define ADXL_CS D3                    // ADXL345 CS (reserviert, I2C Modus)

// --- ADXL345 ---
#define ADXL_ADDR 0x53
#define REG_POWER 0x2D
#define REG_FORMAT 0x31
#define REG_DATAX0 0x32

// --- Spielkonstanten ---
#define FRAME_MS 33                   // ~30 FPS
#define GRAVITY 0.22f                 // Schwerkraft pro Frame
#define FLAP_VEL -2.0f               // Aufwaertsgeschwindigkeit beim Flap
#define PIPE_SPD 1                    // Rohr-Geschwindigkeit
#define PIPE_GAP 14                   // Luecke zwischen Rohren
#define PIPE_W 8                      // Rohrbreite
#define BIRD_X 12                     // Vogel X-Position (fest)
#define BIRD_SZ 4                     // Vogelgroesse
#define FLAP_THRESH 100               // Kipp-Empfindlichkeit (niedriger = leichter)

// --- Spielvariablen ---
float birdY, birdVY;
int pipeX, gapY, score, best;
bool pipePassed, flapLatch;
int16_t az, lastAz;
bool sensorOk;
unsigned long lastFrame;
bool buzzing;
unsigned long buzzEnd;
enum State { PLAY, DEAD } state = PLAY;

// ============================================================================
// I2C Helfer
// ============================================================================

bool i2cWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADXL_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool i2cRead6(uint8_t* buf) {
  Wire.beginTransmission(ADXL_ADDR);
  Wire.write(REG_DATAX0);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)ADXL_ADDR, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 6; i++) buf[i] = Wire.read();
  return true;
}

// ============================================================================
// Sensor
// ============================================================================

bool initSensor() {
  bool ok = true;
  ok &= i2cWrite(REG_POWER, 0x00);   // Standby
  ok &= i2cWrite(REG_FORMAT, 0x08);  // Full-Res +-2g
  ok &= i2cWrite(REG_POWER, 0x08);   // Messmodus
  return ok;
}

int16_t readZ() {
  uint8_t buf[6];
  if (!i2cRead6(buf)) { sensorOk = false; return lastAz; }
  sensorOk = true;
  return (int16_t)((buf[5] << 8) | buf[4]);
}

// ============================================================================
// Buzzer (active-low)
// ============================================================================

void buzzOff() { digitalWrite(BUZZER_PIN, HIGH); buzzing = false; }
void buzzOn()  { digitalWrite(BUZZER_PIN, LOW);  buzzing = true; }
void beep(uint16_t ms) { buzzOn(); buzzEnd = millis() + ms; }
void updateBuzz() { if (buzzing && (long)(millis() - buzzEnd) >= 0) buzzOff(); }

// ============================================================================
// Flap-Erkennung (Kippen der Z-Achse)
// ============================================================================

bool detectFlap() {
  int16_t d = az - lastAz;
  if (d < 0) d = -d;
  lastAz = az;
  if (d > FLAP_THRESH && !flapLatch) { flapLatch = true; return true; }
  if (d <= FLAP_THRESH) flapLatch = false;
  return false;
}

// ============================================================================
// Spiel-Logik
// ============================================================================

void resetGame() {
  birdY = H / 2.0f;
  birdVY = 0;
  pipeX = W;
  gapY = random(6, H - PIPE_GAP - 6);
  score = 0;
  pipePassed = false;
  flapLatch = false;
}

bool collision() {
  if (birdY < 0 || birdY + BIRD_SZ > H) return true;
  if (BIRD_X + BIRD_SZ >= pipeX && BIRD_X <= pipeX + PIPE_W) {
    if (birdY < gapY || birdY + BIRD_SZ > gapY + PIPE_GAP) return true;
  }
  return false;
}

// ============================================================================
// Zeichnen
// ============================================================================

void drawGame() {
  display.clearDisplay();
  display.drawRect(0, 0, W, H, SSD1306_WHITE);           // Rahmen
  display.fillRect(pipeX, 0, PIPE_W, gapY, SSD1306_WHITE);                          // Rohr oben
  display.fillRect(pipeX, gapY + PIPE_GAP, PIPE_W, H - gapY - PIPE_GAP, SSD1306_WHITE); // Rohr unten
  display.fillRect(BIRD_X, (int)birdY, BIRD_SZ, BIRD_SZ, SSD1306_WHITE);            // Vogel
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(score);                                   // Score oben links
  display.display();
}

void drawDead() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(6, 4);   display.print("GAME OVER");
  display.setCursor(6, 16);  display.print("Score:");  display.print(score);
  display.setCursor(6, 26);  display.print("Best :");  display.print(best);
  display.setCursor(2, 38);  display.print("Tilt:restart");
  display.display();
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzOff();

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FEHLER");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  sensorOk = initSensor();
  if (sensorOk) {
    az = readZ(); lastAz = az;
    beep(80);                         // Einmal piepen bei erfolgreichem Start
  } else {
    for (int i = 0; i < 3; i++) {     // 3x schnell piepen bei Sensor-Fehler
      buzzOn(); delay(80);
      buzzOff(); delay(80);
    }
  }
  Serial.println(sensorOk ? "Sensor OK" : "Sensor FEHLER");

  randomSeed(analogRead(A0));
  resetGame();
  state = PLAY;
}

// ============================================================================
// Loop
// ============================================================================

void loop() {
  updateBuzz();
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();
  az = readZ();

  if (state == PLAY) {
    birdVY += GRAVITY;
    birdY += birdVY;
    if (detectFlap()) { birdVY = FLAP_VEL; beep(30); }

    pipeX -= PIPE_SPD;
    if (!pipePassed && pipeX + PIPE_W < BIRD_X) { pipePassed = true; score++; beep(20); }
    if (pipeX + PIPE_W < 0) { pipeX = W; gapY = random(6, H - PIPE_GAP - 6); pipePassed = false; }

    if (collision()) { state = DEAD; if (score > best) best = score; beep(150); }
    drawGame();
    return;
  }

  // GAME OVER
  drawDead();
  if (detectFlap()) { resetGame(); state = PLAY; beep(30); }
}
