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

#define FIBER_KILL_SIGNAL INT32_MIN

enum cmd_tag {
  ACCEPT,
  ASYNC,
  SUSPEND,
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
      wasio_fd_t arg;
    };
    // IO command
    wasio_fd_t vfd;
  };
} cmd_t;

struct fiber_closure {
  fiber_t fiber;
  void *arg;
};

// Queue interface
struct qnode {
  struct qnode *next;
  struct fiber_closure clo;
};

struct queue {
  struct qnode *next;
};

static inline void queue_push(struct queue *q, struct fiber_closure clo) {
  struct qnode *node = (struct qnode*)malloc(sizeof(struct qnode));
  node->clo = clo;
  node->next = q->next;
  q->next = node;
}

static inline struct fiber_closure queue_pop(struct queue *q) {
  if (q == NULL) abort();
  struct qnode *next = q->next;
  q->next = next->next;
  struct fiber_closure clo = next->clo;
  free(next);
  return clo;
}

static inline bool queue_is_empty(const struct queue *q) {
  return q->next == NULL;
}

struct waeio_ctl {
  uint32_t nconns;
  uint32_t max_conns;
  struct queue *frontq;
  struct queue *rearq;
  struct wasio_pollfd wfd;
  struct wasio_event *ev;
  fiber_t fibers[MAX_CONNECTIONS];
};

static struct waeio_ctl ctl = {0};

static inline void queue_swap(void) {
  struct queue *tmp = ctl.frontq;
  ctl.frontq = ctl.rearq;
  ctl.rearq = tmp;
}

static bool handle_request(fiber_t yieldee, fiber_result_t status, void *payload) {
  switch (status) {
  case FIBER_OK: { // Run to completion.
    ctl.nconns--;
    free(yieldee);
  }
    break;
  case FIBER_ERROR: { // TODO(dhil): decide what to do...
    abort();
  }
    break;
  case FIBER_YIELD: {
    cmd_t *cmd = (cmd_t*)payload;
    switch (cmd->tag) {
    case ASYNC: {
      uint32_t vfd = (uint32_t)(intptr_t)cmd->arg;
      fiber_t child = fiber_alloc(cmd->entry);
      ctl.fibers[vfd] = child;
      queue_push(ctl.rearq, (struct fiber_closure){ .fiber = child, .arg = (void*)(intptr_t)vfd });
    } // fall through
    case SUSPEND:
      queue_push(ctl.rearq, (struct fiber_closure){ .fiber = yieldee, .arg = NULL });
      break;
    case ACCEPT:
    case RECV: {
      uint32_t vfd = (uint32_t)(intptr_t)cmd->arg;
      wasio_notify_recv(&ctl.wfd, vfd);
    }
      break;
    case SEND: {
      uint32_t vfd = (uint32_t)(intptr_t)cmd->arg;
      wasio_notify_send(&ctl.wfd, vfd);
      // NOTE(dhil): the fiber is implicitly enqueued by the I/O subsystem.
    }
      break;
    case QUIT:
      return false;
    }
  }
    break;
  }
  return true;
}

static bool run_next(void) {
  // First run all ready fibers.
  fiber_result_t status;
  bool keep_going = true;
  while (!queue_is_empty(ctl.frontq) && keep_going) {
    struct fiber_closure clo = queue_pop(ctl.frontq);
    void *ans = fiber_resume(clo.fiber, clo.arg, &status);
    keep_going = handle_request(clo.fiber, status, ans);
  }
  // Swap front and rear queues.
  queue_swap();
  // Now poll.
  uint32_t nready;
  if (wasio_poll(&ctl.wfd, ctl.ev, ctl.max_conns, &nready, 0) != WASIO_OK)
    return false;
  WASIO_EVENT_FOREACH(&ctl.wfd, ctl.ev, nready, vfd, {
      void *ans = fiber_resume(ctl.fibers[vfd], &status, (void*)(intptr_t)0);
      if (!handle_request(ctl.fibers[vfd], status, ans)) return false;
    });
  return keep_going;
}

