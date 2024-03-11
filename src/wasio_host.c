#include <wasio.h>
#include <wasm_utils.h>

extern
__wasm_import__("host", "host_poll")
int host_poll(void *fds, size_t len);

int wasio_poll(struct wasio_pollfd *fds, size_t len) {
  return host_poll((void*)fds, len);
}

extern
__wasm_import__("host", "host_accept")
int host_accept(int fd, void *addr, void *addr_len);

int wasio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len) {
  return host_accept(fd, (void*)addr, (void*)addr_len);
}

extern
__wasm_import__("host", "host_recv")
int host_recv(int fd, void *buf, size_t len);

int wasio_recv(int fd, char *buf, size_t len) {
  return host_recv(fd, (void*)buf, len);
}

extern
__wasm_import__("host", "host_send")
int host_send(int fd, void *buf, size_t len);

int wasio_send(int fd, char *buf, size_t len) {
  return host_send(fd, (void*)buf, len);
}

extern
__wasm_import__("host", "host_close")
int host_close(int fd);

int wasio_close(int fd) {
  return host_close(fd);
}
