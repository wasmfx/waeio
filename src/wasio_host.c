#include <assert.h>
#include <freelist.h>
#include <stdint.h>
#include <wasio.h>
#include <wasm_utils.h>

struct wasio_fd {
  int64_t epollfd;
  freelist_t fl;
  struct wasio_event evs;
};

enum wasio_epoll_ctl {
  WASIO_CTL_ADD = 1,
  WASIO_CTL_MOD = 2,
  WASIO_CTL_DEL = 3,
};

extern
__wasm_import__("host", "epoll_ctl")
int64_t host_epoll_ctl(int64_t epollfd, enum wasio_epoll_ctl cmd, int64_t fd, uint32_t u32);

extern
__wasm_import__("host", "epoll_create")
int64_t host_epoll_create(void);

wasio_result_t wasio_new(struct wasio_fd *wfd, uint32_t max_fds, int64_t *preopened_sock, void *data) {
  wfd->evs = (struct wasio_event *)malloc(sizeof(struct wasio_event) * max_fds);
  assert(freelist_new(max_fds, &wfd->fl) == FREELIST_OK);
  uint32_t entry;
  assert(freelist_next(wfd->fl, &entry) == FREELIST_OK);
  wfd->evs[entry] = (struct wasio_event) { .fd = *preopened_sock, .data = data };
  wfd->epollfd = (int64_t)host_epoll_create();
  host_epoll_ctl(wfd->epollfd, WASIO_CTL_ADD, *preopened_sock, entry);
  return WASIO_OK;
}

extern
__wasm_import__("host", "epoll_close")
void host_epoll_close(int64_t epollfd);

void wasio_delete(struct wasio_fd *wfd) {
  host_epoll_close(wfd->epollfd);
  wfd->epollfd = -1;
}

wasio_result_t wasio_attach(struct wasio_fd *wfd, int64_t fd, void *data) {
  
}

wasio_result_t wasio_poll(struct wasio_fd *wfd, struct wasio_event *events, uint32_t max_events, uint32_t *num_events, int timeout);

int32_t wasio_error(struct wasio_fd *wfd, int64_t sockfd);

wasio_result_t wasio_accept(struct wasio_fd *wfd, int64_t sockfd);

wasio_result_t wasio_recv(struct wasio_fd *wfd, int64_t sockfd, uint8_t *buf, size_t len);

wasio_result_t wasio_send(struct wasio_fd *wfd, int64_t sockfd, uint8_t *buf, size_t len);

wasio_result_t wasio_close(struct wasio_fd *wfd, int64_t sockfd);
