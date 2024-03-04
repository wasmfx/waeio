#ifndef WAEIO_WAISO_H
#define WAEIO_WAISO_H

#include <sys/socket.h>
#include <wasm_utils.h>

extern
__wasm_export__("wasio_accept")
int wasio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len);

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
