// Host-defined socket implementation

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <host/socket.h>
#include <host/wasmtime_utils.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wasm.h>
#include <wasmtime.h>

static_assert(sizeof(int) == 4, "size of int");
static_assert(sizeof(struct pollfd) == 8, "size of struct pollfd");
static_assert(offsetof(struct pollfd, fd) == 0, "offset of fd");
static_assert(offsetof(struct pollfd, events) == 4, "offset of events");
static_assert(offsetof(struct pollfd, revents) == 6, "offset of revents");

static wasm_functype_t *poll_sig = NULL;  // i32 i32 i32 i32 -> i32
static wasmtime_func_t pollfn;
static wasmtime_extern_t pollex;

static wasm_functype_t *pollin_sig = NULL;  // () -> i32
static wasmtime_func_t pollinfn;
static wasmtime_extern_t pollinex;

static wasm_functype_t *pollout_sig = NULL; // () -> i32
static wasmtime_func_t polloutfn;
static wasmtime_extern_t polloutex;

DEFINE_BINDING(host_poll) {
  assert(nargs == 4);
  assert(nresults == 1);

  // Unpack struct offset, length, and timeout
  uint32_t soffset = uint32_t_of_wasmtime_val_t(args[0]);
  uint32_t slen    = uint32_t_of_wasmtime_val_t(args[1]);
  int32_t timeout  = int32_t_of_wasmtime_val_t(args[2]);

  // Load the structure
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_poll");
  struct pollfd *pollfd = (struct pollfd*)(mem+soffset);

  // Perform the system call.
  int ans = poll(pollfd, (nfds_t)slen, (int)timeout);

  //printf("[host_poll] ans = %d, errno = %d, strerror = %s\n", ans, errno, strerror(errno));

  if (ans < 0) {
    WRITE_ERRNO("host_poll", 3);
  }

  return result1(results, wasmtime_val_t_of_int32_t((int32_t)ans));
}

DEFINE_BINDING(host_pollin) {
  assert(nargs == 0);
  assert(nresults == 1);
  return result1(results, wasmtime_val_t_of_int32_t((int32_t)POLLIN));
}

DEFINE_BINDING(host_pollout) {
  assert(nargs == 0);
  assert(nresults == 1);
  return result1(results, wasmtime_val_t_of_int32_t((int32_t)POLLOUT));
}

wasmtime_error_t* host_poll_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module) {
  wasmtime_error_t *error = NULL;

  if (poll_sig == NULL) {
    poll_sig = wasm_functype_new_4_1(NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32);
    wasmtime_func_new(context, poll_sig, host_poll, NULL, NULL, &pollfn);
    pollex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = pollfn } };
    LINK_HOST_FN("poll", pollex);
  }

  if (pollin_sig == NULL) {
    pollin_sig = wasm_functype_new_0_1(NEW_WASM_I32);
    wasmtime_func_new(context, pollin_sig, host_pollin, NULL, NULL, &pollinfn);
    pollinex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = pollinfn } };
    LINK_HOST_FN("pollin", pollinex);
  }

  if (pollout_sig == NULL) {
    pollout_sig = wasm_functype_new_0_1(NEW_WASM_I32);
    wasmtime_func_new(context, pollout_sig, host_pollout, NULL, NULL, &polloutfn);
    polloutex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = polloutfn } };
    LINK_HOST_FN("pollout", polloutex);
  }

  return error;
}

void host_poll_delete(void) {
  wasm_functype_delete(poll_sig);
  poll_sig = NULL;

  wasm_functype_delete(pollin_sig);
  pollin_sig = NULL;

  wasm_functype_delete(pollout_sig);
  pollout_sig = NULL;
}


