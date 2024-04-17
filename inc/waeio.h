#ifndef WAEIO_H
#define WAEIO_H

#include <stdint.h>
#include <wasio.h>
#include <wasm_utils.h>

__wasm_export__("waeio_main")
int waeio_main(void* (*main)(void*), void*);

__wasm_export__("waeio_async")
int waeio_async(void *(*proc)(wasio_fd_t*), wasio_fd_t vfd);

__wasm_export__("waeio_accept")
int waeio_accept(wasio_fd_t vfd, wasio_fd_t *new_conn);

__wasm_export__("waeio_recv")
int waeio_recv(wasio_fd_t vfd, uint8_t *buf, uint32_t len);

__wasm_export__("waeio_send")
int waeio_send(wasio_fd_t vfd, uint8_t *buf, uint32_t len);

__wasm_export__("waeio_close")
int waeio_close(wasio_fd_t vfd);

#endif
