/*
Example of instantiating a WebAssembly which uses WASI imports.

You can compile and run this example on Linux with:

   cargo build --release -p wasmtime-c-api
   clang-19 driver.c -I ../../wasmtime/crates/c-api/include -I ../../wasmtime/crates/c-api/wasm-c-api/include ../../wasmtime/target/release/libwasmtime.a -lpthread -ldl -lm -o driver
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wasi.h>
#include <wasm.h>
#include <wasmtime.h>

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

static inline uint32_t uint32_t_of_wasmtime_val_t(wasmtime_val_t v) {
  assert(v.kind == WASMTIME_I32);
  return (uint32_t)v.of.i32;
}

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap);

static wasm_trap_t* put_hello_fn(void *env, wasmtime_caller_t *caller,
                                 const wasmtime_val_t *args, size_t nargs,
                                 wasmtime_val_t *results, size_t nresults) {
  (void)env;
  (void)results;
  assert(nargs == 2);
  assert(nresults == 0);
  // Unpack arguments
  uint32_t offset = uint32_t_of_wasmtime_val_t(args[0]);
  uint32_t len    = uint32_t_of_wasmtime_val_t(args[1]);
  if (len < strlen("hello world"))
    return wasmtime_trap_new("[put_hello_fn] insufficient buffer capacity", strlen("[put_hello_fn] insufficient buffer capacity"));

  // Load memory.
  wasmtime_extern_t memory_extern;
  if (!wasmtime_caller_export_get(caller, "memory", strlen("memory"), &memory_extern))
    return wasmtime_trap_new("[put_hello_fn] cannot load memory", strlen("[put_hello_fn] cannot load memory"));
  assert(memory_extern.kind == WASMTIME_EXTERN_MEMORY);
  uint8_t *linmem = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory);
  // Copy the string.
  const char msg[] = "hello world";
  memcpy(linmem+offset, msg, strlen(msg));
  return NULL;
}

int main(void) {
  // Set up our context
  wasm_engine_t *engine = wasm_engine_new();
  assert(engine != NULL);
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  wasmtime_context_t *context = wasmtime_store_context(store);

  // Create a linker with WASI functions defined
  wasmtime_linker_t *linker = wasmtime_linker_new(engine);
  wasmtime_error_t *error = wasmtime_linker_define_wasi(linker);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);

  // Define host functions.
  wasm_functype_t *put_hello_sig = wasm_functype_new_2_0(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_t put_hello;
  wasmtime_func_new(context, put_hello_sig, put_hello_fn, NULL, NULL, &put_hello);
  wasmtime_extern_t put_hello_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = put_hello } };

  // Load our input file to parse it next
  wasm_byte_vec_t wasm;
  FILE *file = fopen("hello.wasm", "rb");
  if (!file) {
    printf("> Error loading file!\n");
    exit(1);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  wasm_byte_vec_new_uninitialized(&wasm, file_size);
  fseek(file, 0L, SEEK_SET);
  if (fread(wasm.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    exit(1);
  }
  fclose(file);

  // Compile our modules
  wasmtime_module_t *module = NULL;
  error = wasmtime_module_new(engine, (uint8_t *)wasm.data, wasm.size, &module);
  if (!module)
    exit_with_error("failed to compile module", error, NULL);
  wasm_byte_vec_delete(&wasm);

  // Instantiate wasi
  wasi_config_t *wasi_config = wasi_config_new();
  assert(wasi_config);
  wasi_config_inherit_argv(wasi_config);
  wasi_config_inherit_env(wasi_config);
  wasi_config_inherit_stdin(wasi_config);
  wasi_config_inherit_stdout(wasi_config);
  wasi_config_inherit_stderr(wasi_config);

  wasm_trap_t *trap = NULL;
  error = wasmtime_context_set_wasi(context, wasi_config);
  if (error != NULL)
    exit_with_error("failed to instantiate WASI", error, NULL);

  // Link host functions
  error = wasmtime_linker_define(linker, context, "host", strlen("host"), "put_hello", strlen("put_hello"), &put_hello_extern);
  if (error != NULL)
    exit_with_error("failed to export host function", error, NULL);

  // Instantiate the module
  error = wasmtime_linker_module(linker, context, "", 0, module);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);
  /* wasmtime_instance_t instance; */
  /* error = wasmtime_linker_define_instance(linker, context, "", 0, &instance); */
  /* if (error != NULL) */
  /*   exit_with_error("failed to define instance", error, NULL); */

  /* error = wasmtime_linker_instantiate(linker, context, module, &instance, &trap); */
  /* if (error != NULL || trap != NULL) */
  /*   exit_with_error("failed to instantiate module", error, trap); */

  // Run it.
  wasmtime_func_t func;
  error = wasmtime_linker_get_default(linker, context, "", 0, &func);
  if (error != NULL)
    exit_with_error("failed to locate default export for module", error, NULL);

  error = wasmtime_func_call(context, &func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("error calling default export", error, trap);

  // Clean up after ourselves at this point
  wasmtime_linker_delete(linker);
  wasmtime_module_delete(module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return 0;
}

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap) {
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  } else {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int)error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
