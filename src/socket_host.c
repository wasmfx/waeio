// Wasmtime host bindings for the socket API

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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

/* static inline wasm_trap_t* write_errno(wasmtime_caller_t *caller, const wasmtime_val_t *errno_arg) { */
/*   int32_t err_offset = int32_t_of_wasmtime_val_t(ERRNO_ARG); */
/*   int32_t err = (int32_t)errno; // TODO(dhil): Possible truncation... */
/*   wasmtime_extern_t memory_extern = { .kind = WASMTIME_EXTERN_MEMORY }; */
/*   if (!WASMTIME_CALLER_EXPORT_GET(caller, "memory", &memory_extern)) */
/*     return WASMTIME_TRAP_NEW("[" CALLER "] cannot load memory"); */
/*   uint8_t *linmem = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory); */
/*   // TODO(dhil): Bounds checking? */
/*   linmem[0] = (n >> 24) & 0xFF; */
/*   linmem[1] = (n >> 16) & 0xFF; */
/*   linmem[2] = (n >> 8) & 0xFF; */
/*   linmem[3] = n & 0xFF; */
/*   return NULL; */
/* } */

typedef enum guest_fd_result {
  GUEST_FD_OK = 0,
  GUEST_FD_INVALID = -1,
  GUEST_FD_OOM,
} guest_fd_result_t;

struct guest_fd {
  int host_fd;
  int64_t errno;
  struct sockaddr_in addr;
  socklen_t socklen;
};

static struct guest_fd guest_fds[MAX_CONNECTIONS];
static freelist_t guest_fd_fl = NULL;

static guest_fd_result_t get_guest_fd(int i, struct guest_fd *out) {
  if (i < 0 || i >= MAX_CONNECTIONS) return GUEST_FD_INVALID;
  *out = guest_fds[i];
  return GUEST_FD_OK;
}

static guest_fd_result_t guest_fd_preallocate(unsigned int *fd) {
  freelist_result_t ans = freelist_next(guest_fd_fl, &fd);
  if (ans == FREELIST_FULL)
    return GUEST_FD_OOM;
  return GUEST_FD_OK;
}

static guest_fd_result_t guest_fd_delete(int i) {
  if (i < 0 || i >= MAX_CONNECTIONS) return GUEST_FD_INVALID;
  guest_fds[i].host_fd = (struct guest_fd){ .host_fd = -1, .errno = 0, .addr = 0 };
  return GUEST_FD_OK;
}

static guest_fd_result_t guest_fd_errno(int i, int64_t *errno) {
  if (i < 0 || i >= MAX_CONNECTIONS) return GUEST_FD_INVALID;
  *errno = guest_fds[i].errno;
  return GUEST_FD_OK;
}

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


// sig : (fd: i32) -> i32
wasm_trap_t* host_accept(void *env, wasmtime_caller_t *caller,
                         const wasmtime_val_t *args, size_t nargs,
                         wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 1);
  assert(nresults == 1);

  // Unpack the argument.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);

  // Get host socket.
  struct guest_fd guest;
  if (get_guest_fd((int)guest_fd, &guest) == GUEST_FD_INVALID)
    return WASMTIME_TRAP_NEW("[host_accept] Invalid guest file descriptor"); // TODO(dhil): Gracefully return rather than trapping

  // Preallocate the guest socket.
  unsigned int i;
  if (guest_fd_preallocate(&i) == GUEST_FD_OOM)
    return WASMTIME_TRAP_NEW("[host_accept] Guest FD table is full"); // TODO(dhil): Gracefully return rather than trapping

  // Perform the system call.
  int ans = accept(guest.host_fd, (struct sockaddr *)&guest_fds[i].addr, &guest_fds[i].socklen);

  if (ans < 0) {
    // Error
    guest_fds[guest_fd].errno = errno;
    // Pack and return the result
    results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
    return NULL;
  } else {
    // Success
    guest_fds[i].host_fd = ans;
    (void)fcntl(ans, F_SETFL, fcntl(ans, F_GETFL, 0) | O_NONBLOCK); // TODO(dhil): Check status.
    results[0] = wasmtime_val_t_of_int32_t((int32_t)i);
    return NULL;
  }
}