int waeio_main(void* (*listener)(wasio_fd_t*)) {
  ctl.frontq = (struct queue*)malloc(sizeof(struct queue));
  ctl.rearq  = (struct queue*)malloc(sizeof(struct queue));
  ctl.nconns = 0;
  ctl.max_conns = MAX_CONNECTIONS;
  assert(wasio_init(&ctl.wfd, ctl.max_conns) == WASIO_OK);
  ctl.ev = WASIO_EVENT_INITIALISER(ctl.max_conns);
  // Open listener socket.
  wasio_fd_t servsock;
  assert(wasio_listen(&ctl.wfd, &servsock, 8080, 1000) == WASIO_OK);
  ctl.nconns++;
  // Allocate fiber for main.
  fiber_t mainfiber = fiber_alloc((fiber_entry_point_t)(void*)listener);
  ctl.fibers[(uint32_t)servsock] = mainfiber;
  // Enqueue main (TODO)
  queue_push(ctl.frontq, (struct fiber_closure){ .fiber = mainfiber, .arg = &servsock });
  // Enter scheduling loop.
  bool keep_going = true;
  while (keep_going) {
    keep_going = run_next();
  }
  // Clean up
  fiber_free(mainfiber);
  wasio_close(&ctl.wfd, servsock);
  wasio_finalize(&ctl.wfd);
  free(ctl.frontq);
  free(ctl.rearq);
  return 0;
}

int waeio_async(void *(*proc)(wasio_fd_t*), wasio_fd_t vfd) {
  cmd_t cmd = { .tag = ASYNC, .entry = (fiber_entry_point_t)proc, .arg = vfd };
  int ans = (int)fiber_yield(&cmd);
  if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
  return ans;
}

static inline bool is_busy(wasio_result_t res) {
  return res == WASIO_EAGAIN;
}

int waeio_accept(wasio_fd_t vfd, wasio_fd_t *new_conn) {
  cmd_t cmd = { .tag = ACCEPT, .vfd = vfd };
  wasio_result_t res;
  do {
    int ans;
    ans = (int)fiber_yield(&cmd);

    if (ans < 0) {
      if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
      return ans;
    }

    // Keep suspending if there is insufficient space to accept new
    // connections.
    while (ctl.nconns == ctl.max_conns) {
      cmd_t cmd = { .tag = SUSPEND, .vfd = -1 };
      ans = (int)fiber_yield(&cmd);
      if (ans < 0) {
        if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
        return ans;
      }
    }

    res = wasio_accept(&ctl.wfd, vfd, new_conn);
    if (res == WASIO_OK)
      return 0;
  } while (is_busy(res));

  return -1;
}

int waeio_recv(wasio_fd_t vfd, uint8_t *buf, uint32_t len) {
  cmd_t cmd = { .tag = RECV, .vfd = vfd };
  uint32_t recvlen = 0;
  wasio_result_t res;
  do {
    int ans = (int)fiber_yield(&cmd);
    if (ans == FIBER_KILL_SIGNAL) {
      errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    res = wasio_recv(&ctl.wfd, vfd, buf, len, &recvlen);
    if (res == WASIO_OK)
      return recvlen;
  } while (is_busy(res));

  return -1;
}

int waeio_send(wasio_fd_t vfd, uint8_t *buf, uint32_t len) {
  cmd_t cmd = { .tag = SEND, .vfd = vfd };
  uint32_t sendlen = 0;
  wasio_result_t res;
  do {
    int ans = (int)fiber_yield(&cmd);
    if (ans == FIBER_KILL_SIGNAL) {
      errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    res = wasio_send(&ctl.wfd, vfd, buf, len, &sendlen);
    if (res == WASIO_OK)
      return sendlen;
  } while (is_busy(res));

  return -1;
}

int waeio_close(wasio_fd_t vfd) {
  return wasio_close(&ctl.wfd, vfd) == WASIO_OK ? 0 : -1;
}

void waeio_cancel_all(void) {
  cmd_t cmd = { .tag = QUIT, .vfd = -1 };
  (void)fiber_yield(&cmd);
}

#undef FIBER_KILL_SIGNAL

