// ============================================================================
// Flappy Bird für D1 mini (ESP8266) mit OLED (SSD1306), ADXL345 und Buzzer
// ============================================================================

#include <Wire.h>                                // Bindet die I2C-Bibliothek ein.
#include <Adafruit_GFX.h>                        // Bindet die Grafik-Basisbibliothek ein.
#include <Adafruit_SSD1306.h>                    // Bindet die SSD1306-OLED-Bibliothek ein.

#define SCREEN_WIDTH 128                         // Definiert die physische Displaybreite in Pixeln.
#define SCREEN_HEIGHT 64                         // Definiert die physische Displayhöhe in Pixeln.
#define OLED_RESET -1                            // Definiert keinen separaten Reset-Pin für OLED.
#define OLED_ADDRESS 0x3C                        // Definiert die I2C-Adresse des OLED-Displays.
#define DISP_WIDTH 64                            // Definiert die nutzbare Spielfeldbreite in Pixeln.
#define DISP_HEIGHT 48                           // Definiert die nutzbare Spielfeldhöhe in Pixeln.
#define X_OFFSET 32                              // Definiert den X-Offset des 64x48-Fensters im 128x64-Buffer.
#define Y_OFFSET 14                              // Definiert den Y-Offset des 64x48-Fensters im 128x64-Buffer.
#define SX(x) ((x) + X_OFFSET)                   // Übersetzt Spielfeld-X in physische Display-X-Koordinate.
#define SY(y) ((y) + Y_OFFSET)                   // Übersetzt Spielfeld-Y in physische Display-Y-Koordinate.

#define SDA_PIN D2                               // Definiert den I2C-SDA-Pin auf D2.
#define SCL_PIN D1                               // Definiert den I2C-SCL-Pin auf D1.

#define ADXL345_ADDR 0x53                        // Definiert die I2C-Adresse des ADXL345.
#define ADXL345_REG_POWER_CTL 0x2D               // Definiert das Power-Control-Register.
#define ADXL345_REG_DATA_FORMAT 0x31             // Definiert das Datenformat-Register.
#define ADXL345_REG_DATAX0 0x32                  // Definiert das erste Datenregister (X0).

#define BUZZER_PIN D5                            // Definiert den Buzzer-Pin auf D5 (anpassbar).

#define FRAME_MS 33                              // Definiert die Framedauer auf ca. 30 FPS.
#define GRAVITY 0.20f                            // Definiert die Schwerkraft pro Frame.
#define FLAP_VELOCITY -1.90f                     // Definiert die Aufwärtsgeschwindigkeit beim Flap.
#define PIPE_SPEED 1                             // Definiert die horizontale Rohrgeschwindigkeit.
#define PIPE_GAP 16                              // Definiert die Lücke zwischen oberem/unterem Rohr.
#define PIPE_WIDTH 10                            // Definiert die Breite der Rohre.
#define BIRD_X 14                                // Definiert die feste X-Position des Vogels.
#define BIRD_SIZE 4                              // Definiert die quadratische Vogelgröße in Pixeln.
#define FLAP_THRESHOLD 120                       // Definiert den Z-Delta-Schwellwert für Flap (hoch und runter).
#define BUZZER_ACTIVE_LOW true                   // Definiert den Buzzer als active-low gemäß Spezifikation.

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Erzeugt das Displayobjekt.

enum GameState { BOOT, READY, PLAYING, GAME_OVER }; // Definiert die möglichen Spielzustände.
GameState gameState = BOOT;                      // Initialisiert den Startzustand.

float birdY = DISP_HEIGHT / 2.0f;                // Speichert die aktuelle Y-Position des Vogels.
float birdVY = 0.0f;                             // Speichert die aktuelle vertikale Geschwindigkeit.
int pipeX = DISP_WIDTH;                          // Speichert die X-Position des aktuellen Rohrs.
int gapY = 20;                                   // Speichert die Y-Startposition der Rohrlücke.
int score = 0;                                   // Speichert den aktuellen Punktestand.
int highScore = 0;                               // Speichert den Highscore (nur RAM).
bool pipePassed = false;                         // Markiert, ob Rohr bereits gewertet wurde.

