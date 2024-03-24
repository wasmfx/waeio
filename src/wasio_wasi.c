#include <assert.h>
#include <fcntl.h>
#include <freelist.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasio.h>

struct wasio_fd {
  freelist_t fl;
  struct pollfd *fds;
  struct wasio_events *events;
};

wasio_result_t wasio_new(struct wasio_fd *wfd, uint32_t max_fds) {
  for (int i = 0; i < MAX_CONNECTIONS; i++)
    fds[i].fd = -1;
  *wfd = 0;
  return WASIO_OK;
}

wasio_result_t wasio_delete(struct wasio_fd *wfd) {
  for (int i = 0; i < MAX_CONNECTIONS; i++)
    fds[i].fd = -1;
  freelist_delete(fl);
  return WASIO_OK;
}

wasio_result_t wasio_poll(struct wasio_fd *wfd, int timeout) {
  int num_ready = poll(fds, (fdds_t)MAX_CONNECTIONS, timeout);
  if (num_ready < 0) return WASIO_ERROR;
  else return WASIO_OK;
}

wasio_result_t wasio_ctl(struct wasio_fd *wfd, wasio_clt_t cmd, int64_t sockfd, void *data) {
  switch (cmd) {
  case WASIO_ADD: {
    uint32_t entry;
    assert(freelist_next(wfd->fl, &entry) == FREELIST_OK);
    wfd->fds[entry] = (struct pollfd) { .fd = (int)sockfd, .events = POLLIN | POLLOUT, .revents = 0 };
    break;
  }
  case WASIO_MOD:
    break;
  case WASIO_DEL:
    break;
  default:
    return WASIO_ERR;
  }

  return WASIO_OK;
}

int32_t wasio_error(int64_t sockfd) {
  return errno;
}

wasio_result_t wasio_accept(struct wasio_fd *wfd, int32_t *fd) {
  assert(freelist_next(fl, fd) == FREELIST_OK);
  int ans = accept(fd, NULL, 0);
  fds[fd].fd = ans;
  if (ans < 0 || fcntl(ans, F_SETFL, fcntl(ans, F_GETFL, 0) | O_NONBLOCK)) return WASIO_ERR;
  else return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *read) {
  *read = recv((int)fd, buf, len, 0);
  if (*read < 0) return WASIO_ERR;
  else return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *sent) {
  *sent = send((int)fd, buf, len, 0);
  if (*sent < 0) return WASIO_ERR;
  else return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_fd *wfd, int32_t fd) {
  assert(freelist_reclaim(fl, fd) == FREELIST_OK);
  int actual_fd = fds[fd];
  fds[fd].fd = -1;
  if (close(actual_fd) != 0) return WASIO_ERR;
  else return WASIO_OK;
}
