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

static struct wasio_pollfd *wfd = NULL;

int waeio_main(void* (*main)(void*), void *arg) {
  (void)arg;
  assert(wasio_init(wfd, MAX_CONNECTIONS) == WASIO_OK);
  // Retrieve listener socket.
  int64_t sockfd = 0;
  wasio_fd_t vfd;
  assert(wasio_wrap(wfd, sockfd, &vfd) == WASIO_OK);
  // Allocate fiber for main.
  fiber_result_t status;
  fiber_t mainfiber = fiber_alloc(main);
  // Resume main.
  // Enqueue main (TODO)
  (void)fiber_resume(mainfiber, &vfd, &status);
  // Clean up
  fiber_free(mainfiber);
  wasio_finalize(wfd);
  return 0;
}

int waeio_async(void *(*proc)(wasio_fd_t*), wasio_fd_t vfd) {
  cmd_t cmd = { .tag = ASYNC, .entry = (fiber_entry_point_t)proc, .arg = vfd };
  int ans = (int)fiber_yield(&cmd);
  if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
  return ans;
}

static inline bool is_busy(int code) {
  return code == EAGAIN || code == EWOULDBLOCK;
}

int waeio_accept(wasio_fd_t vfd, wasio_fd_t *new_conn) {
  cmd_t cmd = { .tag = ACCEPT, .vfd = vfd };
  do {
    int ans = (int)fiber_yield(&cmd);

    if (ans < 0) {
      if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    if (wasio_accept(wfd, vfd, new_conn) == WASIO_OK)
      return 0;
    else
      return -1;
  } while (is_busy(errno));

  abort(); // unreachable
  return 0;
}

int waeio_recv(wasio_fd_t vfd, uint8_t *buf, uint32_t len) {
  cmd_t cmd = { .tag = RECV, .vfd = vfd };
  uint32_t recvlen = 0;
  do {
    int ans = (int)fiber_yield(&cmd);
    if (ans == FIBER_KILL_SIGNAL) {
      errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    if (wasio_recv(wfd, vfd, buf, len, &recvlen) == WASIO_OK)
      return recvlen;
    else
      return -1;
  } while (is_busy(errno));

  abort(); // unreachable
  return 0;
}

int waeio_send(wasio_fd_t vfd, uint8_t *buf, uint32_t len) {
  cmd_t cmd = { .tag = SEND, .vfd = vfd };
  uint32_t sendlen = 0;
  do {
    int ans = (int)fiber_yield(&cmd);
    if (ans == FIBER_KILL_SIGNAL) {
      errno = FIBER_KILL_SIGNAL;
      return ans;
    }
    if (wasio_send(wfd, vfd, buf, len, &sendlen) == WASIO_OK)
      return sendlen;
    else
      return -1;
  } while (is_busy(errno));

  abort(); // unreachable
  return 0;
}

int waeio_close(wasio_fd_t vfd) {
  return wasio_close(wfd, vfd) == WASIO_OK ? 0 : -1;
}

/* struct fiber_closure { */
/*   unsigned int entry; */
/*   fiber_t fiber; */
/*   void *arg; */
/* }; */

/* #define FIBER_QUEUE_LEN MAX_CONNECTIONS */
/* #define MAX_FDS MAX_CONNECTIONS */

/* // Fiber queue. */
/* typedef struct ready_queue { */
/*   size_t len; */
/*   struct fiber_closure store[FIBER_QUEUE_LEN]; */
/* } rqueue_t; */

/* static rqueue_t pushq = { .len = 0, .store = {0} }; */
/* static rqueue_t popq  = { .len = 0, .store = {0} }; */

/* static inline size_t rqueue_length(rqueue_t *q) { */
/*   return q->len; */
/* } */

/* static inline bool rqueue_is_empty(rqueue_t *q) { */
/*   return q->len == 0; */
/* } */

/* static inline bool rqueue_is_full(rqueue_t *q) { */
/*   return q->len == FIBER_QUEUE_LEN; */
/* } */

/* static inline void rqueue_push(rqueue_t *q, struct fiber_closure clo) { */
/*   assert(!rqueue_is_full(q)); */
/*   q->store[q->len++] = clo; */
/* } */

/* static inline struct fiber_closure rqueue_pop(rqueue_t *q) { */
/*   assert(!rqueue_is_empty(q)); */
/*   return q->store[FIBER_QUEUE_LEN - q->len--]; */
/* } */

/* enum cmd_tag { */
/*   ACCEPT, */
/*   ASYNC, */
/*   RECV, */
/*   SEND, */
/*   QUIT */
/* }; */

/* typedef struct cmd { */
/*   enum cmd_tag tag; */
/*   union { */
/*     // Async command */
/*     struct { */
/*       fiber_entry_point_t entry; */
/*       void *arg; */
/*     }; */
/*     // IO command */
/*     int fd; */
/*   }; */
/* } cmd_t; */

/* typedef struct waeio_state { */
/*   rqueue_t *popq; */
/*   rqueue_t *pushq; */
/*   fiber_t fibers[MAX_FDS]; */
/*   struct wasio_pollfd fds[MAX_FDS]; */
/*   freelist_t fl; */
/*   unsigned int current; */
/* } waeio_state_t; */

/* static void initialise(size_t len, waeio_state_t *state, fiber_t main, void *arg) { */
/*   for (size_t i = 0; i < len; i++) { */
/*     state->fds[i] = (struct wasio_pollfd) { .fd = -1, .events = 0, .revents = 0 }; */
/*     state->fibers[i] = NULL; */
/*   } */
/*   assert(freelist_new(MAX_FDS, &state->fl) == FREELIST_OK); */
/*   assert(freelist_next(state->fl, &state->current) == FREELIST_OK); */
/*   state->pushq = &pushq; */
/*   state->popq = &popq; */
/*   rqueue_push(state->pushq, (struct fiber_closure) { .fiber = main, .arg = arg, .entry = state->current }); */
/* } */

/* static inline rqueue_t* get_ready_queue(waeio_state_t *state) { */
/*   rqueue_t *tmp = state->pushq; */
/*   state->pushq = state->popq; */
/*   assert(state->pushq->len == 0); */
/*   state->popq = tmp; */
/*   return tmp; */
/* } */

/* static inline bool handle_cmd(cmd_t *cmd, waeio_state_t *state) { */
/*   switch (cmd->tag) { */
/*   case ASYNC: { */
/*     unsigned int entry; */
/*     // TODO(dhil): we should handle failure gracefully here. */
/*     assert(freelist_next(state->fl, &entry) == FREELIST_OK); */
/*     fiber_t new_fiber = fiber_alloc(cmd->entry); */
/*     state->fibers[entry] = new_fiber; */
/*     // TODO(dhil): We need to check that push is defined here. */
/*     assert(rqueue_length(state->pushq) + 2 < FIBER_QUEUE_LEN); */
/*     rqueue_push(state->pushq, (struct fiber_closure){ .fiber = new_fiber, .arg = cmd->arg, .entry = entry }); */
/*     rqueue_push(state->pushq, (struct fiber_closure){ .fiber = state->fibers[state->current], .arg = (void*)0, .entry = state->current }); */
/*   } */
/*     break; */
/*   case ACCEPT: */
/*     state->fds[state->current].fd = cmd->fd; */
/*     state->fds[state->current].events = wasio_flags(WASIO_POLLIN); */
/*     break; */
/*   case RECV: */
/*     state->fds[state->current].fd = cmd->fd; */
/*     state->fds[state->current].events = wasio_flags(WASIO_POLLIN); */
/*     break; */
/*   case SEND: */
/*     state->fds[state->current].fd = cmd->fd; */
/*     state->fds[state->current].events = wasio_flags(WASIO_POLLOUT); */
/*     break; */
/*   case QUIT: */
/*     return true; */
/*     break; */
/*   default: */
/*     abort(); */
/*   } */

/*   return false; */
/* } */

/* static inline bool continue_fiber(struct fiber_closure clo, waeio_state_t *state) { */
/*   //unsigned int entry; */
/*   state->current = clo.entry; */
/*   fiber_result_t result; */
/*   void *ans = fiber_resume(clo.fiber, clo.arg, &result); */
/*   switch (result) { */
/*     case FIBER_OK: */
/*       assert(freelist_reclaim(state->fl, clo.entry) == FREELIST_OK); */
/*       state->fibers[clo.entry] = NULL; */
/*       state->fds[clo.entry] = (struct wasio_pollfd){ .fd = -1, .events = 0, .revents = 0 }; */
/*       fiber_free(clo.fiber); */
/*       break; */
/*     case FIBER_YIELD: */
/*       return handle_cmd((cmd_t*)ans, state); */
/*       break; */
/*     case FIBER_ERROR: */
/*       abort(); */
/*       break; */
/*     default: */
/*       abort(); */
/*     } */
/*   return false; */
/* } */

/* int waeio_main(void* (*main)(void*), void *arg) { */
/*   bool quit = false; */
/*   waeio_state_t *state = (waeio_state_t*)malloc(sizeof(waeio_state_t)); */
/*   initialise(MAX_FDS, state, fiber_alloc(main), arg); */

/*   // Scheduler loop */
/*   while (!quit) { */
/*     // First run every ready fiber... */
/*     rqueue_t *readyq = get_ready_queue(state); */
/*     while (!rqueue_is_empty(readyq)) { */
/*       struct fiber_closure clo = rqueue_pop(readyq); */
/*       quit = continue_fiber(clo, state); */
/*       if (quit) break; */
/*     } */
/*     if (quit) break; */
/*     // ... then poll for I/O events */
/*     int num_ready = wasio_poll(state->fds, MAX_FDS); */
/*     if (num_ready > 0) { */
/*       for (int i = 0; i < MAX_FDS; i++) { */
/*         if (state->fds[i].fd == -1) continue; */
/*         int rflags = wasio_rflags(state->fds[i].revents); */
/*         if (rflags & WASIO_POLLIN || rflags & WASIO_POLLOUT) { */
/*           state->fds[i].fd = -1; */
/*           quit = continue_fiber((struct fiber_closure){ .fiber = state->fibers[i], .arg = (void*)0, .entry = i }, state); */
/*           if (quit) break; */
/*         } */
/*       } */
/*     } else if (num_ready < 0) abort(); */
/*     else continue; // timeout */
/*   } */

/*   // TODO(dhil): clean up dangling fibers */
/*   freelist_delete(state->fl); */
/*   free(state); */
/*   return 0; */
/* } */

/* int waeio_async(void* (*fp)(void*), void *arg) { */
/*   cmd_t cmd = { .tag = ASYNC, .entry = fp, .arg = arg }; */
/*   int ans = (int)fiber_yield(&cmd); */
/*   if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL; */
/*   return ans; */
/* } */

/* int waeio_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) { */
/*   cmd_t cmd = { .tag = ACCEPT, .fd = fd }; */
/*   int ans = (int)fiber_yield(&cmd); */
/*   if (ans < 0) { */
/*     if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL; */
/*     return ans; */
/*   } */
/*   ans = accept(fd, addr, addr_len); */
/*   return ans; */
/* } */

/* static inline bool is_busy(int code) { */
/*   return code == EAGAIN || code == EWOULDBLOCK; */
/* } */

/* int waeio_recv(int fd, char *buf, size_t len) { */
/*   cmd_t cmd = { .tag = RECV, .fd = fd }; */
/*   int ans; */
/*   do { */
/*     ans = (int)fiber_yield(&cmd); */
/*     if (ans < 0) { */
/*       if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL; */
/*       return ans; */
/*     } */
/*     ans = recv(fd, buf, len, 0); */
/*   } while (ans < 0 && is_busy(errno)); */
/*   return ans; */
/* } */

/* int waeio_send(int fd, char *buf, size_t len) { */
/*   cmd_t cmd = { .tag = SEND, .fd = fd }; */
/*   int ans; */
/*   do { */
/*     ans = (int)fiber_yield(&cmd); */
/*     if (ans < 0) { */
/*       if (ans == FIBER_KILL_SIGNAL) errno = FIBER_KILL_SIGNAL; */
/*       return ans; */
/*     } */
/*     ans = send(fd, buf, len, 0); */
/*   } while (ans < 0 && is_busy(errno)); */
/*   return ans; */
/* } */

/* int waeio_close(int fd) { */
/*   return wasio_close(fd); */
/* } */

/* #undef FIBER_QUEUE_LEN */
/* #undef MAX_FDS */
