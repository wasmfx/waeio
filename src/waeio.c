#include <assert.h>
#include <errno.h>
#include <fiber.h>
#include <freelist.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <waeio.h>
#include <wasio.h>

struct fiber_closure {
  unsigned int entry;
  fiber_t fiber;
  void *arg;
};

#define FIBER_QUEUE_LEN (1 << 20)
#define MAX_FDS (1 << 20)

// Fiber queue.
typedef struct ready_queue {
  size_t len;
  struct fiber_closure store[FIBER_QUEUE_LEN];
} rqueue_t;

static rqueue_t pushq = { .len = 0, .store = {0} };
static rqueue_t popq  = { .len = 0, .store = {0} };

static inline size_t rqueue_length(rqueue_t *q) {
  return q->len;
}

static inline bool rqueue_is_empty(rqueue_t *q) {
  return q->len == 0;
}

static inline bool rqueue_is_full(rqueue_t *q) {
  return q->len == FIBER_QUEUE_LEN;
}

static inline void rqueue_push(rqueue_t *q, struct fiber_closure clo) {
  assert(!rqueue_is_full(q));
  q->store[q->len++] = clo;
}

static inline struct fiber_closure rqueue_pop(rqueue_t *q) {
  assert(!rqueue_is_empty(q));
  return q->store[FIBER_QUEUE_LEN - q->len--];
}

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

typedef struct waeio_state {
  rqueue_t *popq;
  rqueue_t *pushq;
  fiber_t fibers[MAX_FDS];
  struct wasio_pollfd fds[MAX_FDS];
  freelist_t fl;
  unsigned int current;
} waeio_state_t;

static void initialise(size_t len, waeio_state_t *state, fiber_t main, void *arg) {
  for (size_t i = 0; i < len; i++) {
    state->fds[i] = (struct wasio_pollfd) { .fd = -1, .events = 0, .revents = 0 };
    state->fibers[i] = NULL;
  }
  assert(freelist_new(MAX_FDS, &state->fl) == FREELIST_OK);
  assert(freelist_next(state->fl, &state->current) == FREELIST_OK);
  state->pushq = &pushq;
  state->popq = &popq;
  rqueue_push(state->pushq, (struct fiber_closure) { .fiber = main, .arg = arg, .entry = state->current });
}

static inline rqueue_t* get_ready_queue(waeio_state_t *state) {
  rqueue_t *tmp = state->pushq;
  state->pushq = state->popq;
  assert(state->pushq->len == 0);
  state->popq = tmp;
  return tmp;
}

static inline bool handle_cmd(cmd_t *cmd, waeio_state_t *state) {
  switch (cmd->tag) {
  case ASYNC: {
    unsigned int entry;
    // TODO(dhil): we should handle failure gracefully here.
    assert(freelist_next(state->fl, &entry) == FREELIST_OK);
    fiber_t new_fiber = fiber_alloc(cmd->entry);
    state->fibers[entry] = new_fiber;
    // TODO(dhil): We need to check that push is defined here.
    assert(rqueue_length(state->pushq) + 2 < FIBER_QUEUE_LEN);
    rqueue_push(state->pushq, (struct fiber_closure){ .fiber = new_fiber, .arg = cmd->arg, .entry = entry });
    rqueue_push(state->pushq, (struct fiber_closure){ .fiber = state->fibers[state->current], .arg = (void*)0, .entry = state->current });
  }
    break;
  case ACCEPT:
    state->fds[state->current].fd = cmd->fd;
    state->fds[state->current].events = WASIO_POLLIN;
    break;
  case RECV:
    state->fds[state->current].fd = cmd->fd;
    state->fds[state->current].events = WASIO_POLLIN;
    break;
  case SEND:
    state->fds[state->current].fd = cmd->fd;
    state->fds[state->current].events = WASIO_POLLOUT;
    break;
  case QUIT:
    return true;
    break;
  default:
    abort();
  }

  return false;
}

static inline bool continue_fiber(struct fiber_closure clo, waeio_state_t *state) {
  //unsigned int entry;
  state->current = clo.entry;
  fiber_result_t result;
  void *ans = fiber_resume(clo.fiber, clo.arg, &result);
  switch (result) {
    case FIBER_OK:
      assert(freelist_reclaim(state->fl, clo.entry) == FREELIST_OK);
      state->fibers[clo.entry] = NULL;
      state->fds[clo.entry] = (struct wasio_pollfd){ .fd = -1, .events = 0, .revents = 0 };
      fiber_free(clo.fiber);
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

  // Scheduler loop
  while (!quit) {
    // First run every ready fiber...
    rqueue_t *readyq = get_ready_queue(state);
    while (!rqueue_is_empty(readyq)) {
      struct fiber_closure clo = rqueue_pop(readyq);
      quit = continue_fiber(clo, state);
      if (quit) break;
    }
    if (quit) break;
    // ... then poll for I/O events
    int num_ready = wasio_poll(state->fds, MAX_FDS);
    if (num_ready > 0) {
      for (int i = 0; i < MAX_FDS; i++) {
        if (state->fds[i].fd == -1) continue;
        if (state->fds[i].revents & WASIO_POLLIN || state->fds[i].revents & WASIO_POLLOUT) {
          state->fds[i].fd = -1;
          quit = continue_fiber((struct fiber_closure){ .fiber = state->fibers[i], .arg = (void*)0, .entry = i }, state);
          if (quit) break;
        }
      }
    } else if (num_ready < 0) abort();
    else continue; // timeout
  }

  // TODO(dhil): clean up dangling fibers
  freelist_delete(state->fl);
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

#undef FIBER_QUEUE_LEN
#undef MAX_FDS
