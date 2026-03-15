#include <Adafruit_NeoPixel.h>

#define NUM_DIGITS 3
#define LEDS_PER_SEGMENT 4
#define SEGMENTS_PER_DIGIT 7
#define LEDS_PER_DIGIT (LEDS_PER_SEGMENT * SEGMENTS_PER_DIGIT)
#define NUM_LEDS (NUM_DIGITS * LEDS_PER_DIGIT)
#define DATA_PIN 2

Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Segment naming (standard 7-segment order)
//   --2--
//  |     |
//  3     1
//  |     |
//   --0--
//  |     |
//  4     6
//  |     |
//   --5--
const uint8_t digitPatterns[10][7] = {
// 0, 1, 2, 3, 4, 5, 6
  {0, 1, 1, 1, 1, 1, 1}, // 0
  {0, 1, 0, 0, 0, 0, 1}, // 1
  {1, 1, 1, 0, 1, 1, 0}, // 2
  {1, 1, 1, 0, 0, 1, 1}, // 3
  {1, 1, 0, 1, 0, 0, 1}, // 4
  {1, 0, 1, 1, 0, 1, 1}, // 5
  {1, 0, 1, 1, 1, 1, 1}, // 6
  {0, 1, 1, 0, 0, 0, 6}, // 7
  {1, 1, 1, 1, 1, 1, 1}, // 8
  {1, 1, 1, 1, 0, 1, 1}  // 9
};

// Optional: remap each segment to actual LED order if wiring differs
// Segment 0–6 corresponds to A–G for each digit
// Default mapping: segment * 4 LEDs sequentially
int segmentStartIndex(int digit, int segment) {
  return digit * LEDS_PER_DIGIT + segment * LEDS_PER_SEGMENT;
}

void setup() {
  strip.begin();
  strip.show();
}

void loop() {
  for (int i = 0; i < 1000; i++) {
    displayNumber(i);
    delay(500);
  }
}

void displayNumber(int number) {
  clearDisplay();

  int hundreds = (number / 100) % 10;
  int tens     = (number / 10)  % 10;
  int ones     = number % 10;

  displayDigit(hundreds, 0); // left digit
  displayDigit(tens, 1);     // middle digit
  displayDigit(ones, 2);     // right digit

  strip.show();
}

void displayDigit(int digitValue, int digitIndex) {
  if (digitValue < 0 || digitValue > 9) return;

  for (int seg = 0; seg < 7; seg++) {
    if (digitPatterns[digitValue][seg]) {
      turnOnSegment(digitIndex, seg);
    }
  }
}

void turnOnSegment(int digitIndex, int segment) {
  int startIdx = segmentStartIndex(digitIndex, segment);
  for (int i = 0; i < LEDS_PER_SEGMENT; i++) {
    strip.setPixelColor(startIdx + i, strip.Color(255, 255, 255)); // white
  }
}

void clearDisplay() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
}
