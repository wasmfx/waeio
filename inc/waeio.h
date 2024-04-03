#ifndef WAEIO_H
#define WAEIO_H

#include <stdint.h>
#include <wasio.h>
#include <wasm_utils.h>

__wasm_export__("waeio_main")
int waeio_main(void* (*main)(void*), void*);

__wasm_export__("waeio_async")
int waeio_async(void *(*proc)(struct wasio_fd*), struct wasio_fd *fd);

__wasm_export__("waeio_accept")
int waeio_accept(struct wasio_fd fd, struct wasio_fd *new_conn);

__wasm_export__("waeio_recv")
int waeio_recv(struct wasio_fd fd, uint8_t *buf, uint32_t len);

__wasm_export__("waeio_send")
int waeio_send(struct wasio_fd fd, uint8_t *buf, uint32_t len);

__wasm_export__("waeio_close")
int waeio_close(struct wasio_fd fd);

#endif
