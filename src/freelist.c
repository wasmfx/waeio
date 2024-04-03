// TODO(dhil): Perhaps make this a header only library to enable
// static allocation of freelists.
#include <assert.h>
#include <freelist.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

static const size_t int_width = 32;
#if UINT_MAX == UINT32_MAX
#define MASK_HIGH32(X) X
#elif UINT_MAX == UINT64_MAX
#define MASK_HIGH32(X) (X & 0x00000000_FFFFFFFF)
#else
#error "unsupported integer type: the bit width of int must be either 32 or 64"
#endif

struct freelist {
  uint32_t size; // number of elements managed by the freelist
  uint32_t len; // length of the bitvector
  uint32_t vector[];
};

static inline uint32_t calc_vector_len(uint32_t freespace) {
  return freespace % int_width == 0 ? freespace / int_width : freespace / int_width + 1;
}

freelist_result_t freelist_new(uint32_t freespace /* must be a positive number */, freelist_t *fl) {
  if (freespace == 0) return FREELIST_SIZE_ERR;
  uint32_t len = calc_vector_len(freespace);
  *fl = (freelist_t)malloc(sizeof(struct freelist) + sizeof(uint32_t[len]));
  if (*fl == NULL) return FREELIST_MEM_ERR;
  (*fl)->size = freespace;
  (*fl)->len = len;
  memset((*fl)->vector, MASK_HIGH32(UINT32_MAX), sizeof(uint32_t) * len); // all ones initialised
  return FREELIST_OK;
}

freelist_result_t freelist_next(freelist_t freelist, uint32_t *entry) {
  for (size_t i = 0; i < freelist->len; i++) {
    int32_t ans = (uint32_t)__builtin_ffs(MASK_HIGH32(freelist->vector[i]));
    // TODO(dhil): Consider simplifying such that the minimum size is
    // 32, and the length is a multiple of 32.
    if (ans > 0 && (ans - (uint32_t)1) < freelist->size) {
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

freelist_result_t freelist_resize(freelist_t *freelist, uint32_t freespace) {
  if (freespace == 0) return FREELIST_SIZE_ERR;
  uint32_t new_len = calc_vector_len(freespace);
  *freelist = (freelist_t)realloc(*freelist, sizeof(struct freelist) + sizeof(uint32_t[new_len]));
  if (*freelist == NULL) return FREELIST_MEM_ERR;
  (*freelist)->len = new_len;
  (*freelist)->size = freespace;
  return FREELIST_OK;
}
