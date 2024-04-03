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

struct wasio_data {
  void *data;
  int32_t errno;
};

/* struct wasio_fd { */
/*   uint32_t vfd; */
/* }; */

struct wasio_ctl {
  uint32_t npending_events;
  uint32_t residency;
  uint32_t capacity;
  freelist_t fl;
  struct pollfd *fds;
  struct wasio_data *data;
};

static struct wasio_ctl ctl = {0};

wasio_result_t wasio_wrap(struct wasio_fd *wfd, int64_t preopened_fd) {
  if (ctl.residency == ctl.capacity) {
    ctl.capacity *= 2;
    ctl.fds = realloc(ctl.fds, sizeof(struct pollfd)*ctl.capacity);
    ctl.data = realloc(ctl.data, sizeof(struct wasio_data)*ctl.capacity);
    assert(freelist_resize(&ctl.fl, ctl.capacity) == FREELIST_OK);
    for (uint32_t i = ctl.residency; i < ctl.capacity; i++) {
      ctl.fds[i].fd = -1;
    }
  }
  uint32_t vfd;
  assert(freelist_next(ctl.fl, &vfd) == FREELIST_OK);
  ctl.fds[vfd] = (struct pollfd){ .fd = (int)preopened_fd, .events = POLLIN | POLLOUT, .revents = 0 };
  ctl.data[vfd] = (struct wasio_data){ .data = NULL, .errno = 0 };
  *wfd = (struct wasio_fd){ .vfd = vfd };
  ctl.residency += 1;
  return WASIO_OK;
}

wasio_result_t wasio_init(uint32_t hint_max_fds) {
  ctl.npending_events = 0;
  ctl.residency = 0;
  ctl.capacity *= hint_max_fds;
  ctl.fds = (struct pollfd*)malloc(sizeof(struct pollfd)*ctl.capacity);
  ctl.data = (struct wasio_data*)malloc(sizeof(struct wasio_data)*ctl.capacity);
  assert(freelist_new(ctl.capacity, &ctl.fl) == FREELIST_OK);
  return WASIO_OK;
}

void wasio_finalize(void) {
  freelist_delete(ctl.fl);
  free(ctl.fds);
  free(ctl.data);
  ctl.residency = 0;
  ctl.capacity = 0;
}

wasio_result_t wasio_attach(struct wasio_fd *wfd, void *data) {
  ctl.data[wfd->vfd].data = data;
  return WASIO_OK;
}

wasio_result_t wasio_poll(struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int32_t timeout) {
  if (ctl.npending_events > 0) {
    uint32_t j = 0;
    for (uint32_t i = 0; i < ctl.capacity && j < max_events; i++) {
      if (ctl.fds[i].revents & (POLLIN | POLLOUT)) {
        ctl.fds[i].revents = 0;
        events[j++] = (struct wasio_event){ .data = ctl.data[i].data };
        ctl.npending_events--;
      }
    }
    *num_events = j;
    return WASIO_OK;
  }

  int ans = poll(ctl.fds, ctl.residency, (int)timeout);
  if (ans < 0) {
    return WASIO_ERR;
  }

  uint32_t j = 0;
  for (uint32_t i = 0; i < ctl.capacity && j < max_events; i++) {
    if (ctl.fds[i].revents & (POLLIN | POLLOUT)) {
      ctl.fds[i].revents = 0;
      events[j++] = (struct wasio_event){ .data = ctl.data[i].data };
    }
  }
  *num_events = j;
  ctl.npending_events = ans - j;
  return WASIO_OK;
}

int32_t wasio_error(struct wasio_fd wfd) {
  return ctl.data[wfd.vfd].errno;
}

wasio_result_t wasio_accept(struct wasio_fd wfd, struct wasio_fd *new_conn) {
  int fd = ctl.fds[wfd.vfd].fd;
  int ans = accept(fd, NULL, 0);
  if (ans < 0) {
    ctl.data[wfd.vfd].errno = errno;
    return WASIO_ERR;
  }
  assert(wasio_wrap(new_conn, (int64_t)ans) == WASIO_OK);
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_fd wfd, uint8_t *buf, uint32_t len, uint32_t *recvlen) {
  int fd = ctl.fds[wfd.vfd].fd;
  int32_t ans = (int32_t)recv(fd, buf, (size_t)len, 0);
  if (ans < 0) {
    ctl.data[wfd.vfd].errno = errno;
    return WASIO_ERR;
  }
  *recvlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_fd wfd, uint8_t *buf, uint32_t len, uint32_t *sendlen) {
  int fd = ctl.fds[wfd.vfd].fd;
  int32_t ans = (int32_t)send(fd, buf, (size_t)len, 0);
  if (ans < 0) {
    ctl.data[wfd.vfd].errno = errno;
    return WASIO_ERR;
  }
  *sendlen = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_fd wfd) {
  if (close(ctl.fds[wfd.vfd].fd) != 0) {
    ctl.data[wfd.vfd].errno = errno;
    return WASIO_ERR;
  }
  ctl.fds[wfd.vfd].fd = -1;
  ctl.data[wfd.vfd] = (struct wasio_data){ .data = NULL, .errno = 0 };
  ctl.residency -= 1;
  assert(freelist_reclaim(ctl.fl, wfd.vfd) == FREELIST_OK);
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
