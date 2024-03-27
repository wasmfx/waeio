#include <assert.h>
#include <fcntl.h>
#include <freelist.h>
#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasio.h>

struct wasio_data {
  void *data;
  int32_t errno;
}

struct wasio_fd {
  uint32_t npending_events;
  uint32_t capacity;
  freelist_t fl;
  struct pollfd *fds;
  struct wasio_data *data;
};

wasio_result_t wasio_new(struct wasio_fd *wfd, uint32_t max_fds, int64_t *preopened_sock, void *data) {
  if (freelist_new(max_fds, &wfd->fl) == FREELIST_SIZE_ERR) return WASIO_ERR;
  wfd->fds = (struct pollfd*)malloc(sizeof(struct pollfd)*max_fds);
  wfd->data  = (struct wasio_data*)malloc(sizeof(struct wasio_data)*max_fds);
  for (int i = 0; i < max_fds; i++) {
    wdf->fds[i].fd = -1;
    wfd->data[i] = (struct wasio_data){ .data = NULL, errno = 0 };
  }
  wfd->capacity = max_fds;
  wfd->npending_events = 0;
  uint32_t entry;
  assert(FREELIST_NEXT(fl, &entry) == FREELIST_OK);
  wfd->fds[entry].fd = (int)*preopened_sock;
  wfd->fds[entry].events = POLLIN | POLLOUT;
  wfd->data[entny].data = data;
  *preopened_sock = (int64_t)entry;
  return WASIO_OK;
}

void wasio_delete(struct wasio_fd *wfd) {
  if (wfd == NULL) return;
  free(wfd->fds);
  free(wfd->data);
  freelist_delete(wfd->fl);
  wfd = NULL;
}

wasio_result_t wasio_poll(struct wasio_fd *wfd, struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int timeout) {
  int num_ready = wfd->npending_events;
  if (num_ready == 0) {
    num_ready = poll(fds, (nfds_t)wfd->capacity, timeout);
    if (num_ready < 0) return WASIO_ERROR;
  }
  int evi = 0;
  for (int i = 0; i < wfd->capacity && evi < max_events; i++) {
    if (wfd->fds[i].revents & (POLLIN | POLLOUT)) {
      events[evi].fd = (int64_t)wfd->fds[i].fd;
      events[evi++].data = wfd->data[i].data;
    }
  }
  *num_events = evi;
  if (evi < num_ready) wfd->npending_events = num_ready - evi;
  return WASIO_OK;
}

wasio_result_t wasio_attach(struct wasio_fd *wfd, int64_t fd, void *data) {
  uint32_t entry = (uint32_t)fd;
  wfd->data[entry].data = data;
  return WASIO_OK;
}

int32_t wasio_error(struct wasio_fd *wfd, int64_t fd) {
  uint32_t entry = (uint32_t)fd;
  return wfd->data[entry].errno;
}

wasio_result_t wasio_accept(struct wasio_fd *wfd, int64_t *fd) {
  uint32_t entry;
  assert(freelist_next(fl, &entry) == FREELIST_OK);
  *fd = (int64_t)entry;
  int ans = accept(fd, NULL, 0);
  if (ans < 0) {
    wfd->data[entry].errno = errno;
    return WASIO_ERR;
  }
  wfd->fds[entry].fd = (int64_t)ans;
  wfd->fds[entry].events = POLLIN | POLLOUT;
  if (ans < 0 || fcntl(ans, F_SETFL, fcntl(ans, F_GETFL, 0) | O_NONBLOCK)) {
    wfd->data[entry].errno = errno;
    return WASIO_ERR;
  }
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *read) {
  uint32_t entry = (uint32_t)fd;
  *read = recv((int)wfd->fds[entry].fd, buf, len, 0);
  if (*read < 0) {
    wfd->data[entry].errno = errno;
    return WASIO_ERR;
  }
  return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *sent) {
  uint32_t entry = (uint32_t)fd;
  *sent = send((int)wfd->fds[entry].fd, buf, len, 0);
  if (*sent < 0) {
    wfd->data[entry].errno = errno;
    return WASIO_ERR;
  }
  return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_fd *wfd, int64_t fd) {
  uint32_t entry = (uint32_t)fd;
  assert(freelist_reclaim(fl, entry) == FREELIST_OK);
  wfd->fds[entry].fd = -1;
  wfd->data[entry].data = NULL;
  if (close(actual_fd) != 0) {
    wfd->data[entry].errno = errno;
    return WASIO_ERR;
  }
  return WASIO_OK;
}
