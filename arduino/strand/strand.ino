// Driver for RGBW Light/Ambient Display

// For Feather M4 Express and Adafruit NeoPXL8 Featherwing 

#include <Adafruit_NeoPXL8.h>

#define NUM_LED    60       // Per strand. Total number of pixels is 8X this!
#define BRIGHTNESS 255      // Overall scale factor in interval [0,255]
#define LED_ORDER  NEO_GRBW // Data order for LED strands

#define MIN_MODE  1         // min mode
#define MAX_MODE  1         // max mode
int8_t mode = MIN_MODE;     // current mode

void
next_mode() {
  mode++;
  if (mode > MAX_MODE) mode = MIN_MODE;
}

// These are the default pins for the NeoPXL8 Featherwing
int8_t pins[8] = { 13, 12, 11, 10, SCK, 5, 9, 6 };
Adafruit_NeoPXL8 leds(NUM_LED, pins, LED_ORDER);

void setup() {
  leds.begin();
  leds.setBrightness(BRIGHTNESS);
  init_sparkles();
}

#define NUM_FRAMES 512
uint16_t frame = 0;  // animation frame
uint8_t cur_c = 0;  // current color index
#define MODE_CYCLE 8
uint8_t mode_frame = 0;

// Color palette, RGBW
#define NUM_COLORS 9
uint8_t colors[NUM_COLORS][4] = { 
  255,   0,   0,   0, // Red
  255,   0,   0, 128, // Pink
  255, 160,   0,   0, // Orange
  255, 255,   0,   0, // Yellow
    0, 255,   0,   0, // Green
    0, 255, 255,   0, // Cyan
    0,   0, 255,   0, // Blue
  192,   0, 255,   0, // Purple
  255,   0, 255,   0  // Magenta
};


void loop() {
  switch (mode) {
    case 0: {
      // plain white, no RGB
      for (uint16_t i=0; i<8*NUM_LED; i++) {
        leds.setPixelColor(i, 0,0,0,255);
      }
    } break;
    case 1: {
      // bright white, W + full RGB white
      for (uint16_t i=0; i<8*NUM_LED; i++) {
        leds.setPixelColor(i, 255,255,255,255);
      }
    } break;
    case 2: {
      // rain
      for(uint8_t r=0; r<8; r++) { // For each strand...
        for(int p=0; p<NUM_LED; p++) { // For each pixel of each strand...
          leds.setPixelColor(r * NUM_LED + p, rain(cur_c, p));
        }
      }
    } break;
    case 3: {
      // rotating gradient
      if (0 == frame) {
        uint8_t a = cur_c;
        uint8_t b = (a + 1) % NUM_COLORS;
        grad_buf(colors[a][0],colors[a][1],colors[a][2],colors[a][3],
                 colors[b][0],colors[b][1],colors[b][2],colors[b][3]);
      } else {
        shift_buf(1);
      }
      set_buf();
    } break;
    case 4: {
      // random sparkles on white background
      clear_buf(0,0,0,255);
      set_sparkles();
      set_buf();
      adv_sparkles();
    } break;
    default: {
      mode = 0;
      break;
    }
  }
  
  leds.show();
  delay(10);
  
  frame++;
  if (NUM_FRAMES == frame) {
    frame = 0;
    cur_c = (cur_c + 1) % NUM_COLORS;
    mode_frame++;
    if (MODE_CYCLE == mode_frame) {
      mode_frame = 0;
      next_mode();
    }
  }
}


// Gamma-correction table improves the appearance of midrange colors
#define _GAMMA_ 1.8
const int _GBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _G1_ (uint8_t)(pow((__COUNTER__ - _GBASE_) / 255.0, _GAMMA_) * 255.0 + 0.5),
#define _G2_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ // Expands to 8 lines
#define _G3_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ // Expands to 64 lines
const uint8_t gamma8[] = { _G3_ _G3_ _G3_ _G3_ };    // 256 lines

// Given color number and pixel number along strand (0 - (NUM_LED-1)),
// first calculate brightness (b) of pixel, then multiply color by
// this and run it through gamma-correction table.
uint32_t rain(uint8_t s, int p) {
  uint16_t b = 256 - ((frame + p * 256 / NUM_LED) & 0xFF);
  uint32_t c = ((uint32_t)gamma8[(colors[s][0] * b) >> 8] << 16) | // red
               ((uint32_t)gamma8[(colors[s][1] * b) >> 8] <<  8) | // green
                (uint32_t)gamma8[(colors[s][2] * b) >> 8] |        // blue 
                (uint32_t)gamma8[(colors[s][3] * b) >> 8] << 24;   // white 
  return c;
}

