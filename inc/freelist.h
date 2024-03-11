// Compact freelist based on a bitmap
#ifndef WAEIO_FREELIST_H
#define WAEIO_FREELIST_H

#include <stdint.h>
#include <stdlib.h>

enum freelist_return_code {
  FREELIST_OK = 0,
  FREELIST_FULL = -1,
  FREELIST_MEM_ERR = -2,
  FREELIST_SIZE_ERR = -3,
  FREELIST_OB_ENTRY = -4,
};

typedef struct freelist* freelist_t;

extern int freelist_new(size_t freespace /* must be a power of 2 */, freelist_t /* out */ *freelist);
extern int freelist_next(freelist_t freelist, uint32_t /* out */ *entry);
extern int freelist_reclaim(freelist_t freelist, uint32_t entry);
extern void freelist_delete(freelist_t freelist);

#endif