int16_t ax = 0;                                  // Speichert den letzten X-Wert des ADXL345.
int16_t ay = 0;                                  // Speichert den letzten Y-Wert des ADXL345.
int16_t az = 0;                                  // Speichert den letzten Z-Wert des ADXL345.
int16_t lastAz = 0;                              // Speichert den vorherigen Z-Wert für Delta-Bildung.
bool sensorOk = false;                           // Markiert, ob der Sensor korrekt initialisiert ist.

unsigned long lastFrameTs = 0;                   // Speichert den Zeitstempel des letzten Frames.
bool flapLatch = false;                          // Sperrt Mehrfachflap solange Schwellwert gehalten wird.
bool buzzerOn = false;                           // Speichert den aktuellen Ein/Aus-Zustand des Buzzers.
unsigned long buzzerOffTs = 0;                   // Speichert den Abschaltzeitpunkt des Buzzers.

bool i2cWriteByte(uint8_t dev, uint8_t reg, uint8_t data) { // Schreibt ein Byte in ein I2C-Register.
  Wire.beginTransmission(dev);                   // Startet die Übertragung zum Zielgerät.
  Wire.write(reg);                               // Sendet die Registeradresse.
  Wire.write(data);                              // Sendet den Registerwert.
  return Wire.endTransmission() == 0;            // Liefert true bei erfolgreicher Übertragung.
}

bool i2cReadBytes(uint8_t dev, uint8_t reg, uint8_t* buf, uint8_t len) { // Liest mehrere Bytes aus I2C.
  Wire.beginTransmission(dev);                   // Startet die Übertragung zum Zielgerät.
  Wire.write(reg);                               // Sendet die Start-Registeradresse.
  if (Wire.endTransmission(false) != 0) {        // Prüft Repeated-Start ohne Stop.
    return false;                                // Bricht bei Übertragungsfehler ab.
  }
  uint8_t received = Wire.requestFrom(dev, len); // Fordert die gewünschte Byteanzahl an.
  if (received != len) {                         // Prüft, ob alle Bytes angekommen sind.
    return false;                                // Bricht bei unvollständigem Empfang ab.
  }
  for (uint8_t i = 0; i < len; i++) {            // Iteriert über alle empfangenen Bytes.
    buf[i] = Wire.read();                        // Liest Byte für Byte in den Puffer.
  }
  return true;                                   // Meldet erfolgreichen Lesevorgang.
}

bool initAdxl345() {                             // Initialisiert den ADXL345 im Messmodus.
  bool ok = true;                                // Legt eine Sammelvariable für den Status an.
  ok &= i2cWriteByte(ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x00); // Setzt Sensor zuerst in Standby.
  ok &= i2cWriteByte(ADXL345_ADDR, ADXL345_REG_DATA_FORMAT, 0x08); // Aktiviert Full-Resolution ±2g.
  ok &= i2cWriteByte(ADXL345_ADDR, ADXL345_REG_POWER_CTL, 0x08); // Aktiviert den Messmodus.
  return ok;                                     // Gibt den Initialisierungsstatus zurück.
}

bool readAdxl345(int16_t& x, int16_t& y, int16_t& z) { // Liest X, Y und Z vom ADXL345.
  uint8_t raw[6];                                // Legt einen Puffer für 6 Rohdatenbytes an.
  if (!i2cReadBytes(ADXL345_ADDR, ADXL345_REG_DATAX0, raw, 6)) { // Liest die 6 Datenbytes.
    return false;                                // Bricht bei Lesefehler ab.
  }
  x = (int16_t)((raw[1] << 8) | raw[0]);        // Kombiniert X-High/Low zu int16.
  y = (int16_t)((raw[3] << 8) | raw[2]);        // Kombiniert Y-High/Low zu int16.
  z = (int16_t)((raw[5] << 8) | raw[4]);        // Kombiniert Z-High/Low zu int16.
  return true;                                   // Meldet erfolgreiche Sensorlesung.
}

