#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

#define FIBER_KILL_SIGNAL INT_MIN

/* enum wasio_events { */
/*   WASIO_POLLIN = POLLIN, */
/*   WASIO_POLLOUT = POLLOUT */
/* }; */

/* struct wasio_pollfd { */
/*   int fd; */
/*   short events; */
/*   short revents; */
/* }; */

struct wasio_fd;

typedef enum {
  WASIO_OK,
  WASIO_ERR
} wasio_result_t;

typedef enum {
  WASIO_ADD,
  WASIO_MOD,
  WASIO_DEL,
} wasio_ctl_t;


struct wasio_event {
  int64_t fd;
  void *data;
};

extern
__wasm_export__("wasio_new")
wasio_result_t wasio_new(struct wasio_fd *wfd);

extern
__wasm_export__("wasio_delete")
wasio_result_t wasio_delete(struct wasio_fd *wfd);

extern
__wasm_export__("wasio_ctl")
wasio_result_t wasio_ctl(struct wasio_fd *wfd, wasio_clt_t cmd, int64_t sockfd, void *data);

extern
__wasm_export__("wasio_pool")
wasio_result_t wasio_poll(struct wasio_fd *wfd, int timeout, struct wasio_event *events, uint32_t *num_events);

extern
__wasm_export__("wasio_error")
int32_t wasio_error(struct wasio_fd *wfd, int64_t sockfd);

extern
__wasm_export__("wasio_accept")
wasio_result_t wasio_accept(struct wasio_fd *wfd, int64_t sockfd);

extern
__wasm_export__("wasio_recv")
wasio_result_t wasio_recv(struct wasio_fd *wfd, int64_t sockfd, uint8_t *buf, size_t len);

extern
__wasm_export__("wasio_send")
wasio_result_t wasio_send(struct wasio_fd *wfd, int64_t sockfd, uint8_t *buf, size_t len);

extern
__wasm_export__("wasio_close")
wasio_result_t wasio_close(struct wasio_fd *wfd, int64_t sockfd);

#endif
