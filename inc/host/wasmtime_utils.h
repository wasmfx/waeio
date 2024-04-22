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

#include <stdio.h>

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
    error = wasmtime_linker_define(linker, context, export_module, strlen(export_module), name, strlen(name), &ex); \
    if (error != NULL) return error; \
  }

static_assert(sizeof(uint8_t) == 1, "size of uint8_t");
__attribute__((unused))
static inline void memory_write_u32(uint8_t *mem, uint32_t u32) {
  mem[0] = (uint8_t)u32;
  mem[1] = (uint8_t)(u32 >> 8);
  mem[2] = (uint8_t)(u32 >> 16);
  mem[3] = (uint8_t)(u32 >> 24);
}

#define WRITE_ERRNO(host_fn_name, args_offset) \
  { \
    /* NOTE(dhil): Safe guard for potential truncation. */ \
    int err = errno; \
    assert(INT32_MIN <= err && err <= INT32_MAX); \
    uint8_t *mem; \
    LOAD_MEMORY(mem, host_fn_name); \
    uint32_t offset = uint32_t_of_wasmtime_val_t(args[args_offset]); \
    memory_write_u32(mem+offset, (uint32_t)err); \
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

__attribute__((unused))
static inline uint32_t uint32_t_of_wasmtime_val_t(wasmtime_val_t v) {
  return (uint32_t)int32_t_of_wasmtime_val_t(v);
}

__attribute__((unused))
static inline wasm_functype_t* wasm_functype_new_4_1(
  wasm_valtype_t* p1, wasm_valtype_t* p2, wasm_valtype_t* p3, wasm_valtype_t* p4,
  wasm_valtype_t* r
) {
  wasm_valtype_t* ps[4] = {p1, p2, p3, p4};
  wasm_valtype_t* rs[1] = {r};
  wasm_valtype_vec_t params, results;
  wasm_valtype_vec_new(&params, 4, ps);
  wasm_valtype_vec_new(&results, 1, rs);
  return wasm_functype_new(&params, &results);
}

#define NEW_WASM_I32 wasm_valtype_new(WASM_I32)
#define NEW_WASM_I64 wasm_valtype_new(WASM_I64)

#endif
