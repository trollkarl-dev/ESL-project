#include "colorspaces.h"

rgb_t hsv2rgb(hsv_t hsv)
{
    enum { sixth_hue = max_hue / 6 };
    const int sector = (hsv.h / sixth_hue) % 6;

    float vmin = (float) hsv.v *
                 (float) (max_pct - hsv.s) /
		 (float) max_pct;
    float a = ((float)hsv.v - vmin) *
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
