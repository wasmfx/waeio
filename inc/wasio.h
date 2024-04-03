#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <limits.h>
#include <stdint.h>
#include <wasm_utils.h>

struct wasio_fd {
  uint32_t vfd;
};

typedef enum {
  WASIO_OK,
  WASIO_ERR
} wasio_result_t;

struct wasio_event {
  void *data;
};

extern
__wasm_export__("wasio_wrap")
wasio_result_t wasio_wrap(struct wasio_fd *wfd, int64_t preopened_fd);

extern
__wasm_export__("wasio_init")
wasio_result_t wasio_init(uint32_t hint_max_fds);

extern
__wasm_export__("wasio_finalize")
void wasio_finalize(void);

extern
__wasm_export__("wasio_attach")
wasio_result_t wasio_attach(struct wasio_fd *wfd, void *data);

extern
__wasm_export__("wasio_pool")
wasio_result_t wasio_poll(struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int32_t timeout);

extern
__wasm_export__("wasio_error")
int32_t wasio_error(struct wasio_fd wfd);

extern
__wasm_export__("wasio_accept")
wasio_result_t wasio_accept(struct wasio_fd wfd, struct wasio_fd *new_conn);

extern
__wasm_export__("wasio_recv")
wasio_result_t wasio_recv(struct wasio_fd wfd, uint8_t *buf, uint32_t len, uint32_t *recvlen);

extern
__wasm_export__("wasio_send")
wasio_result_t wasio_send(struct wasio_fd wfd, uint8_t *buf, uint32_t len, uint32_t *sendlen);

extern
__wasm_export__("wasio_close")
wasio_result_t wasio_close(struct wasio_fd wfd);

#endif
