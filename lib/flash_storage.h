#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

enum { flash_storage_page_size = 0x1000 };

typedef struct {
    uint32_t item_size;
    uint32_t capacity_items;
    uint32_t *page_address;
    uint32_t page_offset;
} flash_storage_t;

typedef enum {
    fs_err_overflow,
    fs_err_exist,
    fs_err_notaligned,
    fs_err_success
} flash_storage_err_t;

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
void flash_storage_read_last(flash_storage_t *storage, void **data);
void flash_storage_write(flash_storage_t *storage, void *data);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_H */
