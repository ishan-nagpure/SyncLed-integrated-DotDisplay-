#pragma once
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

extern MD_Parola matrix;

// ─── Pixel shapes (8 rows × up to 32 cols, stored as column bitmasks) ─────────
// Each uint8_t is one column, bit0=top row, bit7=bottom row

// Heart shape (11 cols wide)
static const uint8_t SHAPE_HEART[] = {
  0x0C, 0x1E, 0x3E, 0x7C, 0x3E, 0x1E, 0x3E, 0x7C, 0x3E, 0x1E, 0x0C
};

// Smiley face (11 cols wide)
static const uint8_t SHAPE_SMILEY[] = {
  0x3C, 0x42, 0x95, 0xA1, 0xA1, 0x95, 0x42, 0x3C
};

// Arrow right (8 cols wide)
static const uint8_t SHAPE_ARROW[] = {
  0x08, 0x08, 0x08, 0x7F, 0x3E, 0x1C, 0x08, 0x00
};

// Star (10 cols wide)
static const uint8_t SHAPE_STAR[] = {
  0x08, 0x49, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x49, 0x08, 0x00
};

// Music note (8 cols wide)
static const uint8_t SHAPE_NOTE[] = {
  0x78, 0x40, 0x40, 0x78, 0x0F, 0x08, 0x0F, 0x00
};

// Sun (11 cols wide)
static const uint8_t SHAPE_SUN[] = {
  0x22, 0x14, 0x7F, 0x14, 0x41, 0x14, 0x7F, 0x14, 0x22, 0x00
};

struct Shape {
  const uint8_t* data;
  uint8_t        len;
  const char*    name;
};

static const Shape SHAPES[] = {
  { SHAPE_HEART,  sizeof(SHAPE_HEART),  "Heart"  },
  { SHAPE_SMILEY, sizeof(SHAPE_SMILEY), "Smiley" },
  { SHAPE_ARROW,  sizeof(SHAPE_ARROW),  "Arrow"  },
  { SHAPE_STAR,   sizeof(SHAPE_STAR),   "Star"   },
  { SHAPE_NOTE,   sizeof(SHAPE_NOTE),   "Note"   },
  { SHAPE_SUN,    sizeof(SHAPE_SUN),    "Sun"    },
};
static const int NUM_SHAPES = sizeof(SHAPES) / sizeof(SHAPES[0]);

// ─── Motivational quotes ──────────────────────────────────────────────────────
static const char* QUOTES[] = {
  "Believe you can and you're halfway there.",
  "Every day is a new beginning.",
  "Small steps every day.",
  "You are stronger than you think.",
  "Dream big. Work hard. Stay focused.",
  "Progress not perfection.",
  "Be the energy you want to attract.",
  "One step at a time.",
  "Your only limit is your mind.",
  "Make today count.",
  "Stay positive. Work hard. Make it happen.",
  "The best time to start is now.",
  "Push yourself because no one else will.",
  "Great things never come from comfort zones.",
  "Do something today your future self will thank you for.",
};
static const int NUM_QUOTES = sizeof(QUOTES) / sizeof(QUOTES[0]);

// ─── Display matrix content modes ─────────────────────────────────────────────
// These are the auto-cycling modes shown on the matrix
enum MatrixMode {
  MAT_TIME_TEMP = 0,   // scrolls time + temp + humidity
  MAT_QUOTE,           // motivational quote scroll
  MAT_SHAPE,           // pixel shape animation
  MAT_CUSTOM,          // user-set custom text
  MAT_STOPWATCH,       // live stopwatch display
  MAT_TIMER,           // live countdown timer display
  MAT_CLOCK,           // stable static clock HH:MM updating every second
  MAT_MODE_COUNT
};

// Draw a shape directly to the matrix hardware (not scrolling)
// Uses MD_MAX72XX directly for raw pixel control
void showShape(MD_MAX72XX* mx, const Shape& shape) {
  mx->clear();
  int startCol = 0;
  for (uint8_t c = 0; c < shape.len && startCol < (int)mx->getColumnCount(); c++) {
    mx->setColumn(startCol++, shape.data[c]);
  }
}
