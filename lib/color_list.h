#ifndef COLOR_LIST_H
#define COLOR_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "colorspaces.h"
#include <stdbool.h>

#include "app_util.h"

enum { color_list_max_length = 10 };
enum { color_list_max_name_length = 16 };

typedef enum {
    color_list_success,
    color_list_error
} color_list_result_t;

typedef struct {
    hsv_t color;
    char name[color_list_max_name_length];
} color_list_item_t;

typedef struct {
    uint16_t length;
    color_list_item_t items[color_list_max_length];
    bool used[color_list_max_length];
} color_list_t;

STATIC_ASSERT(sizeof(color_list_t) == 212, "Bad size!");

void color_list_init(color_list_t *list);
color_list_result_t color_list_push(color_list_t *list, const color_list_item_t *item);
color_list_result_t color_list_delete(color_list_t *list, const char *name);
color_list_item_t* color_list_find_by_name(color_list_t *list, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* COLOR_LIST_H */
