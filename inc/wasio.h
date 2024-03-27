#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

#define FIBER_KILL_SIGNAL INT_MIN

struct wasio_fd; // embeds fd and fiber.
struct wasio_conn; // from accept.

extern
__wasm_export__("wasio_accept")
wasio_result_t wasio_accept(struct wasio_fd wfd, struct wasio_conn *conn);

extern
__wasm_export__("wasio_handle")
wasio_result_t wasio_handle(struct wasio_conn *conn, void* *(*handler)(struct wasio_fd *fd));



typedef enum {
  WASIO_OK,
  WASIO_ERR
} wasio_result_t;

struct wasio_event {
  int64_t fd;
  void *data;
};

extern
__wasm_export__("wasio_new")
wasio_result_t wasio_new(struct wasio_fd *wfd, uint32_t max_fds, int64_t *preopened_sock, void *data)

extern
__wasm_export__("wasio_delete")
void wasio_delete(struct wasio_fd *wfd);

extern
__wasm_export__("wasio_attach")
wasio_result_t wasio_attach(struct wasio_fd *wfd, int64_t fd, void *data);

extern
__wasm_export__("wasio_pool")
wasio_result_t wasio_poll(struct wasio_fd *wfd, struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int timeout);

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
