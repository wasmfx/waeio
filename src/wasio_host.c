#include <assert.h>
#include <host/errno.h>
#include <stdint.h>
#include <stdio.h>
#include <wasio.h>
#include <wasm_utils.h>

extern
__wasm_import__("host_socket", "listen")
int64_t host_listen(int32_t, int32_t, int32_t*);

__attribute__((unused))
extern
__wasm_import__("host_socket", "connect")
int64_t host_connect(int32_t, int32_t, int32_t, int32_t*);

extern
__wasm_import__("host_socket", "accept")
int64_t host_accept(int64_t, void*);

extern
__wasm_import__("host_socket", "recv")
int32_t host_recv(int64_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "send")
int32_t host_send(int64_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "close")
int32_t host_close(int64_t, void*);

extern
__wasm_import__("host_poll", "poll")
int32_t host_poll(struct pollfd*, uint32_t, uint32_t, int32_t*);

extern
__wasm_import__("host_poll", "pollin")
int32_t host_pollin();

extern
__wasm_import__("host_poll", "pollin")
int32_t host_pollout();

static short int pollin = 0;
static short int pollout = 0;

wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd, uint32_t port, uint32_t backlog) {
  int64_t fd = host_listen(port, backlog, &host_errno);
  return fd < 0 ? WASIO_ERROR : wasio_wrap(wfd, fd, vfd);
}

wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int64_t preopened_fd, wasio_fd_t /* out */ *vfd) {
  uint32_t entry;
  if (freelist_next(wfd->fl, &entry) != FREELIST_OK)
    return WASIO_EFULL;
  wfd->vfds[entry].fd = (int)preopened_fd;
  wfd->fds[entry] = (int)preopened_fd;
  wfd->length++;
  *vfd = (wasio_fd_t)entry;

  return WASIO_OK;
}

wasio_result_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity) {
  if (freelist_new(capacity, &wfd->fl) != FREELIST_OK)
    return WASIO_EFULL;
  wfd->capacity = capacity;
  wfd->length = 0;
  wfd->vfds = (struct pollfd*)malloc(sizeof(struct pollfd)*capacity);
  wfd->fds = (int64_t*)malloc(sizeof(int64_t)*capacity);
  pollin = host_pollin();
  pollout = host_pollout();
  for (uint32_t i = 0; i < capacity; i++) {
    wfd->vfds[i] = (struct pollfd){ .fd = -1, .events = 0, .revents = 0 };
    wfd->fds[i] = -1;
  }
  return WASIO_OK;
}

void wasio_finalize(struct wasio_pollfd *wfd) {
  freelist_delete(wfd->fl);
  wfd->length = 0;
  free(wfd->vfds);
  free(wfd->fds);
}

wasio_result_t wasio_poll( struct wasio_pollfd *wfd
                         , struct wasio_event *ev __attribute__((unused))
                         , uint32_t max_events __attribute__((unused))
                         , uint32_t *evlen
                         , int32_t timeout ) {
  int ans = host_poll(wfd->vfds, wfd->capacity, (int)timeout, &host_errno);
  if (ans < 0) return WASIO_ERROR;
  *evlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t *new_conn_vfd) {
  int ans = (int)wasio_wrap(wfd, -1, new_conn_vfd);
  if (ans < 0) return WASIO_ERROR;

  int fd = wfd->fds[vfd];
  ans = host_accept((int64_t)fd, &host_errno);
  if (ans < 0) return WASIO_ERROR;

  wfd->fds[(uint32_t)*new_conn_vfd] = ans;
  wfd->vfds[(uint32_t)*new_conn_vfd].fd = ans;
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen) {
  int fd = (int)wfd->fds[vfd];
  int ans = host_recv(fd, buf, len, &host_errno);
  if (ans < 0) return WASIO_ERROR;
  *recvlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen) {
  int fd = (int)wfd->fds[vfd];
  int ans = host_send(fd, buf, len, &host_errno);
  if (ans < 0) return WASIO_ERROR;
  *sendlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  if (host_close(wfd->vfds[vfd].fd, &host_errno) != 0) return WASIO_ERROR;
  wfd->vfds[vfd].fd = -1;
  wfd->fds[vfd] = -1;
  wfd->length--;
  assert(freelist_reclaim(wfd->fl, vfd) == FREELIST_OK);
  return WASIO_OK;
}

wasio_result_t wasio_notify_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  wfd->vfds[vfd].fd = (int)wfd->fds[vfd];
  wfd->vfds[vfd].events |= pollin;
  return WASIO_OK;
}

wasio_result_t wasio_notify_send(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  wfd->vfds[vfd].fd = (int)wfd->fds[vfd];
  wfd->vfds[vfd].events |= pollout;
  return WASIO_OK;
}
