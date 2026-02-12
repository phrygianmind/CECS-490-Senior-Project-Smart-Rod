#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <math.h>

// ESP32 I2C pins (common default)
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;

Adafruit_TCS34725 tcs(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// Rates
const unsigned long sampleMs = 20;   // sensor read rate
const unsigned long printMs  = 100;  // serial update rate

// Debounce/lockout after a count
const unsigned long lockoutMs = 120; // tune based on rotation speed

// ------------------- YOUR MEASURED PARAMETERS (meters) -------------------
// You provided diameters; we converted to radii and meters.
const float R_core_m  = 0.02413f;  // 1.9 in diameter -> 0.95 in radius
const float R_full_m  = 0.03048f;  // 2.4 in diameter -> 1.2  in radius
const float spoolW_m  = 0.02413f;  // 0.95 in line-lay width
const float lineDia_m = 0.00030f;  // 0.30 mm

// Optional calibration factor (multiply per-rev distance by this)
float calib = 1.00f;

// ------------------- STATE -------------------
int count = 0;
unsigned long lastCountMs  = 0;
unsigned long lastSampleMs = 0;
unsigned long lastPrintMs  = 0;

// Continuous “calculus/volume” model state
float L_out_m = 0.0f;   // total retrieved distance (meters)
float L_rem_m = 0.0f;   // remaining line on spool (meters)
float R_eff_m = 0.0f;   // effective radius (meters)
float alpha   = 0.0f;   // alpha = A/(pi*W) = d^2/(4W)

// Red condition: r is strictly the largest channel (you use a stronger check below)
bool redNowFromRGB(uint16_t r, uint16_t g, uint16_t b) {
  return (r > g) && (r > b);
}

// Initial line length based on geometry:
// A*L0 = W*pi*(R_full^2 - R_core^2) => L0 = W*pi*(R_full^2 - R_core^2)/A
float computeInitialLineLength() {
  const float A = (float)M_PI * (lineDia_m * 0.5f) * (lineDia_m * 0.5f);
  return (spoolW_m * (float)M_PI * (R_full_m * R_full_m - R_core_m * R_core_m)) / A;
}

// Radius from remaining line: R = sqrt(R_core^2 + alpha*Lrem)
float radiusFromRemainingLine(float Lrem) {
  float val = R_core_m * R_core_m + alpha * Lrem;
  if (val < R_core_m * R_core_m) val = R_core_m * R_core_m;
  return sqrtf(val);
}

// Update distance and radius for one revolution
void onRevolution() {
  // dL = 2*pi*R_eff
  float dL = calib * (2.0f * (float)M_PI * R_eff_m);

  L_out_m += dL;
  L_rem_m -= dL;
  if (L_rem_m < 0.0f) L_rem_m = 0.0f;

  R_eff_m = radiusFromRemainingLine(L_rem_m);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!tcs.begin()) {
    Serial.println("ERROR: TCS34725 not found. Check VIN/GND/SDA/SCL.");
    while (true) delay(100);
  }

  // alpha = d^2/(4W) because A/(pi*W) = [pi*(d/2)^2]/(pi*W)
  alpha = (lineDia_m * lineDia_m) / (4.0f * spoolW_m);

  // Initialize model assuming you start "full" at R_full_m
  L_out_m = 0.0f;
  L_rem_m = computeInitialLineLength();
  R_eff_m = radiusFromRemainingLine(L_rem_m);

  lastCountMs = millis();

  Serial.println("Live RGB printing + count + retrieved distance (continuous radius model).");
  Serial.println("Format: R,G,B | count | R_eff(mm) | L_out(m)");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs < sampleMs) return;
  lastSampleMs = now;

  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c); // c read but not displayed

  bool redNow = redNowFromRGB(r, g, b);
  (void)redNow; // not required, kept since it was in your sketch

  // increment count if red is clearly larger than green and blue values
  if ((r > (uint16_t)(g * 1.5f)) && (r > (uint16_t)(b * 1.5f))) {
    if (now - lastCountMs >= lockoutMs) {
      lastCountMs = now;
      count++;
      onRevolution(); // <-- add calculus-based distance update
      // delay(200);   // removed; lockout already handles this
    }
  }

  // Live print at controlled rate
  if (now - lastPrintMs >= printMs) {
    lastPrintMs = now;

    Serial.print(r);
    Serial.print(",");
    Serial.print(g);
    Serial.print(",");
    Serial.print(b);

    Serial.print(" | ");
    Serial.print(count);

    Serial.print(" | R_eff(mm): ");
    Serial.print(R_eff_m * 1000.0f, 2);

    Serial.print(" | L_out(m): ");
    Serial.println(L_out_m, 3);
  }

  // Optional reset: type 'r' in Serial Monitor
  if (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == 'r' || ch == 'R') {
      count = 0;
      L_out_m = 0.0f;
      L_rem_m = computeInitialLineLength();
      R_eff_m = radiusFromRemainingLine(L_rem_m);
      lastCountMs = millis();
      Serial.println("Reset.");
    }
  }
}
