#include <Wire.h>
#include <Adafruit_BNO08x.h>

#define BNO08X_SDA 21
#define BNO08X_SCL 22
#define BNO08X_INT 19  
#define BNO08X_RST 18  

Adafruit_BNO08x bno08x(BNO08X_RST);
sh2_SensorValue_t sensorValue;

// --- KINEMATICS & PHYSICS ---
float speedX = 0.0;
unsigned long lastMicros = 0;
const float DRIFT_THRESHOLD = 0.15; 
const float DECAY = 0.95;           

// --- SENIOR PROJECT PARAMETERS ---
const float MASS_KG = 0.05;         // 50 grams
const float FORCE_BITE_THRESHOLD = 0.25; 
const float MAX_FORCE_EXPECTED = 2.0;    // Calibrate bar length (2 Newtons = Full Bar)

// --- DISPLAY SETTINGS ---
unsigned long printInterval = 100;   // Updated to 100ms for a smoother visual bar
unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(BNO08X_SDA, BNO08X_SCL);

  if (!bno08x.begin_I2C(0x4A, &Wire, BNO08X_INT)) {
    if (!bno08x.begin_I2C(0x4B, &Wire, BNO08X_INT)) {
      Serial.println("Hardware failure.");
      while (1) delay(10);
    }
  }

  bno08x.enableReport(SH2_LINEAR_ACCELERATION, 10000); 
  lastMicros = micros();
}

void loop() {
  if (bno08x.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_LINEAR_ACCELERATION) {
      
      unsigned long currentMicros = micros();
      float dt = (currentMicros - lastMicros) / 1000000.0;
      lastMicros = currentMicros;

      float ax = sensorValue.un.linearAcceleration.x;
      float ay = sensorValue.un.linearAcceleration.y;
      float az = sensorValue.un.linearAcceleration.z;
      
      float totalAccel = sqrt(ax*ax + ay*ay + az*az);
      float forceNewtons = MASS_KG * totalAccel;

      // SPEED CALCULATION
      if (abs(ax) > DRIFT_THRESHOLD) {
        speedX += ax * dt; 
      } else {
        speedX *= DECAY; 
        if (abs(speedX) < 0.01) speedX = 0; 
      }

      // DELAYED PRINTING FOR THE VISUAL BAR
      if (millis() - lastPrintTime > printInterval) {
        lastPrintTime = millis();

        // 1. Calculate bar length
        int barLength = map(constrain(forceNewtons * 100, 0, MAX_FORCE_EXPECTED * 100), 0, MAX_FORCE_EXPECTED * 100, 0, 30);

        // 2. Print data line
        Serial.print("Spd: "); Serial.print(abs(speedX), 1);
        Serial.print(" m/s | F: "); Serial.print(forceNewtons, 2);
        Serial.print(" N [");

        // 3. Print the visual bar
        for (int i = 0; i < 30; i++) {
          if (i < barLength) Serial.print("=");
          else Serial.print(" ");
        }
        Serial.print("]");

        // 4. Strike Alert Label
        if (forceNewtons > FORCE_BITE_THRESHOLD) {
          Serial.print(" <<< STRIKE!");
        }
        Serial.println();
      }
    }
  }
}
