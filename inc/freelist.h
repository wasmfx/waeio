// Compact freelist based on a bitmap
#ifndef WAEIO_FREELIST_H
#define WAEIO_FREELIST_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

// Detect the native int of the *compilation target platform*
#if UINT_MAX == UINT32_MAX
typedef uint32_t natint_t;
#elif UINT_MAX == UINT64_MAX
typedef uint64_t natint_t;
#else
#error "unsupported integer type: the bit width of int must be either 32 or 64"
#endif

typedef enum freelist_return_code {
  FREELIST_OK = 0,
  FREELIST_FULL = -1,
  FREELIST_MEM_ERR = -2,
  FREELIST_SIZE_ERR = -3,
  FREELIST_OB_ENTRY = -4,
} freelist_result_t;

typedef struct freelist* freelist_t;

extern freelist_result_t freelist_new(size_t freespace /* must be a power of 2 */, freelist_t /* out */ *freelist);
extern freelist_result_t freelist_next(freelist_t freelist, natint_t /* out */ *entry);
extern freelist_result_t freelist_reclaim(freelist_t freelist, natint_t entry);
extern void freelist_delete(freelist_t freelist);

#endif