void setBuzzer(bool on) {                        // Schaltet den Buzzer unter Berücksichtigung active-low.
  if (BUZZER_ACTIVE_LOW) {                       // Prüft, ob active-low-Konfiguration aktiv ist.
    digitalWrite(BUZZER_PIN, on ? LOW : HIGH);   // Schreibt invertierten Pegel für active-low-Buzzer.
  } else {                                       // Verarbeitet den normalen active-high-Fall.
    digitalWrite(BUZZER_PIN, on ? HIGH : LOW);   // Schreibt direkten Pegel für active-high-Buzzer.
  }
  buzzerOn = on;                                 // Speichert den neuen Buzzerzustand.
}

void beep(uint16_t freq, uint16_t durMs) {       // Startet einen nicht-blockierenden aktiven Buzzer-Puls.
  (void)freq;                                    // Ignoriert Frequenz bei aktivem Buzzer (nur Ein/Aus möglich).
  setBuzzer(true);                               // Schaltet den Buzzer ein.
  buzzerOffTs = millis() + durMs;                // Plant den Abschaltzeitpunkt relativ zur aktuellen Zeit.
}

void serviceBuzzer() {                           // Schaltet den Buzzer automatisch nach Ablauf wieder aus.
  if (buzzerOn && ((long)(millis() - buzzerOffTs) >= 0)) { // Prüft, ob ein aktiver Puls abgelaufen ist.
    setBuzzer(false);                            // Schaltet den Buzzer wieder aus.
  }
}

void resetGame() {                                // Setzt alle Spielwerte auf Startzustand zurück.
  birdY = DISP_HEIGHT / 2.0f;                    // Setzt Vogel vertikal in die Mitte.
  birdVY = 0.0f;                                 // Setzt vertikale Geschwindigkeit zurück.
  pipeX = DISP_WIDTH;                            // Setzt Rohr an den rechten Rand.
  gapY = random(8, DISP_HEIGHT - PIPE_GAP - 8); // Wählt eine zufällige Lückenposition.
  score = 0;                                     // Setzt den Punktestand zurück.
  pipePassed = false;                            // Setzt Rohr-Wertungsmarker zurück.
  flapLatch = false;                             // Hebt Flap-Sperre auf.
}

bool detectFlapFromTilt() {                       // Erkennt einen Flap über Kippbewegung.
  int16_t deltaZ = az - lastAz;                  // Berechnet die Änderung zwischen aktuellem und letztem Z-Wert.
  int16_t absDeltaZ = (deltaZ >= 0) ? deltaZ : -deltaZ; // Bildet den Betrag der Z-Änderung für beide Richtungen.
  bool flapNow = (absDeltaZ > FLAP_THRESHOLD);   // Prüft, ob Z-Änderung den Schwellwert übersteigt.
  lastAz = az;                                   // Aktualisiert den letzten Z-Wert für den nächsten Zyklus.
  if (flapNow && !flapLatch) {                   // Prüft auf neue Flanke statt Dauersignal.
    flapLatch = true;                            // Aktiviert Sperre bis Sensor zurückfällt.
    return true;                                 // Meldet einen gültigen Flap-Impuls.
  }
  if (!flapNow) {                                // Prüft, ob Schwellwert wieder unterschritten wurde.
    flapLatch = false;                           // Deaktiviert Sperre für nächsten Flap.
  }
  return false;                                  // Meldet keinen neuen Flap.
}

