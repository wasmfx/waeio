#include <fiber.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <waeio.h>
#include <wasio.h>

enum io_cmd_tag {
  ACCEPT,
  RECV,
  SEND
};

typedef struct io_cmd {
  enum io_cmd_tag tag;
  int fd;
  union {
    struct {
      struct sockaddr *addr;
      socklen_t *addr_len;
    };
    struct {
      char *buf;
      size_t len;
    };
  };
} io_cmd_t;

typedef struct blocked_fiber {
  fiber_t fiber;
  int fd;
  io_cmd_t *cmd;
} blocked_fiber_t;

typedef struct ready_fiber {
  fiber_t fiber;
  int value;
} ready_fiber_t;

typedef struct waeio_state {
  fiber_t blocked[10000];
  size_t ready_len;
  fiber_t ready[10000];
  size_t ready_len;
} waeio_state_t;

int waeio_main(void* (*main)(void*), void *arg) {
  waeio_state_t *state = (waeio_state_t*)malloc(sizeof(waeio_state_t));
  fiber_result_t result;
  state->ready[0] = fiber_alloc(main);
  while(true /* TODO(dhil): Figure out termination condition. */) {
    fiber_t fiber = state->ready[0];
    void *ans = fiber_resume(fiber, arg, &result);
    switch (result) {
    case FIBER_OK:
      fiber_free(fiber);
      continue;
    case FIBER_YIELD:
      io_cmd_t *cmd = (io_cmd_t*)ans;
      switch (cmd->tag) {
      case ACCEPT:
        break;
      case RECV:
        break;
      case SEND:
        break;
      default:
        continue;
      }
      continue;
    case FIBER_ERROR:
      abort();
      break;
    default:
      abort();
      continue;
    }
  }
  free(state);
  return 0;
}

// TODO(dhil): Problem here: yielding is shallow, but the dynamic
// context may require a deep yield.
void* waeio_async(void* (*fp)(void*), void *arg) {
  fiber_result_t result;
  fiber_t fiber = fiber_alloc(fp);
  io_cmd_t *cmd = (io_cmd_t*)fiber_resume(fiber, arg, &result);
  (void)cmd;
  return NULL;
}

int waeio_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) {
  io_cmd_t cmd = { .tag = ACCEPT, .fd = fd, .addr = addr, .addr_len = addr_len };
  int ans = *((int*)fiber_yield(&cmd));
  return ans;
}

int waeio_recv(int fd, char *buf, size_t len) {
  io_cmd_t cmd = { .tag = RECV, .fd = fd, .buf = buf, .len = len };
  int ans = *((int*)fiber_yield(&cmd));
  return ans;
}

int waeio_send(int fd, char *buf, size_t len) {
  io_cmd_t cmd = { .tag = SEND, .fd = fd, .buf = buf, .len = len };
  int ans = *((int*)fiber_yield(&cmd));
  return ans;
}

int waeio_close(int fd) {
  return wasio_close(fd);
}
