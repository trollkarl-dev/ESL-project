#include "color_list.h"

#include <string.h>

void color_list_init(color_list_t *list)
{
    int i;

    list->length = 0;

    for (i = 0; i < color_list_max_length; i++)
    {
        list->used[i] = false;
    }
}

color_list_result_t
color_list_push(color_list_t *list, const color_list_item_t *item)
{
    uint32_t i;

    for (i = 0; i < color_list_max_length; i++)
    {
        if (!(list->used[i]))
        {
            memcpy(list->items + i, item, sizeof(color_list_item_t));
            list->used[i] = true;

            return color_list_success;
        }
    }

    return color_list_error;
}

color_list_item_t*
color_list_find_by_name(color_list_t *list, const char *name)
{
    uint32_t i;

    for (i = 0; i < color_list_max_length; i++)
    {
        if (list->used[i] && (0 == strcmp(name, list->items[i].name)))
        {
            return list->items + i;
        }
    }

    return NULL;
}

color_list_result_t color_list_delete(color_list_t *list, const char *name)
{
    color_list_item_t *color_ptr = color_list_find_by_name(list, name);

    if (NULL == color_ptr)
    {
        return color_list_error;
    }

    bool *flag_ptr = list->used + (color_ptr - list->items);
    *flag_ptr = false;

    return color_list_success;
}