bool checkCollision() {                           // Prüft Kollision mit Rand oder Rohren.
  if (birdY < 0 || birdY + BIRD_SIZE > DISP_HEIGHT) { // Prüft Kollision mit oberem/unterem Rand.
    return true;                                 // Meldet Kollision.
  }

  int birdLeft = BIRD_X;                         // Berechnet linke Vogelkante.
  int birdRight = BIRD_X + BIRD_SIZE;            // Berechnet rechte Vogelkante.
  int pipeLeft = pipeX;                          // Berechnet linke Rohrkante.
  int pipeRight = pipeX + PIPE_WIDTH;            // Berechnet rechte Rohrkante.

  bool overlapX = (birdRight >= pipeLeft) && (birdLeft <= pipeRight); // Prüft horizontale Überdeckung.
  bool inGap = (birdY >= gapY) && ((birdY + BIRD_SIZE) <= (gapY + PIPE_GAP)); // Prüft ob Vogel in Lücke ist.

  if (overlapX && !inGap) {                      // Prüft Rohrkollision bei horizontaler Überdeckung.
    return true;                                 // Meldet Kollision.
  }

  return false;                                  // Meldet keine Kollision.
}

void updateGame() {                               // Aktualisiert die Spielphysik und Logik.
  birdVY += GRAVITY;                             // Erhöht vertikale Geschwindigkeit durch Schwerkraft.
  birdY += birdVY;                               // Aktualisiert Y-Position des Vogels.

  if (detectFlapFromTilt()) {                    // Prüft auf Flap-Impuls vom Sensor.
    birdVY = FLAP_VELOCITY;                      // Setzt Aufwärtsgeschwindigkeit.
    beep(1500, 35);                              // Spielt kurzen Flap-Sound.
  }

  pipeX -= PIPE_SPEED;                           // Bewegt Rohr nach links.

  if (!pipePassed && (pipeX + PIPE_WIDTH < BIRD_X)) { // Prüft ob Vogel das Rohr passiert hat.
    pipePassed = true;                           // Markiert Rohr als gewertet.
    score++;                                     // Erhöht den Punktestand.
    beep(2200, 25);                              // Spielt Punktesound.
  }

  if (pipeX + PIPE_WIDTH < 0) {                  // Prüft ob Rohr vollständig aus dem Bild ist.
    pipeX = DISP_WIDTH;                          // Setzt neues Rohr an den rechten Rand.
    gapY = random(6, DISP_HEIGHT - PIPE_GAP - 6); // Wählt neue zufällige Lückenposition.
    pipePassed = false;                          // Aktiviert Wertung für neues Rohr.
  }

  if (checkCollision()) {                        // Prüft Kollision nach Bewegungsupdate.
    gameState = GAME_OVER;                       // Wechselt in den Game-Over-Zustand.
    if (score > highScore) {                     // Prüft auf neuen Highscore.
      highScore = score;                         // Speichert neuen Highscore.
    }
    beep(400, 180);                              // Spielt Game-Over-Sound.
  }
}

void drawHud() {                                  // Zeichnet Score und Sensorstatus.
  display.setTextSize(1);                        // Setzt kleine Schriftgröße.
  display.setTextColor(SSD1306_WHITE);           // Setzt Schriftfarbe auf Weiß.
  display.setCursor(SX(0), SY(0));               // Setzt Cursor oben links im 64x48-Fenster.
  display.print("S:");                          // Schreibt Score-Label.
  display.print(score);                          // Schreibt aktuellen Score.
  display.setCursor(SX(38), SY(0));              // Setzt Cursor für Sensorindikator im Fenster.
  display.print(sensorOk ? "OK" : "ERR");     // Zeigt Sensorstatus an.
}

