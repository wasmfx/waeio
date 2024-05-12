// A host based implementation of WASIO.

#include <assert.h>
#include <host/errno.h>
#include <host/poll.h>
#include <host/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <wasio.h>

static inline wasio_result_t translate_error(int32_t errno) {
  if (errno == HOST_EAGAIN || errno == HOST_EWOULDBLOCK) {
    //printf("[translate_error] errno = %d, WASIO_EAGAIN\n", errno);
    return WASIO_EAGAIN;
  } else {
    return WASIO_ERROR;
  }
}

wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd, int32_t port, int32_t backlog) {
  if (!(wfd->length < wfd->capacity)) return WASIO_EFULL;
  int32_t fd = host_listen(port, backlog, &host_errno);
  if (fd < 0) return translate_error(host_errno);
  wfd->fds[wfd->length++] = (struct pollfd) { .fd = fd, .events = POLLIN, .revents = 0 };
}

wasio_result_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity) {
  wfd->fds = (struct wasio_pollfd*)malloc(sizeof(struct wasio_pollfd)*capacity);
  if (wfd->fds == NULL) return WASIO_ERROR;
  wfd->capacity = capacity;
  wfd->length = 0;
  return WASIO_OK;
}

void wasio_finalize(struct wasio_pollfd *wfd) {
  free(wfd->fds);
  wfd->length = 0;
  wfd->capacity = 0;
}

wasio_result_t wasio_poll(struct wasio_pollfd *wfd, uint32_t *nready, int32_t timeout) {
  int ans = host_poll(wfd->fds, wfd->length, timeout, &host_errno);
  if (ans < 0) return translate_error(host_errno);
  *nready = (uint32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t *new_conn) {
  if (!(wfd->length < wfd->capacity)) return WASIO_EFULL;
  int ans = host_accept(vfd, &host_errno);
  if (ans < 0) return translate_error(host_errno);
  wfd->fds[wfd->length++] = (struct pollfd) { .fd = (int32_t)ans, .events = POLLIN, .revents = 0 };
  *new_conn = (int32_t)ans;
  return WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen);

wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen);

wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd);

