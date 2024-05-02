#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <assert.h>
#include <freelist.h>
#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

#if WASIO_BACKEND == 1
#include <poll.h>
struct wasio_event {
  uint64_t _phantom;
};
#define WASIO_EVENT_FOREACH(wfd, /* ignored */ evs, num_events, vfd, BODY) \
  { (void)evs; \
    for (uint32_t i = 0, j = 0; i < (wfd)->length && j < num_events; i++) { \
      if ((wfd)->vfds[i].fd < 0) continue; /* TODO(dhil): we should check revents here */  \
      { (wfd)->vfds[i].fd = -1; int64_t vfd = (int64_t)i; j++; BODY } \
    }\
  }
#define WASIO_EVENT_INITIALISER(_max_events) NULL
#elif WASIO_BACKEND == 2
struct pollfd {
  int32_t fd;
  short int events;
  short int revents;
} __attribute__((packed));
static_assert(sizeof(struct pollfd) == 8, "size of struct pollfd");
static_assert(offsetof(struct pollfd, fd) == 0, "offset of fd");
static_assert(offsetof(struct pollfd, events) == 4, "offset of events");
static_assert(offsetof(struct pollfd, revents) == 6, "offset of revents");

struct wasio_event {
  uint64_t _phantom;
};

#define WASIO_EVENT_FOREACH(wfd, /* ignored */ evs, num_events, vfd, BODY) \
  { (void)evs; \
    for (uint32_t i = 0, j = 0; i < (wfd)->length && j < num_events; i++) { \
      if ((wfd)->vfds[i].fd >= 0 && (wfd)->vfds[i].revents != 0) { \
        (wfd)->vfds[i].events = 0; (wfd)->vfds[i].revents = 0; int64_t vfd = (int64_t)i; j++; BODY \
      } \
    } \
  }
#define WASIO_EVENT_INITIALISER(_max_events) NULL
#else
#error "unsupported backend"
#endif

struct wasio_pollfd {
  uint32_t capacity;
  uint32_t length;
  freelist_t fl;
  int32_t *fds;
  struct pollfd *vfds;
};
static_assert(sizeof(int) == 4, "size of int");
static_assert(sizeof(int32_t) == 4, "size of int32_t");
static_assert(sizeof(struct wasio_pollfd) == 20, "size of struct wasio_pollfd");
static_assert(offsetof(struct wasio_pollfd, capacity) == 0, "offset of capacity");
static_assert(offsetof(struct wasio_pollfd, length) == 4, "offset of length");
static_assert(offsetof(struct wasio_pollfd, fl) == 8, "offset of fl");
static_assert(offsetof(struct wasio_pollfd, fds) == 12, "offset of fds");
static_assert(offsetof(struct wasio_pollfd, vfds) == 16, "offset of vfds");


// Virtual file descriptor.
typedef int32_t wasio_fd_t;

typedef enum {
  WASIO_OK = 0,
  WASIO_ERROR = 1,
  WASIO_EFULL = 2,
  WASIO_EAGAIN = 3,
} wasio_result_t;

extern
__wasm_export__("wasio_listen")
wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd, uint32_t port, uint32_t backlog);

extern
__wasm_export__("wasio_wrap")
wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int32_t preopened_fd, wasio_fd_t /* out */ *vfd);

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
__wasm_export__("wasio_notify_recv")
wasio_result_t wasio_notify_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd);

extern
__wasm_export__("wasio_notify_send")
wasio_result_t wasio_notify_send(struct wasio_pollfd *wfd, wasio_fd_t vfd);
#endif
