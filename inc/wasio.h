#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <limits.h>
#include <poll.h>
#include <sys/socket.h>
#include <wasm_utils.h>

#define FIBER_KILL_SIGNAL INT_MIN

enum wasio_events {
  WASIO_POLLIN = POLLIN,
  WASIO_POLLOUT = POLLOUT
};

struct wasio_pollfd {
  int fd;
  short events;
  short revents;
};

extern
__wasm_export__("wasio_flags")
int wasio_flags(int flags);

extern
__wasm_export__("wasio_rflags")
int wasio_rflags(int rflags);

extern
__wasm_export__("wasio_poll")
int wasio_poll(struct wasio_pollfd *fds, size_t len);

extern
__wasm_export__("wasio_accept")
int wasio_accept(int fd, struct sockaddr *addr, socklen_t *addr_len);

extern
__wasm_export__("wasio_recv")
int wasio_recv(int fd, char *buf, size_t len);

extern
__wasm_export__("wasio_send")
int wasio_send(int fd, char *buf, size_t len);

extern
__wasm_export__("wasio_close")
int wasio_close(int fd);

#endif