void drawGameScene() {                            // Rendert die aktive Spielszene.
  display.clearDisplay();                        // Löscht den Displaypuffer.

  display.drawRect(SX(0), SY(0), DISP_WIDTH, DISP_HEIGHT, SSD1306_WHITE); // Zeichnet den Rahmen des 64x48-Fensters.
  display.drawLine(SX(0), SY(DISP_HEIGHT - 1), SX(DISP_WIDTH - 1), SY(DISP_HEIGHT - 1), SSD1306_WHITE); // Zeichnet Bodenlinie im Fenster.

  display.fillRect(SX(pipeX), SY(0), PIPE_WIDTH, gapY, SSD1306_WHITE); // Zeichnet oberes Rohrsegment.
  display.fillRect(SX(pipeX), SY(gapY + PIPE_GAP), PIPE_WIDTH, DISP_HEIGHT - (gapY + PIPE_GAP), SSD1306_WHITE); // Zeichnet unteres Rohrsegment.

  display.fillRect(SX(BIRD_X), SY((int)birdY), BIRD_SIZE, BIRD_SIZE, SSD1306_WHITE); // Zeichnet den Vogel als Quadrat.

  drawHud();                                     // Zeichnet HUD über die Szene.

  display.display();                             // Überträgt den Puffer auf das OLED.
}

void drawReadyScreen() {                          // Zeichnet den Startbildschirm.
  display.clearDisplay();                        // Löscht den Displaypuffer.
  display.setTextSize(1);                        // Setzt kleine Schriftgröße.
  display.setTextColor(SSD1306_WHITE);           // Setzt Schriftfarbe auf Weiß.
  display.drawRect(SX(0), SY(0), DISP_WIDTH, DISP_HEIGHT, SSD1306_WHITE); // Zeichnet den Rahmen des 64x48-Fensters.
  display.setCursor(SX(6), SY(10));              // Setzt Cursor für Titel im Fenster.
  display.println("FLAPPY BIRD");              // Schreibt den Spieltitel.
  display.setCursor(SX(4), SY(24));              // Setzt Cursor für Hinweis im Fenster.
  display.println("Tilt to start");            // Schreibt Startanweisung.
  display.display();                             // Aktualisiert das OLED.
}

void drawGameOverScreen() {                       // Zeichnet den Game-Over-Bildschirm.
  display.clearDisplay();                        // Löscht den Displaypuffer.
  display.setTextSize(1);                        // Setzt kleine Schriftgröße.
  display.setTextColor(SSD1306_WHITE);           // Setzt Schriftfarbe auf Weiß.
  display.drawRect(SX(0), SY(0), DISP_WIDTH, DISP_HEIGHT, SSD1306_WHITE); // Zeichnet den Rahmen des 64x48-Fensters.
  display.setCursor(SX(10), SY(8));              // Setzt Cursor für Überschrift im Fenster.
  display.println("GAME OVER");                // Schreibt Game-Over-Text.
  display.setCursor(SX(8), SY(20));              // Setzt Cursor für Score-Zeile im Fenster.
  display.print("Score: ");                    // Schreibt Score-Label.
  display.println(score);                        // Schreibt aktuellen Score.
  display.setCursor(SX(8), SY(30));              // Setzt Cursor für Highscore-Zeile im Fenster.
  display.print("Best : ");                    // Schreibt Highscore-Label.
  display.println(highScore);                    // Schreibt Highscore.
  display.setCursor(SX(0), SY(40));              // Setzt Cursor für Neustarthinweis im Fenster.
  display.println("Tilt for restart");         // Schreibt Neustartanweisung.
  display.display();                             // Aktualisiert das OLED.
}

