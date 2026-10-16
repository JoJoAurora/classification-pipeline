#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h>
#include "model.h"

Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

const int UV_PIN = 34;
const int SDA_PIN = 21;
const int SCL_PIN = 22;

// Must match the logging sketch exactly
const float ALPHA = 0.2f;
const unsigned long SAMPLE_INTERVAL_MS = 100;

float uvFiltered = -1;
float irFiltered = -1;
unsigned long lastSampleTime = 0;
unsigned long sampleIndex = 0;

// Returns class index, or -1 for UNKNOWN. Also reports the cell and winning value.
int classify(float uv, float ir, int &row, int &col, float &bestVal) {
  // Same scaling as training: UV raw, IR through log1p, both min-max with the shipped bounds
  float x = (uv - SCALE_BOUNDS[0]) / (SCALE_BOUNDS[1] - SCALE_BOUNDS[0]);
  float y = (log1pf(ir) - SCALE_BOUNDS[2]) / (SCALE_BOUNDS[3] - SCALE_BOUNDS[2]);

  col = constrain((int)(x * GRID_N), 0, GRID_N - 1);   // col = scaled UV
  row = constrain((int)(y * GRID_N), 0, GRID_N - 1);   // row = scaled log(IR)

  int best = -1;
  bestVal = -1.0f;
  for (int c = 0; c < NUM_CLASSES; c++) {
    float v = pgm_read_byte(&CLASS_TABLE[c][row][col]) / 255.0f;
    if (v > bestVal) { bestVal = v; best = c; }
  }
  return (bestVal < REJECT_TAU) ? -1 : best;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!tsl.begin()) {
    Serial.println("# ERROR: TSL2591 not found.");
    while (1) delay(1000);
  }
  tsl.setGain(TSL2591_GAIN_MED);                       // same as logging
  tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  Serial.println("idx,uv,ir,row,col,class,latency_us");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
  lastSampleTime = now;

  int rawUV = analogRead(UV_PIN);
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t rawIR = lum >> 16;

  if (uvFiltered < 0) { uvFiltered = rawUV; irFiltered = rawIR; }
  else {
    uvFiltered = ALPHA * rawUV + (1 - ALPHA) * uvFiltered;
    irFiltered = ALPHA * rawIR + (1 - ALPHA) * irFiltered;
  }

  int row, col;
  float val;
  unsigned long t0 = micros();
  int cls = classify(uvFiltered, irFiltered, row, col, val);   // lookup only, no sensor read
  unsigned long dt = micros() - t0;

  Serial.print(sampleIndex++); Serial.print(",");
  Serial.print(uvFiltered, 2); Serial.print(",");
  Serial.print(irFiltered, 2); Serial.print(",");
  Serial.print(row); Serial.print(",");
  Serial.print(col); Serial.print(",");
  Serial.print(cls < 0 ? "UNKNOWN" : CLASS_NAMES[cls]); Serial.print(",");
  Serial.println(dt);
}
