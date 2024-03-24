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

#if UINT_MAX == UINT32_MAX
static_assert(sizeof(unsigned int) == 4, "bit width of unsigned int");
static const size_t int_width = 32;
static const unsigned int int_max = UINT32_MAX;
#elif UINT_MAX == UINT64_MAX
static_assert(sizeof(unsigned int) == 8, "bit width of unsigned int");
static const size_t int_width = 64;
static const unsigned int int_max = UINT64_MAX;
#else
#error "unsupported integer type: the bit width of int must be either 32 or 64"
#endif

#define FREELIST_STATIC_INITIALIZER(EXPONENT) \
  (struct freelist) { .size = (1 << EXPONENT) \
  , .len = ((1 << EXPONENT) % int_width == 0 ? (1 << EXPONENT) / int_width : (1 << EXPONENT) / int_width + 1) \
  , .vector = unsigned int[((1 << EXPONENT) % int_width == 0 ? (1 << EXPONENT) / int_width : (1 << EXPONENT) / int_width + 1)] }


extern freelist_result_t freelist_new(size_t freespace /* must be a power of 2 */, freelist_t /* out */ *freelist);
extern freelist_result_t freelist_next(freelist_t freelist, unsigned int /* out */ *entry);
extern freelist_result_t freelist_reclaim(freelist_t freelist, unsigned int entry);
extern void freelist_delete(freelist_t freelist);

#endif
