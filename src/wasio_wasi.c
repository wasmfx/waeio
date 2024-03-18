#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasio.h>

int wasio_flags(int flags) {
  int result = 0;
  if (flags & WASIO_POLLIN) result |= POLLIN;
  if (flags & WASIO_POLLOUT) result |= POLLOUT;
  return result;
}

int wasio_rflags(int flags) {
  int result = 0;
  if (flags & POLLIN) result |= WASIO_POLLIN;
  if (flags & POLLOUT) result |= WASIO_POLLOUT;
  return result;
}

int wasio_poll(struct wasio_pollfd *fds, size_t len) {
  return poll((struct pollfd*)fds, (nfds_t)len, -1 /* blocking */);
}

int wasio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len) {
  int ans = accept(fd, addr, addr_len);
  if (ans < 0) return ans;
  int status = fcntl(ans, F_SETFL, fcntl(ans, F_GETFL, 0) | O_NONBLOCK);
  if (status < 0) return status;
  else return ans;
}

int wasio_recv(int fd, char *buf, size_t len) {
  return recv(fd, buf, len, 0);
}

int wasio_send(int fd, char *buf, size_t len) {
  return send(fd, buf, len, 0);
}

int wasio_close(int fd) {
  return close(fd);
}
