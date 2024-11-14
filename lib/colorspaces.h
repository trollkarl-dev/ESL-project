#ifndef COLORSPACES_H
#define COLORSPACES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum { max_hue = 360 };
enum { max_pct = 100 };
enum { max_saturation = max_pct };
enum { max_brightness = max_pct };

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

rgb_t hsv2rgb(hsv_t hsv);

#ifdef __cplusplus
}
#endif

#endif /* COLORSPACES_H */
