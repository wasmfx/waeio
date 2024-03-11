#include <assert.h>
#include <freelist.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#if UINT_MAX == UINT32_MAX
typedef uint32_t bitmap_t;
static_assert(sizeof(bitmap_t) == 4, "bit width of bitmap_t");
static const size_t int_width = 32;
static const natint_t natint_max = UINT32_MAX;
#elif UINT_MAX == UINT64_MAX
typedef uint64_t bitmap_t;
static_assert(sizeof(bitmap_t) == 8, "bit width of bitmap_t");
static const size_t int_width = 64;
static const natint_t natint_max = UINT64_MAX;
#else
#error "unsupported integer type: the bit width of int must be either 32 or 64"
#endif

struct freelist {
  size_t size; // number of elements managed by the freelist
  size_t len; // length of the bitvector
  bitmap_t vector[];
};

static inline size_t calc_vector_len(size_t freespace) {
  return freespace % int_width == 0 ? freespace / int_width : freespace / int_width + 1;
}

freelist_result_t freelist_new(size_t freespace /* must be a power of 2 */, freelist_t *fl) {
  if (!freespace || (freespace & (freespace - 1))) {
    return FREELIST_SIZE_ERR;
  }
  size_t len = calc_vector_len(freespace);
  *fl = (freelist_t)malloc(sizeof(struct freelist) + sizeof(bitmap_t[len]));
  if (*fl == NULL) return FREELIST_MEM_ERR;
  (*fl)->size = freespace;
  (*fl)->len = len;
  memset((*fl)->vector, natint_max, sizeof(natint_t) * len); // all ones initialised
  return FREELIST_OK;
}

freelist_result_t freelist_next(freelist_t freelist, uint32_t *entry) {
  for (size_t i = 0; i < freelist->len; i++) {
    int ans = __builtin_ffs(freelist->vector[i]);
    // TODO(dhil): Consider simplifying such that the minimum size is
    // 32, and the length is a multiple of 32.
    if (ans > 0 && (size_t)(ans - 1) < freelist->size) {
      uint32_t index = ans - 1;
      freelist->vector[i] &= ~(1 << index);
      *entry = index + (int_width * i);
      /* printf("Found index: %d, ans: %d, i: %zu, length: %zu, size: %zu\n", (int)*entry, ans, i, freelist->len, freelist->size); */
      /* fflush(stdout); */
      return FREELIST_OK;
    }
  }
  return FREELIST_FULL;
}

freelist_result_t freelist_reclaim(freelist_t freelist, uint32_t entry) {
  if (entry >= freelist->size) {
    return FREELIST_OB_ENTRY;
  }
  uint32_t v_index = entry / int_width;
  uint32_t b_index = entry - (int_width * v_index);
  freelist->vector[v_index] |= 1 << b_index;
  return FREELIST_OK;
}

void freelist_delete(freelist_t freelist) {
  free(freelist);
  freelist = NULL;
}
