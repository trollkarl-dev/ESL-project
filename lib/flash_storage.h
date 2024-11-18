#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/****************************************
 * Example of Flash Storage Structure

             0x00  0x01  0x02  0x03
           +-----+-----+-----+-----+
0x00001000 | 'F' | 'S' | 'S' | 'T' |
           +-----+-----+-----+-----+
0x00001004 |   Item Size (bytes)   |
           +-----+-----+-----+-----+
0x00001008 |    Capacity (items)   |
           +-----+-----+-----+-----+
0x0000100C |         Item 0        |
           +-----+-----+-----+-----+
0x00001010 |         Item 1        |
           +-----+-----+-----+-----+
0x00001014 |         Item 2        |
           +-----+-----+-----+-----+
0x00001018 |         Item 3        |
           +-----+-----+-----+-----+
0x0000101C | 'F' | 'S' | 'S' | 'P' |
           +-----+-----+-----+-----+

****************************************/

enum { flash_storage_page_size = 0x1000 };

typedef enum {
    fs_err_success,
    fs_err_exist,
    fs_err_overflow,
    fs_err_notaligned
} flash_storage_err_t;

typedef struct {
    uint32_t item_size;
    uint32_t capacity_items;
    uint32_t amount;
    uint32_t *page_address;
    uint32_t page_offset;
    flash_storage_err_t status;
} flash_storage_t;

extern void flash_storage_page_erase(uint32_t address);
extern void flash_storage_write_bytes(uint32_t address,
                                      void const *bytes,
                                      uint32_t num);
extern bool flash_storage_write_done_check(void);

flash_storage_err_t flash_storage_init(flash_storage_t *storage,
                                       uint32_t item_size,
                                       uint32_t capacity_items,
                                       void *mem);
void flash_storage_rewrite(flash_storage_t *storage);
void *flash_storage_read_last(flash_storage_t *storage);
bool flash_storage_write(flash_storage_t *storage, void *data);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_H */
