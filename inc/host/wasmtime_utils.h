// A collection of convenient utilities for embedding wasmtime + host bindings.
#ifndef WAEIO_WASMTIME_UTILS_H
#define WAEIO_WASMTIME_UTILS_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <wasm.h>
#include <wasmtime.h>

#define DEFINE_BINDING(NAME) static wasm_trap_t* NAME(void *env __attribute__((unused)), \
                                                      wasmtime_caller_t *caller __attribute__((unused)), \
                                                      const wasmtime_val_t *args, size_t nargs __attribute__((unused)), \
                                                      wasmtime_val_t *results, size_t nresults __attribute__((unused)))

__attribute__((unused))
static inline wasm_trap_t* result1(wasmtime_val_t *results, wasmtime_val_t arg) {
  results[0] = arg;
  return NULL;
}

__attribute__((unused))
static inline wasm_trap_t* result0(wasmtime_val_t *results __attribute__((unused))) {
  return NULL;
}

#define LOAD_MEMORY(memptr, host_fn_name) \
  { \
    wasmtime_extern_t memory_extern; \
    if (!wasmtime_caller_export_get(caller, "memory", strlen("memory"), &memory_extern)) \
      return wasmtime_trap_new("[" host_fn_name "] cannot load memory", strlen("[" host_fn_name "] cannot load memory")); \
    assert(memory_extern.kind == WASMTIME_EXTERN_MEMORY); \
    memptr = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory); \
  }

#define LINK_HOST_FN(name, ex) \
  { \
    error = wasmtime_linker_define(linker, context, export_module, strlen(export_module), name, strlen(name), &ex)); \
    if (error != NULL) return error; \
  }

static_assert(sizeof(uint8_t) == 1, "size of uint8_t");
__attribute__((unused))
static inline void write_errno(uint8_t *loc) {
  int err = errno;
  // NOTE(dhil): Safe guard for potential truncation.
  assert(INT32_MIN <= err && err <= INT32_MAX);
  uint32_t v = (uint32_t)err;

  loc[0] = (uint8_t)((uint32_t)v >> 24);
  loc[1] = (uint8_t)((uint32_t)v >> 16);
  loc[2] = (uint8_t)((uint32_t)v >> 8);
  loc[3] = (uint8_t)v;
}

__attribute__((unused))
static inline wasmtime_val_t wasmtime_val_t_of_int32_t(int32_t v) {
  return (wasmtime_val_t){ .kind = WASMTIME_I32, .of = { .i32 = v } };
}

__attribute__((unused))
static inline int32_t int32_t_of_wasmtime_val_t(wasmtime_val_t v) {
  assert(v.kind == WASMTIME_I32);
  return v.of.i32;
}

__attribute__((unused))
static inline wasmtime_val_t wasmtime_val_t_of_int64_t(int64_t v) {
  return (wasmtime_val_t){ .kind = WASMTIME_I64, .of = { .i64 = v } };
}


__attribute__((unused))
static inline int64_t int64_t_of_wasmtime_val_t(wasmtime_val_t v) {
  assert(v.kind == WASMTIME_I64);
  return v.of.i64;
}

#endif
