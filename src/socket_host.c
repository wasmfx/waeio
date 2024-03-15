// Wasmtime host bindings for the socket API

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <freelist.h>
#include <wasm.h>
#include <wasmtime.h>

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

#define WASMTIME_TRAP_NEW(CONST_MSG) wasmtime_trap_new(CONST_MSG, sizeof(CONST_MSG))

#define WASMTIME_LINKER_DEFINE(LINKER, CONTEXT, EXPORT_MODULE_NAME, EXPORT_ITEM_NAME, EXTERN_ITEM) \
  wasmtime_linker_define(LINKER, CONTEXT, EXPORT_MODULE_NAME, sizeof(EXPORT_MODULE_NAME), EXPORT_ITEM_NAME, sizeof(EXPORT_ITEM_NAME), EXTERN_ITEM)

#define WASMTIME_CALLER_EXPORT_GET(CALLER, EXPORT_NAME, EXTERN_ITEM) \
  wasmtime_caller_export_get(CALLER, EXPORT_NAME, sizeof(EXPORT_NAME), EXTERN_ITEM)

static inline wasm_trap_t* write_errno(wasmtime_caller_t *caller, const wasmtime_val_t *errno_arg) {
  int32_t err_offset = int32_t_of_wasmtime_val_t(ERRNO_ARG);
  int32_t err = (int32_t)errno; // TODO(dhil): Possible truncation...
  wasmtime_extern_t memory_extern = { .kind = WASMTIME_EXTERN_MEMORY };
  if (!WASMTIME_CALLER_EXPORT_GET(caller, "memory", &memory_extern))
    return WASMTIME_TRAP_NEW("[" CALLER "] cannot load memory");
  uint8_t *linmem = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory);
  // TODO(dhil): Bounds checking?
  linmem[0] = (n >> 24) & 0xFF;
  linmem[1] = (n >> 16) & 0xFF;
  linmem[2] = (n >> 8) & 0xFF;
  linmem[3] = n & 0xFF;
  return NULL;
}

static freelist_t freelist = NULL;

static bool initialised = false;

static wasm_functype_t *accept_sig = NULL;
static wasmtime_func_t accept_func;
static wasmtime_extern_t accept_extern;

static wasm_functype_t *recv_sig = NULL;
static wasmtime_func_t recv_func;
static wasmtime_extern_t recv_extern;

static wasm_functype_t *send_sig = NULL;
static wasmtime_func_t send_func;
static wasmtime_extern_t send_extern;

static wasm_functype_t *close_sig = NULL;
static wasmtime_func_t close_func;
static wasmtime_extern_t close_extern;


// sig : (fd: i32, errno: out i32) -> i32
wasm_trap_t* host_accept(void *env, wasmtime_caller_t *caller,
                         const wasmtime_val_t *args, size_t nargs,
                         wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 2);
  assert(nresults == 1);

  // Unpack the argument.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);

  // Get host socket.
  struct guest_socket sock;
  if (!get_guest_socket(guest_fd, &sock))
    return WASMTIME_TRAP_NEW("Invalid guest file descriptor");

  // Perform the system call.
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int ans = accept(sock.host_fd, (struct sockaddr *)&client_addr, &client_len);

  if (ans < 0) {
    // Error
    // Pack and return the result.
    WRITE_ERRNO("host_accept", args[1]);
    results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
    return NULL;
  } else {
    // Success
    // Allocate a new guest socket.
    struct guest_socket client_sock = { .host_fd = ans, .addr = client_addr };
    int32_t res = alloc_guest_socket(client_sock);
    if (res < 0) return WASMTIME_TRAP_NEW("Failed to allocate guest socket");
    results[0] = wasmtime_val_t_of_int32_t(res);
    return NULL;
  }
}

// sig : (fd: i32, buf: i32, len: i32, errno: out i32) -> i32
wasm_trap_t* host_recv(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  return WASMTIME_TRAP_NEW("TODO");
}

// sig : (fd: i32, buf: i32, len: i32, errno: out i32) -> i32
wasm_trap_t* host_send(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  return WASMTIME_TRAP_NEW("TODO");
}

// sig : (fd: i32, errno: out i32) -> i32
wasm_trap_t* host_close(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 2);
  assert(nresults == 1);

  // Unpack the argument.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);

  // Get the guest descriptor.
  struct guest_socket sock;
  if (get_guest_socket(guest_fd, &sock) < 0)
    return WASMTIME_TRAP_NEW("[host_close] invalid guest file descriptor");

  // Perform the system call.
  int ans = close(sock.host_fd);

  // Pack and return the result.
  results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
  return NULL;
}

static inline wasm_functype_t* wasm_functype_new_4_1( wasm_valtype_t *arg1
                                                    , wasm_valtype_t *arg2
                                                    , wasm_valtype_t *arg3
                                                    , wasm_valtype_t *arg4
                                                    , wasm_valtype_t *res ) {
  wasm_valtype_t* ps[4] = {arg1, arg2, arg3, arg4};
  wasm_valtype_t* rs[1] = {res};
  wasm_valtype_vec_t params, results;
  wasm_valtype_vec_new(&params, 4, ps);
  wasm_valtype_vec_new(&results, 1, rs);
  return wasm_functype_new(&params, &results);
}

wasmtime_error_t* wasmtime_socket_linker_define(wasmtime_linker_t *linker, wasmtime_context_t *context) {
  assert(!initialised);
  wasmtime_error_t *error = NULL;

  accept_sig = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, accept_sig, host_accept, NULL, NULL, &accept_func);
  accept_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = accept_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "accept", &accept_extern);
  if (error != NULL) return error;

  recv_sig = wasm_functype_new_4_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, recv_sig, host_recv, NULL, NULL, &recv_func);
  recv_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = recv_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "recv", &recv_extern);
  if (error != NULL) return error;

  send_sig = wasm_functype_new_4_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, send_sig, host_send, NULL, NULL, &send_func);
  send_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = send_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "send", &send_extern);
  if (error != NULL) return error;

  close_sig = wasm_functype_new__1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, send_sig, host_send, NULL, NULL, &send_func);
  close_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = close_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "close", &send_extern);
  if (error != NULL) return error;

  initialised = true;
  return NULL;
}

void wasmtime_socket_teardown(void) {
  assert(initialised);
  wasm_functype_delete(tcp_stream_socket_sig);
  wasm_functype_delete(listen_sig);
  wasm_functype_delete(accept_sig);
  wasm_functype_delete(recv_sig);
  wasm_functype_delete(send_sig);
  wasm_functype_delete(close_sig);
  initialised = false;
}
