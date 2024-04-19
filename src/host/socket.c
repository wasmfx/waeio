// Host-defined socket implementation

#include <error.h>
#include <host/socket.h>
#include <host/wasmtime_utils.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <wasm.h>
#include <wasmtime.h>

static wasm_valtype_t *i32 = NULL;
static wasm_valtype_t *i64 = NULL;

static wasm_functype_t *fsig_i64_i32_to_i64 = NULL;
static wasm_functype_t *fsig_i32_i32_i32_to_i64 = NULL;
static wasm_functype_t *fsig_i64_i32_i32_i32_to_i32 = NULL;
static wasm_functype_t *fsig_i64_i32_to_i32 = NULL;

static wasmtime_func_t listenfn; // i32 i32 i32 -> i64
static wasmtime_extern_t listenex;
static wasmtime_func_t connectfn; // i32 i32 i32 -> i64
static wasmtime_extern_t connectex;
static wasmtime_func_t acceptfn; // i64 i32 -> i64
static wasmtime_extern_t acceptex;
static wasmtime_func_t sendfn;   // i64 i32 i32 i32 -> i32
static wasmtime_extern_t sendex;
static wasmtime_func_t recvfn;   // i64 i32 i32 i32 -> i32
static wasmtime_extern_t recvex;
static wasmtime_func_t closefn;  // i64 i32 -> i32
static wasmtime_extern_t closeex;


static inline uint8_t* load_memory(const char *host_fn_name) {
}

DEFINE_BINDING(host_connect) {
  assert(nargs == 3);
  assert(nresults == 1);

  // Unpack addr and port args.
  int32_t addr_offset = int32_t_of_wasmtime_val_t(args[0]);
  //int32_t addr_len = int32_t_of_wasmtime_val_t(args[1]);
  int32_t port = int32_t_of_wasmtime_val_t(args[2]);

  // Load memory.
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_connect");

  // Create and connect the socket.
  int64_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
  // TODO error checking.
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr((char*)(mem+addr_offset)); // Read the address string.
  addr.sin_port = htons((int)port);

  int ans = connect((int)sockfd, (struct sockaddr *)&addr, sizeof(addr));
  // TODO error checking

  RETURN(results, wasmtime_val_t_of_int64_t(sockfd));
}

wasmtime_error_t* host_socket_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module) {
  wasmtime_error_t *error = NULL;

  if (i32 == NULL) {
    i32 = wasm_valtype_new(WASM_I32);
  }

  if (i64 == NULL) {
    i64 = wasm_valtype_new(WASM_i64);
  }

  if (fsig_i64_i32_to_i64 == NULL) {
    // Accept
    fsig_i64_i32_to_i64 = wasm_functype_new_2_1(i64, i32, i64);
  }

  if (fsig_i32_i32_i32_to_i64 == NULL) {
    // Listen and connect
    fsig_i32_i32_i32_to_i64 = wasm_functype_new_3_1(i32, i32, i32, i64);
  }

  if (fsig_i64_i32_i32_i32_to_i32 == NULL) {
    // Send and recv
    fsig_i64_i32_i32_i32_to_i32 = wasm_functype_new_4_1(i64, i32, i32, i32, i32);
  }

  if (fsig_i64_i32_to_i32 == NULL) {
    // Close
    fsig_i64_i32_to_i32 = wasm_functype_new_2_1(i64, i32, i32);
  }
  return error;
}

void host_socket_delete(void) {
  wasm_functype_delete(fsig_i64_i32_to_i64);
  fsig_i64_i32_to_i64 = NULL;
  wasm_functype_delete(fsig_i32_i32_32_to_i64);
  fsig_i32_i32_32_to_i64 = NULL;
  wasm_functype_delete(fsig_i64_i32_to_i64);
  fsig_i64_i32_to_i64 = NULL;
  wasm_functype_delete(fsig_i64_i32_i32_i32_to_i32);
  fsig_i64_i32_i32_i32_to_i32 = NULL;
  wasm_functype_delete(fsig_i64_i32_to_i32);
  fsig_i64_i32_to_i32 = NULL;
  wasm_valtype_delete(i32);
  i32 = NULL;
  wasm_valtype_delete(i64);
  i64 = NULL;
}


