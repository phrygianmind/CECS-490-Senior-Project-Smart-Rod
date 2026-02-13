#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <math.h>
#include <LiquidCrystal.h>

// ESP32 I2C pins for TCS34725
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;

Adafruit_TCS34725 tcs(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// LCD1602 in 4-bit parallel mode (no I2C backpack)
// RS, E, D4, D5, D6, D7
const int LCD_RS = 14;
const int LCD_E  = 27;
const int LCD_D4 = 26;
const int LCD_D5 = 25;
const int LCD_D6 = 33;
const int LCD_D7 = 32;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Rates
const unsigned long sampleMs = 20;
const unsigned long printMs  = 200;

// Debounce 
const unsigned long lockoutMs = 120;

// Spool parameters 
const float R_core_m  = 0.02413f;
const float R_full_m  = 0.03048f;
const float spoolW_m  = 0.02413f;
const float lineDia_m = 0.00030f;

// Optional calibration
float calib = 1.00f;

// STATE 
int count = 0;
unsigned long lastCountMs  = 0;
unsigned long lastSampleMs = 0;
unsigned long lastPrintMs  = 0;

float L_out_m = 0.0f;
float L_rem_m = 0.0f;
float R_eff_m = 0.0f;
float alpha   = 0.0f;

// Detect BLUE: b clearly larger than r and g
bool isBlueMark(uint16_t r, uint16_t g, uint16_t b) {
  return (b > (uint16_t)(r * 1.5f)) && (b > (uint16_t)(g * 1.5f));
}

// Initial line length based on geometry
float computeInitialLineLength() {
  const float A = (float)M_PI * (lineDia_m * 0.5f) * (lineDia_m * 0.5f);
  return (spoolW_m * (float)M_PI * (R_full_m * R_full_m - R_core_m * R_core_m)) / A;
}

float radiusFromRemainingLine(float Lrem) {
  float val = R_core_m * R_core_m + alpha * Lrem;
  if (val < R_core_m * R_core_m) val = R_core_m * R_core_m;
  return sqrtf(val);
}

void onRevolution() {
  float dL = calib * (2.0f * (float)M_PI * R_eff_m);
  L_out_m += dL;

  L_rem_m -= dL;
  if (L_rem_m < 0.0f) L_rem_m = 0.0f;

  R_eff_m = radiusFromRemainingLine(L_rem_m);
}

void resetModel() {
  count = 0;
  L_out_m = 0.0f;
  L_rem_m = computeInitialLineLength();
  R_eff_m = radiusFromRemainingLine(L_rem_m);
  lastCountMs = millis();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Reset");
  lcd.setCursor(0, 1); lcd.print("...");
}

void setup() {
  Serial.begin(115200);

  // I2C for color sensor
  Wire.begin(I2C_SDA, I2C_SCL);

  // LCD init
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Smart Rod");
  lcd.setCursor(0, 1); lcd.print("Init...");

  // Color sensor init
  if (!tcs.begin()) {
    Serial.println("ERROR: TCS34725 not found. Check VIN/GND/SDA/SCL.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("TCS34725");
    lcd.setCursor(0, 1); lcd.print("NOT FOUND");
    while (true) delay(100);
  }

  alpha = (lineDia_m * lineDia_m) / (4.0f * spoolW_m);

  L_rem_m = computeInitialLineLength();
  R_eff_m = radiusFromRemainingLine(L_rem_m);
  lastCountMs = millis();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Ready");
  lcd.setCursor(0, 1); lcd.print("Counting...");

  Serial.println("Live RGB + count + distance + LCD1602 (parallel)");
  Serial.println("Format: R,G,B | count | R_eff(mm) | L_out(m)");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs < sampleMs) return;
  lastSampleMs = now;

  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // Count revolutions on BLUE mark
  if (isBlueMark(r, g, b)) {
    if (now - lastCountMs >= lockoutMs) {
      lastCountMs = now;
      count++;
      onRevolution();
    }
  }

  // Update Serial + LCD
  if (now - lastPrintMs >= printMs) {
    lastPrintMs = now;

    // Serial
    Serial.print(r); Serial.print(",");
    Serial.print(g); Serial.print(",");
    Serial.print(b);
    Serial.print(" | ");
    Serial.print(count);
    Serial.print(" | R_eff(mm): ");
    Serial.print(R_eff_m * 1000.0f, 2);
    Serial.print(" | L_out(m): ");
    Serial.println(L_out_m, 3);

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("Cnt:");
    lcd.print(count);
    lcd.print(" R:");
    lcd.print(R_eff_m * 1000.0f, 1);
    lcd.print("   "); // clear trailing chars

    lcd.setCursor(0, 1);
    lcd.print("Dist:");
    lcd.print(L_out_m, 1);
    lcd.print(" m     "); // clear trailing chars
  }

  // Reset via serial 'r'
  if (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == 'r' || ch == 'R') {
      resetModel();
      Serial.println("Reset.");
    }
  }
}
