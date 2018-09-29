#ifndef __BITMAP_ICON__
#define __BITMAP_ICON__

#include <stdint.h>

// Bitmap Icon
typedef struct {
    uint8_t width;              // Icon width
    uint8_t height;             // Icon height
    uint8_t bpp;                // Bits per pixel
    uint8_t *bitmap;            // Bitmap
} BitmapIcon;

#endif // __BITMAP_ICON__
