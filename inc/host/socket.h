#ifndef WAEIO_HOST_SOCKET_H
#define WAEIO_HOST_SOCKET_H

#include <stdint.h>
#include <wasm_utils.h>

extern
__wasm_import__("host_socket", "listen")
int32_t host_listen(int32_t, int32_t, int32_t*);

extern
__wasm_import__("host_socket", "accept")
int32_t host_accept(int32_t, int32_t*);

extern
__wasm_import__("host_socket", "recv")
int32_t host_recv(int32_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "send")
int32_t host_send(int32_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "close")
int32_t host_close(int32_t, int32_t*);

#endif
