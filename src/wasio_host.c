#include <assert.h>
#include <host/errno.h>
#include <stdint.h>
#include <stdio.h>
#include <wasio.h>
#include <wasm_utils.h>

extern
__wasm_import__("host_socket", "listen")
int64_t host_listen(int32_t, int32_t, void*);

__attribute__((unused))
extern
__wasm_import__("host_socket", "connect")
int64_t host_connect(int32_t, int32_t, int32_t, void*);

extern
__wasm_import__("host_socket", "accept")
int64_t host_accept(int64_t, void*);

extern
__wasm_import__("host_socket", "recv")
int32_t host_recv(int64_t, uint8_t*, uint32_t, void*);

extern
__wasm_import__("host_socket", "send")
int32_t host_send(int64_t, uint8_t*, uint32_t, void*);

extern
__wasm_import__("host_socket", "close")
int32_t host_close(int64_t, void*);


wasio_result_t wasio_listen(struct wasio_pollfd *wfd, wasio_fd_t /* out */ *vfd) {
  (void)wfd;
  *vfd = (wasio_fd_t)host_listen(8080, 1, &host_errno);
  return *vfd < 0 ? WASIO_ERROR : WASIO_OK;
}

wasio_result_t wasio_wrap(struct wasio_pollfd *wfd, int64_t preopened_fd, wasio_fd_t /* out */ *vfd) {
  (void)wfd;
  (void)preopened_fd;
  (void)vfd;
  return WASIO_OK;
}

wasio_result_t wasio_init(struct wasio_pollfd *wfd, uint32_t capacity) {
  (void)wfd;
  (void)capacity;
  return WASIO_OK;
}

void wasio_finalize(struct wasio_pollfd *wfd) {
  (void)wfd;
}

wasio_result_t wasio_poll(struct wasio_pollfd *wfd, struct wasio_event *ev, uint32_t max_events, uint32_t *evlen, int32_t timeout) {
  (void)wfd;
  (void)ev;
  (void)max_events;
  (void)evlen;
  (void)timeout;
  return WASIO_OK;
}

wasio_result_t wasio_accept(struct wasio_pollfd *wfd, wasio_fd_t vfd, wasio_fd_t *new_conn) {
  (void)wfd;
  *new_conn = host_accept((int64_t)vfd, &host_errno);
  return *new_conn < 0 ? WASIO_ERROR : WASIO_OK;
}

wasio_result_t wasio_recv(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *recvlen) {
  (void)wfd;
  *recvlen = host_recv((int64_t)vfd, buf, len, &host_errno);
  return *recvlen < 0 ? WASIO_ERROR : WASIO_OK;
}

wasio_result_t wasio_send(struct wasio_pollfd *wfd, wasio_fd_t vfd, uint8_t *buf, uint32_t len, uint32_t *sendlen) {
  (void)wfd;
  (void)wfd;
  *sendlen = host_send((int64_t)vfd, buf, len, &host_errno);
  return *sendlen < 0 ? WASIO_ERROR : WASIO_OK;
}

wasio_result_t wasio_close(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  (void)wfd;
  return host_close((int64_t)vfd, &host_errno) < 0 ? WASIO_ERROR : WASIO_OK;
}

wasio_result_t wasio_mark_ready(struct wasio_pollfd *wfd, wasio_fd_t vfd) {
  (void)wfd;
  (void)vfd;
  return WASIO_OK;
}
