#include "flash_storage.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t header[] = {'F', 'S', 'S', 'T'}; /* Flash Storage STart */
static const uint8_t footer[] = {'F', 'S', 'S', 'P'}; /* Flash Storage StoP */

static uint8_t temp_page[flash_storage_page_size];

static bool is_multiple_of(uint32_t value, uint32_t check_value)
{
    return ((value / check_value) * check_value) == value;
}

static bool flash_storage_item_is_empty(uint8_t *ptr, uint32_t item_size)
{
    uint8_t control_byte = 0xFF;
    uint32_t i;

    for (i = 0; i < item_size; i++)
    {
        control_byte &= *(ptr + i);
    }

    return control_byte == 0xFF;
}

static uint8_t *flash_storage_get_first_itemptr(flash_storage_t *storage)
{
    return (uint8_t *) storage->page_address +
           storage->page_offset +
           sizeof(header) +
           2 * sizeof(uint32_t);
}

static uint8_t *flash_storage_get_last_itemptr(flash_storage_t *storage)
{
    uint8_t *first_item = flash_storage_get_first_itemptr(storage);

    return first_item + storage->item_size * (storage->capacity_items - 1);
}

static uint8_t *flash_storage_find_last(flash_storage_t *storage)
{
    uint8_t *first_item = flash_storage_get_first_itemptr(storage);
    uint8_t *last_item = flash_storage_get_last_itemptr(storage);

    if (!((storage->status == fs_err_success) ||
          (storage->status == fs_err_exist)))
    {
        return NULL;
    }

    for (; last_item >= first_item; last_item -= storage->item_size)
    {
        if (!flash_storage_item_is_empty(last_item, storage->item_size))
        {
            return last_item;
        }
    }

    return NULL;
}

void flash_storage_rewrite(flash_storage_t *storage)
{
    uint32_t params[2];

    uint32_t payload_size = storage->item_size * storage->capacity_items;

    uint32_t header_offset = storage->page_offset;
    uint32_t params_offset = header_offset + sizeof(header);
    uint32_t payload_offset = params_offset + 2 * sizeof(uint32_t);
    uint32_t footer_offset = payload_offset + payload_size;

    params[0] = storage->item_size;
    params[1] = storage->capacity_items;

    memcpy(temp_page, storage->page_address, flash_storage_page_size);
    memcpy(temp_page + header_offset, header, sizeof(header));
    memcpy(temp_page + footer_offset, footer, sizeof(footer));
    memcpy(temp_page + params_offset, params, sizeof(params));
    memset(temp_page + payload_offset, 0xFF, payload_size);

    flash_storage_page_erase((uint32_t) storage->page_address);
    flash_storage_write_bytes((uint32_t) storage->page_address,
                              temp_page,
                              flash_storage_page_size);

    while (!flash_storage_write_done_check())
    {

    }

    storage->amount = 0;
}

flash_storage_err_t flash_storage_init(flash_storage_t *storage,
                                       uint32_t item_size,
                                       uint32_t capacity_items,
                                       void *mem)
{
    uint8_t *memory = (uint8_t *) mem;

    uint32_t page_offset = (uint32_t) memory % flash_storage_page_size;
    uint32_t page_address = (uint32_t) memory - page_offset;

    uint32_t payload_size = item_size * capacity_items;
    uint32_t full_size = payload_size +
                         sizeof(header) +
                         sizeof(footer) +
                         2 * sizeof(uint32_t);

    uint32_t header_offset = 0;
    uint32_t params_offset = header_offset + sizeof(header);
    uint32_t payload_offset = params_offset + 2 * sizeof(uint32_t);
    uint32_t footer_offset = payload_offset + payload_size;

    uint32_t *params = (uint32_t *) (memory + params_offset);

    if (!(is_multiple_of((uint32_t) mem, 4) &&
          is_multiple_of(item_size, 4)))
    {
        storage->status = fs_err_notaligned;
        return fs_err_notaligned;
    }

    if ((full_size + page_offset) > flash_storage_page_size)
    {
        storage->status = fs_err_overflow;
        return fs_err_overflow;
    }

    if ((0 == memcmp(memory + header_offset, header, sizeof(header))) &&
        (0 == memcmp(memory + footer_offset, footer, sizeof(footer))) &&
        (item_size == params[0]) &&
        (capacity_items == params[1]))
    {
        uint8_t *last_written_item;

        storage->item_size = params[0];
        storage->capacity_items = params[1];
        storage->page_address = (uint32_t *) page_address;
        storage->page_offset = page_offset;
        storage->status = fs_err_exist;

        last_written_item = flash_storage_find_last(storage);

        if (last_written_item == NULL)
        {
            storage->amount = 0;
        }
        else
        {
            uint8_t *first_item = flash_storage_get_first_itemptr(storage);

            storage->amount =
            ((last_written_item - first_item) / storage->item_size) + 1;
        }

        return fs_err_exist;
    }

    storage->item_size = item_size;
    storage->capacity_items = capacity_items;
    storage->page_address = (uint32_t *) page_address;
    storage->page_offset = page_offset;
    storage->status = fs_err_success;
    storage->amount = 0;

    flash_storage_rewrite(storage);

    return fs_err_success;
}

void *flash_storage_read_last(flash_storage_t *storage)
{
    uint8_t *first_item = flash_storage_get_first_itemptr(storage);

    if (!((storage->status == fs_err_success) ||
          (storage->status == fs_err_exist)))
    {
        return NULL;
    }

    if (storage->amount == 0)
        return NULL;
    else
        return first_item + storage->item_size * (storage->amount - 1);
}

bool flash_storage_write(flash_storage_t *storage, void *data)
{
    uint8_t *first_item = flash_storage_get_first_itemptr(storage);

    if (!((storage->status == fs_err_success) ||
          (storage->status == fs_err_exist)))
    {
        return false;
    }

    if (flash_storage_item_is_empty(data, storage->item_size))
    {
        return false;
    }

    if (storage->amount < storage->capacity_items)
    {
        uint8_t *item = first_item + storage->amount * storage->item_size;
        flash_storage_write_bytes((uint32_t) item,
                                  data,
                                  storage->item_size);

        while (!flash_storage_write_done_check())
        {

        }

        storage->amount++;
        return true;
    }

    flash_storage_rewrite(storage);
    return flash_storage_write(storage, data);
}
