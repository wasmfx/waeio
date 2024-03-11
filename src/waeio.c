#include <errno.h>
#include <fiber.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <waeio.h>
#include <wasio.h>

enum cmd_tag {
  ACCEPT,
  ASYNC,
  RECV,
  SEND,
  QUIT
};

typedef struct cmd {
  enum cmd_tag tag;
  union {
    // Async command
    struct {
      fiber_entry_point_t entry;
      void *arg;
    };
    // IO command
    int fd;
  };
} cmd_t;

struct waeio_pollfd {
  struct wasio_pollfd pollfd;
  fiber_t owner;
};

#define MAX_FDS 10000

typedef struct waeio_state {
  struct waeio_pollfd fds[MAX_FDS];
  size_t nextfree;
  size_t current;
  bool do_poll;
  void *arg;
} waeio_state_t;

static void initialise(size_t len, waeio_state_t *state, fiber_t main, void *arg) {
  for (size_t i = 0; i < len; i++) {
    state->fds[i] = (struct waeio_pollfd){ .pollfd = (struct wasio_pollfd) { .fd = -1, .events = 0, .revents = 0 }, .owner = NULL };
  }
  state->fds[0].owner = main;
  state->nextfree = 1;
  state->current = 0;
  state->do_poll = true;
  state->arg = arg;
}

static inline bool handle_cmd(cmd_t *cmd, waeio_state_t *state) {
  switch (cmd->tag) {
  case ASYNC: {
    fiber_t new_fiber = fiber_alloc(cmd->entry);
    state->current = state->nextfree++;
    state->fds[state->current].owner = new_fiber;
    state->arg = cmd->arg;
    state->do_poll = false;
  }
    break;
  case ACCEPT:
    state->fds[state->current].pollfd.events = WASIO_POLLIN;
    break;
  case RECV:
    state->fds[state->current].pollfd.events = WASIO_POLLIN;
    break;
  case SEND:
    state->fds[state->current].pollfd.events = WASIO_POLLOUT;
    break;
  case QUIT:
    return true;
    break;
  default:
    abort();
  }

  return false;
}

static inline bool continue_fiber(fiber_t fiber, waeio_state_t *state) {
  fiber_result_t result;
  void *ans = fiber_resume(fiber, state->arg, &result);
  state->arg = (void*)0;
  switch (result) {
    case FIBER_OK:
      fiber_free(fiber);
      break;
    case FIBER_YIELD:
      return handle_cmd((cmd_t*)ans, state);
      break;
    case FIBER_ERROR:
      abort();
      break;
    default:
      abort();
    }
  return false;
}

int waeio_main(void* (*main)(void*), void *arg) {
  bool quit = false;
  waeio_state_t *state = (waeio_state_t*)malloc(sizeof(waeio_state_t));
  initialise(MAX_FDS, state, fiber_alloc(main), arg);
  while(!quit) {
    if (state->do_poll) {
      int num_ready = wasio_poll((struct wasio_pollfd*)state->fds, MAX_FDS);
      for (int i = 0; i < num_ready; i++) {
        if (state->fds[i].pollfd.revents & WASIO_POLLIN || state->fds[i].pollfd.revents & WASIO_POLLOUT) {
          state->fds[i].pollfd.revents = 0;
          quit = continue_fiber(state->fds[i].owner, state);
          if (quit) break;
        }
      }
    }
    else state->do_poll = true;
  }
  free(state);
  return 0;
}

int waeio_async(void* (*fp)(void*), void *arg) {
  cmd_t cmd = { .tag = ASYNC, .entry = fp, .arg = arg };
  int ans = (int)fiber_yield(&cmd);
  if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
  return ans;
}

int waeio_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) {
  cmd_t cmd = { .tag = ACCEPT, .fd = fd };
  int ans = (int)fiber_yield(&cmd);
  if (ans < 0) {
    if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
    return ans;
  }
  ans = accept(fd, addr, addr_len);
  return ans;
}

static inline bool is_busy(int code) {
  return code == EAGAIN || code == EWOULDBLOCK;
}

int waeio_recv(int fd, char *buf, size_t len) {
  cmd_t cmd = { .tag = RECV, .fd = fd };
  int ans;
  do {
    ans = (int)fiber_yield(&cmd);
    if (ans < 0) {
      if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    ans = recv(fd, buf, len, 0);
  } while (ans < 0 && is_busy(errno));
  return ans;
}

int waeio_send(int fd, char *buf, size_t len) {
  cmd_t cmd = { .tag = SEND, .fd = fd };
  int ans;
  do {
    ans = (int)fiber_yield(&cmd);
    if (ans < 0) {
      if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    ans = send(fd, buf, len, 0);
  } while (ans < 0 && is_busy(errno));
  return ans;
}

int waeio_close(int fd) {
  return wasio_close(fd);
}
