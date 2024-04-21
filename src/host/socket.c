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
static wasm_functype_t *fsig_i32_i32_i32_i32_to_i64 = NULL;
static wasm_functype_t *fsig_i64_i32_i32_i32_to_i32 = NULL;
static wasm_functype_t *fsig_i64_i32_to_i32 = NULL;

static wasmtime_func_t listenfn; // i32 i32 i32 -> i64
static wasmtime_extern_t listenex;
static wasmtime_func_t connectfn; // i32 i32 i32 i32 -> i64
static wasmtime_extern_t connectex;
static wasmtime_func_t acceptfn; // i64 i32 -> i64
static wasmtime_extern_t acceptex;
static wasmtime_func_t sendfn;   // i64 i32 i32 i32 -> i32
static wasmtime_extern_t sendex;
static wasmtime_func_t recvfn;   // i64 i32 i32 i32 -> i32
static wasmtime_extern_t recvex;
static wasmtime_func_t closefn;  // i64 i32 -> i32
static wasmtime_extern_t closeex;

DEFINE_BINDING(host_connect) {
  assert(nargs == 4);
  assert(nresults == 1);

  // Unpack addr and port args.
  int32_t addr_offset = int32_t_of_wasmtime_val_t(args[0]);
  int32_t addr_len = int32_t_of_wasmtime_val_t(args[1]);
  int32_t port = int32_t_of_wasmtime_val_t(args[2]);

  // Load memory.
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_connect");

  // Create and connect the socket.
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    int32_t offset = int32_t_of_wasmtime_val_t(args[3]);
    write_errno(mem+offset);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  assert((char*)(mem+addr_offset+addr_len) == '\0');
  addr.sin_addr.s_addr = inet_addr((char*)(mem+addr_offset)); // Read the address string.
  addr.sin_port = htons((int)port);

  int ans = connect((int)sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (ans < 0) {
    int32_t offset = int32_t_of_wasmtime_val_t(args[3]);
    write_errno(mem+offset);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
}

DEFINE_BINDING(host_accept) {
  assert(nargs == 2);
  assert(nresults == 1);

  // Unpack socket fd.
  int64_t sockfd = int64_t_of_wasmtime_val_t(args[0]);

  // Perform the system call.
  int ans = accept(sockfd, NULL, 0);

  if (ans < 0) {
    int32_t offset = int32_t_of_wasmtime_val_t(args[1]);
    int8_t *mem;
    LOAD_MEMORY(mem, "host_accept");
    write_errno(mem+offset);
  }

  return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
}

DEFINE_BINDING(host_listen) {
  assert(nargs == 3);
  assert(nresults == 1);

  // Unpack port and backlog
  int32_t port = int32_t_of_wasmtime_val_t(args[0]);
  int32_t backlog = int32_t_of_wasmtime_val_t(args[1]);

  // Create the socket.
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    uint8_t *mem;
    LOAD_MEMORY(mem, "host_listen");
    int32_t offset = int32_t_of_wasmtime_val_t(args[3]);
    write_errno(mem+offset);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
  }

  // Bind the socket.
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  int ans = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
  if (ans < 0) {
    uint8_t *mem;
    LOAD_MEMORY(mem, "host_listen");
    int32_t offset = int32_t_of_wasmtime_val_t(args[3]);
    write_errno(mem+offset);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  // Start listening.
  ans = listen(sockfd, backlog);
  if (ans < 0) {
    uint8_t *mem;
    LOAD_MEMORY(mem, "host_listen");
    int32_t offset = int32_t_of_wasmtime_val_t(args[3]);
    write_errno(mem+offset);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  return result1(results, (int64_t)sockfd);
}

wasmtime_error_t* host_socket_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module) {
  wasmtime_error_t *error = NULL;

  if (i32 == NULL)
    i32 = wasm_valtype_new(WASM_I32);

  if (i64 == NULL)
    i64 = wasm_valtype_new(WASM_i64);

  if (fsig_i64_i32_to_i64 == NULL) {
    // Accept
    fsig_i64_i32_to_i64 = wasm_functype_new_2_1(i64, i32, i64);
    wasmtime_func_new(context, fsig_i64_i32_to_i64, host_accept, NULL, NULL, &acceptfn);
    acceptex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = acceptfn } };
    LINK_HOST_FN("accept", acceptex)
  }

  if (fsig_i32_i32_i32_to_i64 == NULL) {
    // Listen
    fsig_i32_i32_i32_to_i64 = wasm_functype_new_3_1(i32, i32, i32, i64);
  }

  if (fsig_i32_i32_i32_i32_to_i64 == NULL) {
    // Connect
    fsig_i32_i32_i32_i32_to_i64 = wasm_functype_new_4_1(i32, i32, i32, i32, i64);
    wasmtime_func_new(context, fsig_i32_i32_i32_i32_to_i64, host_connect, NULL, NULL, &connectfn);
    connectex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = connectfn } };
    LINK_HOST_FN("connect", connectex)
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

  wasm_functype_delete(fsig_i32_i32_i32_to_i64);
  fsig_i32_i32_32_to_i64 = NULL;

  wasm_functype_delete(fsig_i32_i32_i32_i32_to_i64);
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


