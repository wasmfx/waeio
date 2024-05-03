#include <assert.h>
#include <host/errno.h>
#include <stdint.h>
#include <stdio.h>
#include <wasio.h>
#include <wasm_utils.h>

extern
__wasm_import__("host_socket", "listen")
int32_t host_listen(int32_t, int32_t, int32_t*);

__attribute__((unused))
extern
__wasm_import__("host_socket", "connect")
int32_t host_connect(int32_t, int32_t, int32_t, int32_t*);

extern
__wasm_import__("host_socket", "accept")
int32_t host_accept(int32_t, void*);

extern
__wasm_import__("host_socket", "recv")
int32_t host_recv(int32_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "send")
int32_t host_send(int32_t, uint8_t*, uint32_t, int32_t*);

extern
__wasm_import__("host_socket", "close")
int32_t host_close(int32_t, void*);

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

static inline wasio_result_t translate_error(int32_t errno) {
  if (errno == HOST_EAGAIN || errno == HOST_EWOULDBLOCK) {
    printf("[translate_error] errno = %d, WASIO_EAGAIN\n", errno);
    return WASIO_EAGAIN;
  } else {
    return WASIO_ERROR;
  }
}

wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd, uint32_t port, uint32_t backlog) {
  int32_t fd = host_listen(port, backlog, &host_errno);
  return fd < 0 ? translate_error(host_errno) : wasio_wrap(wfd, fd, vfd);
}

wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int32_t preopened_fd, wasio_fd_t /* out */ *vfd) {
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
  wfd->fds = (int32_t*)malloc(sizeof(int32_t)*capacity);
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
  if (ans < 0) return translate_error(host_errno);
  *evlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t *new_conn_vfd) {
  int fd = wfd->fds[vfd];
  int ans = host_accept((int32_t)fd, &host_errno);
  printf("[wasio_accept] conn_vfd = %d, physfd = %d, errno = %s (%d)\n", *new_conn_vfd, ans, host_strerror(host_errno), host_errno);
  if (ans < 0) return translate_error(host_errno);

  wasio_result_t res = wasio_wrap(wfd, ans, new_conn_vfd);
  if (res != WASIO_OK) return res;

  wfd->fds[(uint32_t)*new_conn_vfd] = ans;
  wfd->vfds[(uint32_t)*new_conn_vfd].fd = ans;
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen) {
  int fd = (int)wfd->fds[vfd];
  int ans = host_recv(fd, buf, len, &host_errno);
  if (ans < 0) return translate_error(host_errno);
  *recvlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen) {
  int fd = (int)wfd->fds[vfd];
  int ans = host_send(fd, buf, len, &host_errno);
  if (ans < 0) return translate_error(host_errno);
  *sendlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  if (host_close(wfd->vfds[vfd].fd, &host_errno) != 0) return translate_error(host_errno);
  wfd->vfds[vfd].fd = -1;
  wfd->vfds[vfd].revents = 0;
  wfd->fds[vfd] = -1;
  wfd->length--;
  assert(freelist_reclaim(wfd->fl, (uint32_t)vfd) == FREELIST_OK);
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