void setup() {                                    // Führt die Initialisierung beim Start aus.
  Serial.begin(115200);                          // Startet die serielle Debug-Schnittstelle.
  delay(200);                                    // Wartet kurz zur Stabilisierung.

  pinMode(BUZZER_PIN, OUTPUT);                   // Konfiguriert den Buzzer-Pin als Ausgang.
  setBuzzer(false);                              // Stellt sicher, dass der active-low-Buzzer aus ist.

  Wire.begin(SDA_PIN, SCL_PIN);                  // Initialisiert I2C mit D1-mini-Pins.

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // Prüft OLED-Initialisierung.
    Serial.println("FEHLER: OLED init fehlgeschlagen"); // Gibt OLED-Fehler aus.
    while (true) {                               // Bleibt bei kritischem Fehler in Endlosschleife.
      delay(1000);                               // Wartet zyklisch zur Entlastung.
    }
  }

  display.clearDisplay();                        // Löscht den Displaypuffer.
  display.display();                             // Überträgt leeren Puffer.

  sensorOk = initAdxl345();                      // Initialisiert den ADXL345-Sensor.
  if (!sensorOk) {                               // Prüft Sensorstatus.
    Serial.println("FEHLER: ADXL345 init fehlgeschlagen"); // Gibt Sensorfehler aus.
  } else {                                       // Verarbeitet erfolgreichen Sensorstart.
    Serial.println("ADXL345 init erfolgreich"); // Gibt Sensor-OK aus.
    if (readAdxl345(ax, ay, az)) {               // Liest einen Initialwert zur stabilen Delta-Bildung.
      lastAz = az;                               // Setzt den Referenzwert für die Z-Delta-Erkennung.
    }
  }

  randomSeed(analogRead(A0));                    // Initialisiert Zufall mit analogem Rauschen.
  resetGame();                                   // Setzt Spielzustand auf Anfang.
  gameState = READY;                             // Wechselt in den Startbildschirm.
  drawReadyScreen();                             // Zeichnet initialen Startbildschirm.

  Serial.println("Setup abgeschlossen");        // Meldet Abschluss der Initialisierung.
}

void loop() {                                     // Führt die Hauptschleife kontinuierlich aus.
  serviceBuzzer();                               // Wartet den Buzzer unabhängig vom Spielzustand.
  unsigned long now = millis();                  // Liest den aktuellen Zeitstempel.
  if (now - lastFrameTs < FRAME_MS) {            // Begrenzt die Bildrate auf FRAME_MS.
    return;                                      // Bricht ab, wenn noch kein neues Frame fällig ist.
  }
  lastFrameTs = now;                             // Speichert Zeitstempel für dieses Frame.

  sensorOk = readAdxl345(ax, ay, az);            // Liest aktuelle Sensorwerte.
  if (!sensorOk) {                               // Prüft auf Sensorlesefehler.
    Serial.println("FEHLER: ADXL345 Messung ungueltig"); // Gibt Messfehler seriell aus.
  }

  if (gameState == READY) {                      // Prüft, ob Startzustand aktiv ist.
    drawReadyScreen();                           // Rendert den Startbildschirm.
    if (sensorOk && detectFlapFromTilt()) {      // Prüft Startimpuls über Kippbewegung.
      resetGame();                               // Setzt Spielwerte für neue Runde.
      gameState = PLAYING;                       // Wechselt in den Spielzustand.
      beep(1800, 40);                            // Spielt Startsound.
    }
    return;                                      // Verlässt Loop für dieses Frame.
  }

  if (gameState == PLAYING) {                    // Prüft, ob aktiv gespielt wird.
    updateGame();                                // Aktualisiert Logik und Physik.
    drawGameScene();                             // Rendert das aktuelle Spielfeld.

    Serial.print("AZ=");                        // Schreibt Sensorpräfix seriell.
    Serial.print(az);                            // Schreibt Z-Beschleunigungswert.
    Serial.print(" Score=");                    // Schreibt Score-Präfix seriell.
    Serial.println(score);                       // Schreibt aktuellen Score.
    return;                                      // Verlässt Loop für dieses Frame.
  }

  if (gameState == GAME_OVER) {                  // Prüft, ob Game-Over-Zustand aktiv ist.
    drawGameOverScreen();                        // Rendert den Game-Over-Bildschirm.
    if (sensorOk && detectFlapFromTilt()) {      // Prüft Neustartimpuls über Kippbewegung.
      resetGame();                               // Setzt Spielwerte zurück.
      gameState = PLAYING;                       // Startet neue Spielrunde.
      beep(2000, 35);                            // Spielt Neustartsound.
    }
  }
}
