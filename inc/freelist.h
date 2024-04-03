// Compact freelist based on a bitmap
#ifndef WAEIO_FREELIST_H
#define WAEIO_FREELIST_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum freelist_return_code {
  FREELIST_OK = 0,
  FREELIST_FULL = -1,
  FREELIST_MEM_ERR = -2,
  FREELIST_SIZE_ERR = -3,
  FREELIST_OB_ENTRY = -4,
} freelist_result_t;

typedef struct freelist* freelist_t;

extern freelist_result_t freelist_new(uint32_t freespace /* must be positive */, freelist_t /* out */ *freelist);
extern freelist_result_t freelist_next(freelist_t freelist, uint32_t /* out */ *entry);
extern freelist_result_t freelist_reclaim(freelist_t freelist, uint32_t entry);
extern freelist_result_t freelist_resize(freelist_t *freelist, uint32_t freespace);
extern void freelist_delete(freelist_t freelist);

#endif
