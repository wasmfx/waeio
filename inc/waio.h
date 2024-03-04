#ifndef WAEIO_WAIO_H
#define WAEIO_WAIO_H

#include <sys/socket.h>
#include <wasm_utils.h>

extern
__wasm_export__("waio_accept")
int waio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len);

extern
__wasm_export__("waio_recv")
int waio_recv(int fd, char *buf, size_t len);

extern
__wasm_export__("waio_send")
int waio_send(int fd, char *buf, size_t len);

extern
__wasm_export__("waio_close")
int waio_close(int fd);

#endif
