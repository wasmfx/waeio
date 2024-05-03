// An implementation of the fiber.h interface using wasmfx continuations
#include <stdlib.h>
#include <stdint.h>

#include "fiber.h"
#include "wasm.h"

// Type for indices into the continuation table `$conts` on the wasm side. In
// this implementation, we consider `fiber_t` (which is a pointer to an
// incomplete type) to be equivalent to `cont_table_index_t` and freely convert
// between the two.
typedef uintptr_t cont_table_index_t;

// Initial size of the `$conts` table. Keep this value in sync with the
// corresponding (table ...) definition.
static const size_t INITIAL_TABLE_CAPACITY = 1024;

// The current capacity of the `$conts` table.
static size_t cont_table_capacity = INITIAL_TABLE_CAPACITY;
// Number of entries at the end of `$conts` table that we have never used so
// far.
// Invariant:
// `cont_table_unused_size` + `free_list_size` <= `cont_table_capacity`
static size_t cont_table_unused_size = INITIAL_TABLE_CAPACITY;

// This is a stack of indices into `$conts` that we have previously used, but
// subsequently freed. Allocated as part of `fiber_init`. Invariant: Once
// allocated, the capacity of the `free_list` (i.e., the number of `size_t`
// values we allocate memory for) is the same as `cont_table_capacity`.
static size_t* free_list = NULL;
// Number of entries in `free_list`.
// Invariant: free_list_size <= `cont_table_capacity`.
static size_t free_list_size = 0;


extern
__wasm_import("fiber_wasmfx_imports", "wasmfx_grow_cont_table")
void wasmfx_grow_cont_table(size_t);

extern
__wasm_import("fiber_wasmfx_imports", "wasmfx_indexed_cont_new")
void wasmfx_indexed_cont_new(fiber_entry_point_t, cont_table_index_t);

extern
__wasm_import("fiber_wasmfx_imports", "wasmfx_indexed_resume")
void* wasmfx_indexed_resume(size_t fiber_index, void *arg, fiber_result_t *result);

extern
__wasm_import("fiber_wasmfx_imports", "wasmfx_suspend")
void* wasmfx_suspend(void *arg);


 cont_table_index_t wasmfx_acquire_table_index() {
  uintptr_t table_index;
  if (cont_table_unused_size > 0) {
    // There is an entry in the continuation table that has not been used so far.
    table_index = cont_table_capacity - cont_table_unused_size;
    cont_table_unused_size--;
  } else if (free_list_size > 0) {
      // We can pop an element from the free list stack.
      table_index = free_list[free_list_size - 1];
      free_list_size--;
  } else {
      // We have run out of table entries.
      size_t new_cont_table_capacity = 2 * cont_table_capacity;

      // Ask wasm to grow the table by the previous size, and we grow the
      // `free_list` ourselves.
      wasmfx_grow_cont_table(cont_table_capacity);
      free(free_list);
      free_list = malloc(sizeof(size_t) * new_cont_table_capacity);

      // We added `cont_table_capacity` new entries to the table, and then
      // immediately consume one for the new continuation.
      cont_table_unused_size = cont_table_capacity - 1;
      table_index = cont_table_capacity;
      cont_table_capacity = new_cont_table_capacity;
  }
  return table_index;
}

void wasmfx_release_table_index(cont_table_index_t table_index) {
  free_list[free_list_size] = table_index;
  free_list_size++;
}


__wasm_export("fiber_alloc")
fiber_t fiber_alloc(fiber_entry_point_t entry) {
  cont_table_index_t table_index = wasmfx_acquire_table_index();
  wasmfx_indexed_cont_new(entry, table_index);

  return (fiber_t) table_index;
}

__wasm_export("fiber_free")
void fiber_free(fiber_t fiber) {
  cont_table_index_t table_index = (cont_table_index_t) fiber;

  // NOTE: Currently, fiber stacks are deallocated only when the continuation
  // returns. Thus, the only thing we can do here is releasing the table index.
  wasmfx_release_table_index(table_index);
}

__wasm_export("fiber_resume")
void* fiber_resume(fiber_t fiber, void *arg, fiber_result_t *result) {
  cont_table_index_t table_index = (cont_table_index_t) fiber;
  return wasmfx_indexed_resume(table_index, arg, result);
}

__wasm_export("fiber_yield")
void* fiber_yield(void *arg) {
  return wasmfx_suspend(arg);
}

__wasm_export("fiber_init")
void fiber_init() {
  free_list = malloc(INITIAL_TABLE_CAPACITY * sizeof(size_t));
}

__wasm_export("fiber_finalize") void fiber_finalize() {
  free(free_list);
}
