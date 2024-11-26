#ifndef COLORSPACES_H
#define COLORSPACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum { max_hue = 360 };
enum { max_saturation = 100 };
enum { max_brightness = 100 };

#define MIN3(x,y,z)  ((y) <= (z) ? \
                          ((x) <= (y) ? (x) : (y)) \
                      : \
                          ((x) <= (z) ? (x) : (z)))
 
#define MAX3(x,y,z)  ((y) >= (z) ? \
                         ((x) >= (y) ? (x) : (y)) \
                     : \
                         ((x) >= (z) ? (x) : (z)))
 
struct rgb_color {
    float r, g, b;    // 0.0 and 1.0
};
 
struct hsv_color {
    float hue;        // 0.0 and 360.0 
    float sat;        // 0.0 (gray) and 1.0
    float val;        // 0.0 (black) and 1.0 
};

typedef struct {
    uint16_t h;
    uint8_t s;
    uint8_t v;
} hsv_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

inline hsv_t hsv(uint16_t h, uint8_t s, uint8_t v)
{
    return (hsv_t) {h, s, v};
}

inline rgb_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (rgb_t) {r, g, b};
}

inline rgb_t rgb_float(float r, float g, float b)
{
    return rgb((uint8_t) r, (uint8_t) g, (uint8_t) b);
}

rgb_t hsv2rgb(hsv_t hsv);
hsv_t rgb2hsv(rgb_t rgb);

#ifdef __cplusplus
}
#endif

#endif /* COLORSPACES_H */