// buffer for computing next frame
uint8_t buf[8][NUM_LED][4];
int16_t buf_shift = 0;

// set pixels to current state of buffer, including current shift
void
set_buf() {
  int16_t pshift = buf_shift;
  while (pshift < 0) pshift += NUM_LED;
  for (uint8_t s = 0; s < 8; s++) {
    for (uint16_t i = 0; i < NUM_LED; i++) {
      uint16_t j = (i + pshift);
      while (j > NUM_LED) j -= NUM_LED;
      uint16_t k = s*NUM_LED + j;
      leds.setPixelColor(k, buf[s][i][0], buf[s][i][1], buf[s][i][2], buf[s][i][3]);
    }
  }
}

// load buffer with a single color (gamma corrected)
void
clear_buf(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  for (uint8_t s = 0; s < 8; s++) {
    for (uint16_t i = 0; i < NUM_LED; i++) {
      buf[s][i][0] = gamma8[r];
      buf[s][i][1] = gamma8[g];
      buf[s][i][2] = gamma8[b];
      buf[s][i][3] = gamma8[w];
    }
  }
}

// shift buffer (shift can be positive or negative, rotary shift
void shift_buf(int8_t sh) {
  buf_shift += sh;
}

// load buffer with a gradient between two colors (gamma corrected)
void
grad_buf(
  uint8_t r0, uint8_t g0, uint8_t b0, uint8_t w0,
  uint8_t r1, uint8_t g1, uint8_t b1, uint8_t w1
) {
  for (uint8_t s = 0; s < 8; s++) {
    for (uint16_t i = 0; i < NUM_LED; i++) {
      uint16_t a1 = ((i * 256 / NUM_LED) & 0xFF);
      uint16_t a0 = 256 - a1;
      buf[s][i][0] = gamma8[((r0 * a0 + r1 * a1) >> 8) & 0xFF];
      buf[s][i][1] = gamma8[((g0 * a0 + g1 * a1) >> 8) & 0xFF];
      buf[s][i][2] = gamma8[((b0 * a0 + b1 * a1) >> 8) & 0xFF];
      buf[s][i][3] = gamma8[((w0 * a0 + w1 * a1) >> 8) & 0xFF];
    }
  }
}

#define NUM_SPARKLES 32
uint16_t sparkle_p[NUM_SPARKLES];
uint8_t sparkle_c[NUM_SPARKLES];
uint8_t sparkle_b[NUM_SPARKLES];

void
init_sparkles() {
  for (uint8_t i = 0; i < NUM_SPARKLES; i++) {
    uint32_t j = i;
    sparkle_p[i] = (j*1793 + 88721) % (8*NUM_LED);
    sparkle_c[i] = j % NUM_COLORS;
    sparkle_b[i] = (uint8_t)((uint16_t)i*256/NUM_SPARKLES) % 256;
  }
}

void
set_sparkles() {
  for (uint8_t i = 0; i < NUM_SPARKLES; i++) {
    uint16_t p = sparkle_p[i];
    uint8_t s = p / NUM_LED;
    uint16_t j = p % NUM_LED;
    uint8_t b = sparkle_b[i];
    uint8_t c = sparkle_c[i];
    buf[s][j][0] = gamma8[(b*colors[c][0] >> 8) & 0xFF];
    buf[s][j][1] = gamma8[(b*colors[c][1] >> 8) & 0xFF];
    buf[s][j][2] = gamma8[(b*colors[c][2] >> 8) & 0xFF];
    buf[s][j][3] = gamma8[(b*colors[c][3] >> 8) & 0xFF];
  }
}

void
adv_sparkles() {
  for (uint8_t i = 0; i < NUM_SPARKLES; i++) {
    sparkle_b[i]++;
    if (0 == sparkle_b[i]) {
      sparkle_p[i] = (sparkle_p[i]*1793 + 88721) % (8*NUM_LED);
      sparkle_c[i] = (sparkle_c[i] + 1) % NUM_COLORS;
      sparkle_b[i] = 0;
    }
  }
}
