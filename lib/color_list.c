#include "color_list.h"

#include <string.h>

void color_list_init(color_list_t *list)
{
    list->length = 0;
}

color_list_result_t
color_list_push(color_list_t *list, const color_list_item_t *item)
{
    if (list->length == color_list_max_length)
        return color_list_error;

    memcpy(list->items + list->length, item, sizeof(color_list_item_t));
    (list->length)++;

    return color_list_success;
}

color_list_item_t* color_list_find_by_name(color_list_t *list, const char *name)
{
    uint32_t i;

    for (i = 0; i < list->length; i++)
    {
        if (0 == strcmp(name, list->items[i].name))
        {
            return list->items + i;
        }
    }

    return NULL;
}
