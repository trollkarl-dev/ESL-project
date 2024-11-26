#include "colorspaces.h"

rgb_t hsv2rgb(hsv_t hsv)
{
    enum { sixth_hue = max_hue / 6 };
    const int sector = (hsv.h / sixth_hue) % 6;

    float vmin = (float) hsv.v *
                 (float) (max_saturation - hsv.s) /
                 (float) max_brightness;

    float a = ((float) hsv.v - vmin) *
              (float) (hsv.h % sixth_hue) /
              (float) sixth_hue;

    float vinc = vmin + a;
    float vdec = hsv.v - a;

    switch (sector)
    {
        case 0:  return rgb_float(hsv.v, vinc, vmin); break;
        case 1:  return rgb_float(vdec, hsv.v, vmin); break;
        case 2:  return rgb_float(vmin, hsv.v, vinc); break;
        case 3:  return rgb_float(vmin, vdec, hsv.v); break;
        case 4:  return rgb_float(vinc, vmin, hsv.v); break;
        case 5:
        default: return rgb_float(hsv.v, vmin, vdec); break;
    }
}

static struct hsv_color rgb_to_hsv(struct rgb_color rgb) {
 
    struct hsv_color hsv;
    float rgb_min, rgb_max;
    rgb_min = MIN3(rgb.r, rgb.g, rgb.b);
    rgb_max = MAX3(rgb.r, rgb.g, rgb.b);
    hsv.val = rgb_max;
    if (hsv.val == 0) {
        hsv.hue = hsv.sat = 0;
        return hsv;
    }
 
    rgb.r /= hsv.val;
    rgb.g /= hsv.val;
    rgb.b /= hsv.val;
    rgb_min = MIN3(rgb.r, rgb.g, rgb.b);
    rgb_max = MAX3(rgb.r, rgb.g, rgb.b);
    hsv.sat = rgb_max - rgb_min;
    if (hsv.sat == 0) {
        hsv.hue = 0;
        return hsv;
    }
    
    rgb.r = (rgb.r - rgb_min)/(rgb_max - rgb_min);
    rgb.g = (rgb.g - rgb_min)/(rgb_max - rgb_min);
    rgb.b = (rgb.b - rgb_min)/(rgb_max - rgb_min);
    rgb_min = MIN3(rgb.r, rgb.g, rgb.b);
    rgb_max = MAX3(rgb.r, rgb.g, rgb.b);
 
    if (rgb_max == rgb.r) {
        hsv.hue = 0.0 + 60.0*(rgb.g - rgb.b);
        if (hsv.hue < 0.0) {
            hsv.hue += 360.0;
        }
    } else if (rgb_max == rgb.g) {
        hsv.hue = 120.0 + 60.0*(rgb.b - rgb.r);
    } else /* rgb_max == rgb.b */ {
        hsv.hue = 240.0 + 60.0*(rgb.r - rgb.g);
    }
    return hsv;
}

hsv_t rgb2hsv(rgb_t color)
{
    struct rgb_color c_src;
    struct hsv_color c_dst;
    hsv_t result;

    c_src.r = (float) color.r / 100;
    c_src.g = (float) color.g / 100;
    c_src.b = (float) color.b / 100;

    c_dst = rgb_to_hsv(c_src);

    result.h = (uint16_t) c_dst.hue;
    result.s = (uint8_t) (c_dst.sat * max_saturation);
    result.v = (uint8_t) (c_dst.val * max_brightness);

    return result;
}
