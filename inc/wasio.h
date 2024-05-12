#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <wasm_utils.h>

#if WASIO_BACKEND == 1
#include <poll.h>
#define WASIO_POLLIN POLLIN
#define WASIO_POLLPRI POLLPRI
#define WASIO_POLLOUT POLLOUT
#define WASIO_POLLERR POLLERR
#define WASIO_POLLHUP POLLHUP
#define WASIO_POLLNVAL POLLNVAL
#elif WASIO_BACKEND == 2
#include <host/poll.h>
#define WASIO_POLLIN HOST_POLLIN
#define WASIO_POLLPRI HOST_POLLPRI
#define WASIO_POLLOUT HOST_POLLOUT
#define WASIO_POLLERR HOST_POLLERR
#define WASIO_POLLHUP HOST_POLLHUP
#define WASIO_POLLNVAL HOST_POLLNVAL
#else
#error "unsupported backend"
#endif

struct wasio_pollfd;

struct wasio_pollfd {
  uint32_t capacity;
  uint32_t length;
  struct pollfd *fds;
};

#define WASIO_STATIC_INITIALIZER(wfd, capacity) \
  static struct pollfd _wasio_fds[capacity]; \
  static struct wasio_pollfd (wfd) = (struct wasio_pollfd) { \
    .capacity = capacity, .length = 0, .fds = &_wasio_fds \
  };

static_assert(sizeof(int) == 4, "size of int");
static_assert(sizeof(int32_t) == 4, "size of int32_t");
static_assert(sizeof(struct pollfd*) == 4, "pointer width");
static_assert(sizeof(struct wasio_pollfd) == 12, "size of struct wasio_pollfd");
static_assert(offsetof(struct wasio_pollfd, capacity) == 0, "offset of capacity");
static_assert(offsetof(struct wasio_pollfd, length) == 4, "offset of length");
static_assert(offsetof(struct wasio_pollfd, vfds) == 8, "offset of fds");

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
wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd, int32_t port, int32_t backlog);

extern
__wasm_export__("wasio_init")
wasio_result_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity);

extern
__wasm_export__("wasio_finalize")
void wasio_finalize(struct wasio_pollfd *wfd);

extern
__wasm_export__("wasio_pool")
wasio_result_t wasio_poll(struct wasio_pollfd *wfd, uint32_t *nready, int32_t timeout);

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
#endif
