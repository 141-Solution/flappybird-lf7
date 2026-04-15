systempromt:
- erstelle ein arduino programm main.ino für die folgenden Komponenten und anweisungen 
- bei fehlenden infos ergänze die frage in dieser datei
- short answers
- senior developer
- kommentiere jede Zeile auf deutsch
- soll das spielen von flappybird möglich machen



1. Komponentenübersicht
• D1 mini (ESP8266)
• OLED Shield (0,66", SSD1306)
- adxl345 beschleunigungssensor
- buzzer shield 
2. Schnittstellen / Kommunikation
• OLED:
- Kommunikationsschnittstelle:	I2C
- I2C Adresse:				0x3C/0x3D
- adxl345: I2C
- I2C Adresse:				0x53 


3. Relevante technische Daten
OLED:
• OLED-Auflösung: 0.66" 64x48
• OLED-Treiber-IC: SSD1306


4. Funktionsanforderungen an den Prototyp
• fortlaufende Messung
• Anzeige auf OLED
• serielle Debug-Ausgabe (optional/empfohlen)
• Fehlerausgabe bei ungültiger Messung
5. Darstellungsanforderungen (Display)


5. libs:
- Adafruit SSD1306 Wemos Mini OLED by Adafruit
- Adafruit GFX Library by Adafruit

6. Offene Fragen (bitte ergänzen)
- Welcher GPIO-Pin ist für den Buzzer vorgesehen (z. B. D5/GPIO14)?
D5
- Ist der Buzzer aktiv HIGH oder aktiv LOW?
active low
- Soll Kippen nach oben (ADXL345 Y+) oder nach unten (ADXL345 Y-) den Flap auslösen?
z richtung also hoxh und runter
- Soll es einen Startbildschirm mit "Tilt to Start" geben (ja/nein)?
ja bitte
- Soll der Highscore nur im RAM (bis Reset) oder persistent (EEPROM) gespeichert werden?
ram