// sig : (fd: i32, buf: i32, len: i32) -> i32
wasm_trap_t* host_recv(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 3);
  assert(nresults == 1);

  // Unpack the arguments.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);
  int32_t buf_offset = int32_t_of_wasmtime_val_t(args[1]);
  int32_t buf_len = int32_t_of_wasmtime_val_t(args[2]);

  // Get the host socket.
  struct guest_fd guest;
  if (get_guest_fd((int)guest_fd, &guest) == GUEST_FD_INVALID)
    return WASMTIME_TRAP_NEW("[host_recv] Invalid guest file descriptor"); // TODO(dhil): Gracefully return rather than trapping

  // Get the linear memory.
  if (!WASMTIME_CALLER_EXPORT_GET(caller, "memory", &memory_extern))
    return WASMTIME_TRAP_NEW("[host_recv] cannot load memory");
  uint8_t *linmem = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory);

  // Perform the system call.
  int ans = recv(guest.host_fd, linmem, (size_t)buf_len, 0);

  if (ans < 0) {
    // Error
    guest_fds[guest_fd].errno = errno;
  }
  results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
  return NULL;
}

// sig : (fd: i32, buf: i32, len: i32) -> i32
wasm_trap_t* host_send(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 3);
  assert(nresults == 1);

  // Unpack the arguments.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);
  int32_t buf_offset = int32_t_of_wasmtime_val_t(args[1]);
  int32_t buf_len = int32_t_of_wasmtime_val_t(args[2]);

  // Get the host socket.
  struct guest_fd guest;
  if (get_guest_fd((int)guest_fd, &guest) == GUEST_FD_INVALID)
    return WASMTIME_TRAP_NEW("[host_send] Invalid guest file descriptor"); // TODO(dhil): Gracefully return rather than trapping

  // Get the linear memory.
  if (!WASMTIME_CALLER_EXPORT_GET(caller, "memory", &memory_extern))
    return WASMTIME_TRAP_NEW("[host_send] cannot load memory");
  uint8_t *linmem = wasmtime_memory_data(wasmtime_caller_context(caller), &memory_extern.of.memory);

  // Perform the system call.
  int ans = send(guest.host_fd, linmem, (size_t)buf_len, 0);

  if (ans < 0) {
    // Error
    guest_fds[guest_fd].errno = errno;
  }
  results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
  return NULL;
}

// sig : (fd: i32) -> i32
wasm_trap_t* host_close(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults) {
  assert(nargs == 1);
  assert(nresults == 1);

  // Unpack the argument.
  int32_t guest_fd = int32_t_of_wasmtime_val_t(args[0]);

  // Get the guest descriptor.
  struct guest_fd guest;
  if (get_guest_fd(guest_fd, &guest) == GUEST_FD_INVALID)
    return WASMTIME_TRAP_NEW("[host_close] invalid guest file descriptor");

  // Perform the system call.
  int ans = close(sock.host_fd);

  // Pack and return the result.
  results[0] = wasmtime_val_t_of_int32_t((int32_t)ans);
  return NULL;
}

/* static inline wasm_functype_t* wasm_functype_new_4_1( wasm_valtype_t *arg1
/*                                                     , wasm_valtype_t *arg2 */
/*                                                     , wasm_valtype_t *arg3 */
/*                                                     , wasm_valtype_t *arg4 */
/*                                                     , wasm_valtype_t *res ) { */
/*   wasm_valtype_t* ps[4] = {arg1, arg2, arg3, arg4}; */
/*   wasm_valtype_t* rs[1] = {res}; */
/*   wasm_valtype_vec_t params, results; */
/*   wasm_valtype_vec_new(&params, 4, ps); */
/*   wasm_valtype_vec_new(&results, 1, rs); */
/*   return wasm_functype_new(&params, &results); */
/* } */

wasmtime_error_t* wasmtime_socket_linker_define(wasmtime_linker_t *linker, wasmtime_context_t *context) {
  assert(!initialised);
  wasmtime_error_t *error = NULL;

  accept_sig = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, accept_sig, host_accept, NULL, NULL, &accept_func);
  accept_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = accept_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "accept", &accept_extern);
  if (error != NULL) return error;

  recv_sig = wasm_functype_new_3_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, recv_sig, host_recv, NULL, NULL, &recv_func);
  recv_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = recv_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "recv", &recv_extern);
  if (error != NULL) return error;

  send_sig = wasm_functype_new_3_1(wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, send_sig, host_send, NULL, NULL, &send_func);
  send_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = send_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "send", &send_extern);
  if (error != NULL) return error;

  close_sig = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  wasmtime_func_new(context, send_sig, host_send, NULL, NULL, &send_func);
  close_extern = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = close_func } };
  error = WASMTIME_LINKER_DEFINE(linker, context, "socket", "close", &send_extern);
  if (error != NULL) return error;

  assert(freelist_new(MAX_CONNECTIONS, &guest_fd_fl) == FREELIST_OK);

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
  freelist_delete(guest_fd_fl);
  initialised = false;
}
