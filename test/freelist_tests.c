#include <assert.h>
#include <freelist.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

void fill_list(size_t capacity, freelist_t fl) {
  uint32_t entry;
  for (size_t i = 0; i < capacity; i++) {
    /* printf("i: %zu\n", i); */
    assert(freelist_next(fl, &entry) == FREELIST_OK);
  }
}

int main(void) {
  uint32_t entry;
  freelist_t fl;

  // Allocation tests
  assert(freelist_new(0, &fl) == FREELIST_SIZE_ERR);
  assert(freelist_new(3, &fl) == FREELIST_OK);

  // Tests with small list (size 4).
  assert(freelist_resize(&fl, 1 << 2 /* 4 */) == FREELIST_OK);
  fill_list(1 << 2, fl);
  assert(freelist_next(fl, &entry) == FREELIST_FULL);
  assert(freelist_reclaim(fl, 3) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 3);
  assert(freelist_next(fl, &entry) == FREELIST_FULL);
  assert(freelist_reclaim(fl, 0) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 0);
  assert(freelist_reclaim(fl, 1) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 1);
  assert(freelist_reclaim(fl, 2) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 2);
  assert(freelist_reclaim(fl, 4) == FREELIST_OB_ENTRY);
  freelist_delete(fl);

  // Tests with a slightly larger list (size 128).
  assert(freelist_new(1 << 7 /* 128 */, &fl) == FREELIST_OK);
  fill_list(1 << 7, fl);
  assert(freelist_next(fl, &entry) == FREELIST_FULL);
  assert(freelist_reclaim(fl, 67) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 67);
  assert(freelist_reclaim(fl, 256) == FREELIST_OB_ENTRY);
  freelist_delete(fl);

  // Resize tests.
  assert(freelist_new(3, &fl) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 0);
  assert(freelist_resize(&fl, 4) == FREELIST_OK);
  assert(freelist_next(fl, &entry) == FREELIST_OK && entry == 1);
  assert(freelist_resize(&fl, 1) == FREELIST_OK);
  assert(freelist_reclaim(fl, 1) == FREELIST_OB_ENTRY);
  assert(freelist_next(fl, &entry) == FREELIST_FULL);
  assert(freelist_reclaim(fl, 0) == FREELIST_OK);
  freelist_delete(fl);

  return 0;
}
