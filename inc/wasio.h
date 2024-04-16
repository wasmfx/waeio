#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

typedef enum {
  WASIO_EFULL = 64000,
} wasio_errno_t;

#if WASIO_BACKEND == 1
#include <poll.h>
#include <freelist.h>
struct wasio_pollfd {
  uint32_t capacity  __attribute__((aligned(8)));
  uint32_t length    __attribute__((aligned(8)));
  freelist_t fl      __attribute__((aligned(8)));
  struct pollfd *fds __attribute__((aligned(8)));
} __attribute__((packed));
static_assert(sizeof(struct wasio_pollfd) == 32, "size of struct wasio_pollfd");
static_assert(offsetof(struct wasio_pollfd, capacity) == 0, "offset of capacity");
static_assert(offsetof(struct wasio_pollfd, length) == 8, "offset of length");
static_assert(offsetof(struct wasio_pollfd, fl) == 16, "offset of fl");
static_assert(offsetof(struct wasio_pollfd, fds) == 24, "offset of fds");
struct wasio_event {
  uint64_t _phantom;
};
#define WASIO_EVENT_FOREACH(wfd, /* ignored */ evs, num_events, vfd, BODY) \
  { (void)evs; \
    for (uint32_t i = 0, j = 0; i < wfd->length && j < num_events; i++) { \
      if (wfd->fds[i].fd < 0) continue; \
      { uint64_t vfd = (uint64_t)i; j++; BODY } \
    }\
  }
#else
#error "unsupported backend"
#endif

// Virtual file descriptor.
typedef uint64_t wasio_fd_t;

struct wasio_event {
  void *data;
};

extern
__wasm_export__("wasio_wrap")
int32_t wasio_wrap(struct wasio_pollfd *wfd, int64_t preopened_fd, wasio_fd_t /* out */ *vfd);

extern
__wasm_export__("wasio_init")
int32_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity)

extern
__wasm_export__("wasio_finalize")
void wasio_finalize(struct wasio_pollfd *wfd);

extern
__wasm_export__("wasio_pool")
int32_t wasio_poll(struct wasio_pollfd *wfd, struct wasio_event *ev, uint32_t max_events, int32_t timeout);

extern
__wasm_export__("wasio_accept")
int32_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, struct wasio_fd *new_conn);

extern
__wasm_export__("wasio_recv")
int32_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen);

extern
__wasm_export__("wasio_send")
int32_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen);

extern
__wasm_export__("wasio_close")
int32_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd);

#endif
