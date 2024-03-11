#include <assert.h>
#include <stdio.h>
#include <freelist.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t bitmap_t;

struct freelist {
  size_t size; // number of elements managed by the freelist
  size_t len; // length of the bitvector
  bitmap_t vector[];
};

static const size_t bitvector_width = 32;

static_assert(sizeof(bitmap_t) == 4, "bit width of bitmap_t");

static inline size_t calc_vector_len(size_t freespace) {
  return freespace % bitvector_width == 0 ? freespace / bitvector_width : freespace / bitvector_width + 1;
}

int freelist_new(size_t freespace /* must be a power of 2 */, freelist_t *fl) {
  if (!freespace || (freespace & (freespace - 1))) {
    return FREELIST_SIZE_ERR;
  }
  size_t len = calc_vector_len(freespace);
  *fl = (freelist_t)malloc(sizeof(struct freelist) + sizeof(bitmap_t[len]));
  if (*fl == NULL) return FREELIST_MEM_ERR;
  (*fl)->size = freespace;
  (*fl)->len = len;
  memset((*fl)->vector, ~0LL, freespace); // all ones initialised
  return FREELIST_OK;
}

int freelist_next(freelist_t freelist, uint32_t *entry) {
  for (size_t i = 0; i < freelist->len; i++) {
    int ans = __builtin_ffs(freelist->vector[i]);
    // TODO(dhil): Consider simplifying such that the minimum size is
    // 32, and the length is a multiple of 32.
    if (ans > 0 && (size_t)(ans - 1) < freelist->size) {
      uint32_t index = ans - 1;
      freelist->vector[i] &= ~(1 << index);
      *entry = index + (bitvector_width * i);
      /* printf("Found index: %d, ans: %d, i: %zu, length: %zu, size: %zu\n", (int)*entry, ans, i, freelist->len, freelist->size); */
      /* fflush(stdout); */
      return FREELIST_OK;
    }
  }
  return FREELIST_FULL;
}

int freelist_reclaim(freelist_t freelist, uint32_t entry) {
  if (entry >= (bitvector_width * freelist->len)) {
    return FREELIST_OB_ENTRY;
  }
  uint32_t v_index = entry / bitvector_width;
  uint32_t b_index = entry - (bitvector_width * v_index);
  freelist->vector[v_index] |= 1 << b_index;
  return FREELIST_OK;
}

void freelist_delete(freelist_t freelist) {
  free(freelist);
  freelist = NULL;
}
