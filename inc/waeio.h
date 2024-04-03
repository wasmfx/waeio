#ifndef WAEIO_H
#define WAEIO_H

#include <wasio.h>
#include <wasm_utils.h>

__wasm_export__("waeio_main")
int waeio_main(void* (*main)(void*), void*);

__wasm_export__("waeio_async")
int waeio_async(void *(*proc)(void*), void*);

__wasm_export__("waeio_accept")
int waeio_accept(int fd);

__wasm_export__("waeio_recv")
int waeio_recv(int fd, char *buf, size_t len);

__wasm_export__("waeio_send")
int waeio_send(int fd, char *buf, size_t len);

__wasm_export__("waeio_close")
int waeio_close(int fd);

#endif
