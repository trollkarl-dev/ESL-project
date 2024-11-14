#include "colorspaces.h"

rgb_t hsv2rgb(hsv_t hsv)
{
    enum { sixth_hue = max_hue / 6 };
    const int sector = (hsv.h / sixth_hue) % 6;
    const int vmin = (((hsv.v * (max_pct - hsv.s)) << 8) / max_pct) >> 8;
    const int a = ((((hsv.v - vmin) * (hsv.h % sixth_hue)) << 8) / sixth_hue) >> 8;
    const int vinc = vmin + a;
    const int vdec = hsv.v - a;

    switch (sector)
    {
        case 0:  return (rgb_t) {hsv.v, vinc, vmin}; break;
        case 1:  return (rgb_t) {vdec, hsv.v, vmin}; break;
        case 2:  return (rgb_t) {vmin, hsv.v, vinc}; break;
        case 3:  return (rgb_t) {vmin, vdec, hsv.v}; break;
        case 4:  return (rgb_t) {vinc, vmin, hsv.v}; break;
        case 5:
        default: return (rgb_t) {hsv.v, vmin, vdec}; break;
    }
}
