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
#include <host/driver/poll.h>
#include <host/driver/socket.h>
#include <host/wasmtime_utils.h>

static void exit_with_error(const char *message, wasmtime_error_t *error,
                            wasm_trap_t *trap);

int main(int argc, const char **argv) {

  if (argc < 2) {
    printf("usage: %s <file.wasm>\n", argv[0]);
    exit(1);
  }

  // Set up our context
  wasm_config_t *config = wasm_config_new();
#if defined DEBUG
  wasmtime_config_debug_info_set(config, true);
#endif
  wasmtime_config_wasm_function_references_set(config, true);
  wasmtime_config_wasm_exceptions_set(config, true);
  wasmtime_config_wasm_typed_continuations_set(config, true);
  wasm_engine_t *engine = wasm_engine_new_with_config(config);
  assert(engine != NULL);
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  wasmtime_context_t *context = wasmtime_store_context(store);

  // Create a linker with WASI functions defined
  wasmtime_linker_t *linker = wasmtime_linker_new(engine);
  wasmtime_error_t *error = wasmtime_linker_define_wasi(linker);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);

  // Load our input file to parse it next
  wasm_byte_vec_t wasm;
  FILE *file = fopen(argv[1], "rb");
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
  error = host_socket_init(linker, context, "host_socket");
  if (error != NULL)
    exit_with_error("failed to export host function", error, NULL);

  error = host_poll_init(linker, context, "host_poll");
  if (error != NULL)
    exit_with_error("failed to export host function", error, NULL);

  // Instantiate the module
  error = wasmtime_linker_module(linker, context, "", 0, module);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);

  // Run it.
  wasmtime_func_t func;
  error = wasmtime_linker_get_default(linker, context, "", 0, &func);
  if (error != NULL)
    exit_with_error("failed to locate default export for module", error, NULL);

  error = wasmtime_func_call(context, &func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("error calling default export", error, trap);

  // Clean up after ourselves at this point
  host_socket_delete();
  host_poll_delete();
  wasmtime_linker_delete(linker);
  wasmtime_module_delete(module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine); // deletes config too.
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
