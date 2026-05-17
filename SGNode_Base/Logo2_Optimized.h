// Logo2_Optimized.h - Optimized boot logo for SGNode Base Station
// This file contains a space-optimized version of the logo that stores only non-white pixels
// 
// Optimization results:
// - Original: 56,100 pixels (255x220) in full RGB565 array
// - Optimized: 18,006 non-white pixels in coordinate format
// - Size reduction: 67.9%
// - Memory savings: ~76KB

#ifndef LOGO2_OPTIMIZED_H
#define LOGO2_OPTIMIZED_H

#include <stdint.h>

// Pixel structure for coordinate-based storage
typedef struct {
    uint8_t x;
    uint8_t y;
    uint16_t color;
} Pixel;

// Logo dimensions
const uint16_t logo2_width = 255;
const uint16_t logo2_height = 220;

// Include the optimized pixel data
#include "Logo2_Optimized_Data_Cleaned.h"


#endif // LOGO2_OPTIMIZED_H
