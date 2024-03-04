#include <fiber.h>
#include <unistd.h>
#include <wasio.h>

int waeio_run(void (*main)(void)) {
  return 0;
}

int waeio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len) {
  return 0;
}

int waeio_recv(int fd, char *buf, size_t len) {
  return 0;
}

int waeio_send(int fd, char *buf, size_t len) {
  return 0;
}

int waeio_close(int fd) {
  return close(fd);
}
