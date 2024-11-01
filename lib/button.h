#ifndef BUTTON_H
#define BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

enum {
    b_shift_reg_top  = 0xFF,
    b_shift_reg_down = 0x00
};

struct button {
    uint8_t shift_reg;
    bool pressed_flag;

    bool (*is_pressed)(void);

    void (*pressed_callback)(void);
    void (*released_callback)(void);
};

void button_init(struct button *);
void button_check(struct button *);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
