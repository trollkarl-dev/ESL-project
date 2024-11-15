#ifndef COLORSPACES_H
#define COLORSPACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum { max_hue = 360 };
enum { max_saturation = 100 };
enum { max_brightness = 100 };

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

#ifdef __cplusplus
}
#endif

#endif /* COLORSPACES_H */
