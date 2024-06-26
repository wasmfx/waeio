/* Needs capability: wasmtime run -S preview2=n -S tcplisten=127.0.0.1:9000 --env 'LISTEN_FDS=1' tcp_listenfd_server.wasm */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <freelist.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasio.h>

wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int64_t preopened_fd, wasio_fd_t /* out */ *vfd) {
  uint32_t entry;
  if (freelist_next(wfd->fl, &entry) != FREELIST_OK)
    return WASIO_EFULL;
  wfd->vfds[entry].fd = -1;
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
  for (uint32_t i = 0; i < capacity; i++) {
    wfd->vfds[i] = (struct pollfd){ .fd = -1, .events = POLLIN | POLLOUT, .revents = 0 };
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
  int ans = poll(wfd->vfds, (nfds_t)wfd->capacity, (int)timeout);
  if (ans < 0) return WASIO_ERROR;
  *evlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t /* out */ *new_conn_vfd) {
  int ans = (int)wasio_wrap(wfd, -1, new_conn_vfd);
  if (ans < 0) return WASIO_ERROR;

  int fd = wfd->fds[vfd];
  ans = accept(fd, NULL, 0);
  if (ans >= 0)
    wfd->vfds[(uint32_t)*new_conn_vfd].fd = ans;
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen) {
  int fd = (int)wfd->fds[vfd];
  int ans = recv(fd, buf, (size_t)len, 0);
  if (ans < 0) return WASIO_ERROR;
  *recvlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen) {
  int fd = wfd->vfds[vfd].fd;
  int ans = send(fd, buf, (size_t)len, 0);
  if (ans < 0) return WASIO_ERROR;
  *sendlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  if (close(wfd->vfds[vfd].fd) != 0) return WASIO_ERROR;
  wfd->vfds[vfd].fd = -1;
  wfd->fds[vfd] = -1;
  wfd->length--;
  assert(freelist_reclaim(wfd->fl, vfd) == FREELIST_OK);
  return WASIO_OK;
}

wasio_result_t wasio_mark_ready(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  wfd->vfds[vfd].fd = (int)wfd->fds[vfd];
  return WASIO_OK;
}

/* wasio_result_t wasio_new(struct wasio_fd *wfd, uint32_t max_fds, int64_t *preopened_sock, void *data) { */
/*   if (freelist_new(max_fds, &wfd->fl) == FREELIST_SIZE_ERR) return WASIO_ERR; */
/*   wfd->fds = (struct pollfd*)malloc(sizeof(struct pollfd)*max_fds); */
/*   wfd->data  = (struct wasio_data*)malloc(sizeof(struct wasio_data)*max_fds); */
/*   for (int i = 0; i < max_fds; i++) { */
/*     wdf->fds[i].fd = -1; */
/*     wfd->data[i] = (struct wasio_data){ .data = NULL, errno = 0 }; */
/*   } */
/*   wfd->capacity = max_fds; */
/*   wfd->npending_events = 0; */
/*   uint32_t entry; */
/*   assert(FREELIST_NEXT(fl, &entry) == FREELIST_OK); */
/*   wfd->fds[entry].fd = (int)*preopened_sock; */
/*   wfd->fds[entry].events = POLLIN | POLLOUT; */
/*   wfd->data[entny].data = data; */
/*   *preopened_sock = (int64_t)entry; */
/*   return WASIO_OK; */
/* } */

/* void wasio_delete(struct wasio_fd *wfd) { */
/*   if (wfd == NULL) return; */
/*   free(wfd->fds); */
/*   free(wfd->data); */
/*   freelist_delete(wfd->fl); */
/*   wfd = NULL; */
/* } */

/* wasio_result_t wasio_poll(struct wasio_fd *wfd, struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int timeout) { */
/*   int num_ready = wfd->npending_events; */
/*   if (num_ready == 0) { */
/*     num_ready = poll(fds, (nfds_t)wfd->capacity, timeout); */
/*     if (num_ready < 0) return WASIO_ERROR; */
/*   } */
/*   int evi = 0; */
/*   for (int i = 0; i < wfd->capacity && evi < max_events; i++) { */
/*     if (wfd->fds[i].revents & (POLLIN | POLLOUT)) { */
/*       events[evi].fd = (int64_t)wfd->fds[i].fd; */
/*       events[evi++].data = wfd->data[i].data; */
/*     } */
/*   } */
/*   *num_events = evi; */
/*   if (evi < num_ready) wfd->npending_events = num_ready - evi; */
/*   return WASIO_OK; */
/* } */

/* wasio_result_t wasio_attach(struct wasio_fd *wfd, int64_t fd, void *data) { */
/*   uint32_t entry = (uint32_t)fd; */
/*   wfd->data[entry].data = data; */
/*   return WASIO_OK; */
/* } */

/* int32_t wasio_error(struct wasio_fd *wfd, int64_t fd) { */
/*   uint32_t entry = (uint32_t)fd; */
/*   return wfd->data[entry].errno; */
/* } */

/* wasio_result_t wasio_accept(struct wasio_fd *wfd, int64_t *fd) { */
/*   uint32_t entry; */
/*   assert(freelist_next(fl, &entry) == FREELIST_OK); */
/*   *fd = (int64_t)entry; */
/*   int ans = accept(fd, NULL, 0); */
/*   if (ans < 0) { */
/*     wfd->data[entry].errno = errno; */
/*     return WASIO_ERR; */
/*   } */
/*   wfd->fds[entry].fd = (int64_t)ans; */
/*   wfd->fds[entry].events = POLLIN | POLLOUT; */
/*   if (ans < 0 || fcntl(ans, F_SETFL, fcntl(ans, F_GETFL, 0) | O_NONBLOCK)) { */
/*     wfd->data[entry].errno = errno; */
/*     return WASIO_ERR; */
/*   } */
/*   return WASIO_OK; */
/* } */

/* wasio_result_t wasio_recv(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *read) { */
/*   uint32_t entry = (uint32_t)fd; */
/*   *read = recv((int)wfd->fds[entry].fd, buf, len, 0); */
/*   if (*read < 0) { */
/*     wfd->data[entry].errno = errno; */
/*     return WASIO_ERR; */
/*   } */
/*   return WASIO_OK; */
/* } */

/* wasio_result_t wasio_send(struct wasio_fd *wfd, int64_t fd, uint8_t *buf, size_t len, size_t *sent) { */
/*   uint32_t entry = (uint32_t)fd; */
/*   *sent = send((int)wfd->fds[entry].fd, buf, len, 0); */
/*   if (*sent < 0) { */
/*     wfd->data[entry].errno = errno; */
/*     return WASIO_ERR; */
/*   } */
/*   return WASIO_OK; */
/* } */

/* wasio_result_t wasio_close(struct wasio_fd *wfd, int64_t fd) { */
/*   uint32_t entry = (uint32_t)fd; */
/*   assert(freelist_reclaim(fl, entry) == FREELIST_OK); */
/*   wfd->fds[entry].fd = -1; */
/*   wfd->data[entry].data = NULL; */
/*   if (close(actual_fd) != 0) { */
/*     wfd->data[entry].errno = errno; */
/*     return WASIO_ERR; */
/*   } */
/*   return WASIO_OK; */
/* } */
