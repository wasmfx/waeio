#include <sys/socket.h>
#include <unistd.h>

int waio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len) {
  return accept(fd, addr, addr_len, 0);
}

int waio_recv(int fd, char *buf, size_t len) {
  return recv(fd, buf, len, 0);
}

int waio_send(int fd, char *buf, size_t len) {
  return send(fd, buf, len, 0);
}

int waio_close(int fd) {
  return close(fd);
}
