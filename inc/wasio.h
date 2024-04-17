#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

#if WASIO_BACKEND == 1
#include <poll.h>
#include <freelist.h>
struct wasio_pollfd {
  uint32_t capacity;
  uint32_t length;
  freelist_t fl;
  int64_t *fds;
  struct pollfd *vfds;
};
static_assert(sizeof(struct wasio_pollfd) == 20, "size of struct wasio_pollfd");
static_assert(offsetof(struct wasio_pollfd, capacity) == 0, "offset of capacity");
static_assert(offsetof(struct wasio_pollfd, length) == 4, "offset of length");
static_assert(offsetof(struct wasio_pollfd, fl) == 8, "offset of fl");
static_assert(offsetof(struct wasio_pollfd, fds) == 12, "offset of fds");
static_assert(offsetof(struct wasio_pollfd, vfds) == 16, "offset of vfds");
struct wasio_event {
  uint64_t _phantom;
};
#define WASIO_EVENT_FOREACH(wfd, /* ignored */ evs, num_events, vfd, BODY) \
  { (void)evs; \
    for (uint32_t i = 0, j = 0; i < (wfd)->length && j < num_events; i++) { \
      if ((wfd)->vfds[i].fd < 0) continue; \
      { (wfd)->vfds[i].fd = -1; int64_t vfd = (int64_t)i; j++; BODY } \
    }\
  }
#define WASIO_EVENT_INITIALISER(_max_events) NULL
#else
#error "unsupported backend"
#endif

// Virtual file descriptor.
typedef uint64_t wasio_fd_t;

typedef enum {
  WASIO_OK,
  WASIO_ERROR,
  WASIO_EFULL,
} wasio_result_t;

extern
__wasm_export__("wasio_wrap")
wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int64_t preopened_fd, wasio_fd_t /* out */ *vfd);

extern
__wasm_export__("wasio_init")
wasio_result_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity);

extern
__wasm_export__("wasio_finalize")
void wasio_finalize(struct wasio_pollfd *wfd);

extern
__wasm_export__("wasio_pool")
wasio_result_t wasio_poll(struct wasio_pollfd *wfd, struct wasio_event *ev, uint32_t max_events, uint32_t *evlen, int32_t timeout);

extern
__wasm_export__("wasio_accept")
wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t *new_conn);

extern
__wasm_export__("wasio_recv")
wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen);

extern
__wasm_export__("wasio_send")
wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen);

extern
__wasm_export__("wasio_close")
wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd);

extern
__wasm_export__("wasio_mark_ready")
wasio_result_t wasio_mark_ready(struct wasio_pollfd *wfd, wasio_fd_t vfd);
#endif
