#include <assert.h>
#include <host/errno.h>
#include <host/poll.h>
#include <host/socket.h>
#include <http_utils.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <picohttpparser.h>

//#include <waeio.h>
#include <fiber.h>
#include <wasio.h>

#include <unistd.h>
#include <wasi/api.h>
#define wasi_print(str) do { \
    const uint8_t *start = (uint8_t *)str; \
    const uint8_t *end = start; \
    for (; *end != '\0'; end++) {} \
    __wasi_ciovec_t iov = {.buf = start, .buf_len = end - start}; \
    size_t nwritten; \
    (void)__wasi_fd_write(STDOUT_FILENO, &iov, 1, &nwritten);   \
} while (0)
#define wasi_print_debug(str) \
    wasi_print(__FILE__ ":" STRINGIZE(__LINE__) ": " str "\n")


#define debug_println(fn, fd, msg) do {               \
    printf("[%s(%d)] %s\n", fn, fd, msg); \
    fflush(stdout);\
  } while(false);
/* #define debug_println(fn, fd, msg) {}; */

#define FIBER_KILL_SIGNAL INT32_MIN

static const uint32_t buffer_size = 1 << 16;
static const uint32_t max_headers = 100;
static const uint32_t max_clients = MAX_CONNECTIONS - 1;
static uint32_t clients = 0;


struct fiber_closure {
  fiber_t fiber;
  wasio_fd_t fd;
};

struct fiber_queue {
  uint32_t capacity;
  uint32_t length;
  struct fiber_closure q[];
};

static struct fiber_queue *frontq, *rearq;

static inline struct fiber_queue* fq_new(uint32_t capacity) {
  struct fiber_queue *fq = (struct fiber_queue*)malloc(sizeof(struct fiber_queue) + sizeof(struct fiber_closure[capacity]));
  fq->capacity = capacity;
  fq->length = 0;
  return fq;
}

static inline void fq_push(struct fiber_queue *fq, struct fiber_closure clo) {
  if (fq->length < fq->capacity) {
    fq->q[fq->length++] = clo;
  } else {
    abort(); // TODO(dhil): expand queue buffer
  }
}

static inline void fq_swap(struct fiber_queue **frontq, struct fiber_queue **rearq) {
  struct fiber_queue *q = *frontq;
  *frontq = *rearq;
  *rearq = q;
}

static struct wasio_pollfd wfd;
static struct wasio_event ev;
static struct fiber_closure fibers[MAX_CONNECTIONS];

typedef enum { ASYNC = 0, YIELD = 1, RECV = 2, SEND = 3, QUIT = 4 } cmd_tag_t;

typedef struct cmd {
  cmd_tag_t tag;
  union {
    struct fiber_closure clo;
    wasio_fd_t fd;
  };
} cmd_t;

static cmd_t cmd; // NOTE(dhil): we use a global variable to
                  // communicate payloads, as asyncify or the
                  // toolchain seemingly messes up the program if we
                  // transfer payloads between stacks.

static inline cmd_t cmd_make(cmd_tag_t tag, wasio_fd_t fd) {
  cmd_t cmd;
  cmd.tag = tag;
  cmd.fd = fd;
  return cmd;
}

static inline int yield() {
  cmd = cmd_make(YIELD, 0);
  return (int)(intptr_t)fiber_yield(&cmd);
}

static int32_t w_recv(struct wasio_pollfd *wfd, wasio_fd_t fd, uint8_t *buf, uint32_t bufsize) {
  cmd = cmd_make(RECV, fd);
  uint32_t nbytes = 0;
  wasio_result_t ans = WASIO_OK;
  /* printf("[w_recv(%d)] receiving\n", fd); */
  while ( (ans = wasio_recv(wfd, fd, buf, bufsize, &nbytes)) == WASIO_EAGAIN) {
    debug_println("w_recv", fd, "yielding");
    int code = (int)(intptr_t)fiber_yield(&cmd);
    if (code == FIBER_KILL_SIGNAL) return FIBER_KILL_SIGNAL;
    debug_println("w_recv", fd, "continued");
  }
  return ans == WASIO_OK ? nbytes : -1;
}

static int32_t w_send(struct wasio_pollfd *wfd, wasio_fd_t fd, uint8_t *buf, uint32_t bufsize) {
  cmd = cmd_make(SEND, fd);
  uint32_t nbytes = 0;
  wasio_result_t ans;
  while ( (ans = wasio_send(wfd, fd, buf, bufsize, &nbytes)) == WASIO_EAGAIN) {
    debug_println("w_send", fd, "yielding");
    int code = (int)(intptr_t)fiber_yield(&cmd);
    if (code == FIBER_KILL_SIGNAL) return FIBER_KILL_SIGNAL;
    debug_println("w_send", fd, "continued");
  }
  return ans == WASIO_OK ? nbytes : -1;
}

static void* handle_connection(wasio_fd_t);
static wasio_result_t w_accept(struct wasio_pollfd *wfd, wasio_fd_t fd, uint32_t max_accept, uint32_t *naccepted) {

    /* case ASYNC: */
    /*   debug_println("handle_request", clo.fd, " -> ASYNC"); */
    /*   debug_println("handle_request", cmd.clo.fd, " <- ASYNC"); */
    /*   fibers[cmd.clo.fd] = cmd.clo; */
    /*   fq_push(rearq, cmd.clo); // fall-through */
  while (*naccepted < max_accept) {
    wasio_fd_t conn = -1;
    wasio_result_t ans = wasio_accept(wfd, fd, &conn);

    if (ans == WASIO_OK) {
      printf("[w_accept(%d)] new conn: %d\n", fd, conn);
      assert(wasio_notify_recv(wfd, fd) == WASIO_OK);
      fiber_t fiber = fiber_alloc((fiber_entry_point_t)(void*)handle_connection);
      fq_push(rearq, (struct fiber_closure) { .fiber = fiber, .fd = conn });
      *naccepted += 1;
      continue;
    } else {
      return ans; // TODO(dhil): propagate I/O error?
    }
  }

  return WASIO_OK;
}

static void* handle_connection(wasio_fd_t clientfd) {
  char reqbuf[buffer_size];
  const char *method;
  size_t method_len;
  const char *path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[max_headers];

  int32_t ans = 0;
  while (true) {
    size_t prevbuflen = 0, buflen = 0;
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);
    while (true) {
      ans = w_recv(&wfd, clientfd, (uint8_t*)reqbuf+buflen, sizeof(reqbuf) - buflen); // TODO(dhil): recv may be partial
      //printf("[handle_connection(%d)] bytes recv'd: %d\n", clientfd, ans);
      /* printf("[handle_connection(%d)] request:\n%s\n", clientfd, reqbuf); */
      if (ans == 0) return NULL;
      if (ans < 0) {
        printf("I/O failure\n");
        debug_println("handle_connection", clientfd, "I/O recv failure");
        return NULL; // bail out.
      }
      prevbuflen = buflen;
      buflen += ans;

      ans = phr_parse_request(reqbuf, sizeof(reqbuf) - buflen, &method, &method_len, &path, &path_len,
                              &minor_version, headers, &num_headers, prevbuflen);
      if (ans > 0) {
        debug_println("handle_connection", clientfd, "Http parse OK");
        if (path_len == 1 && strncmp(path, "/", 1) == 0) {
          ans = 1; // OK root
        } else if (path_len == strlen("/quit") && strncmp(path, "/quit", strlen("/quit")) == 0) {
          ans = 2; // OK quit
        } else {
          ans = 0; // Not found
        }
        break;
      } else if (ans == -1) {
        debug_println("handle_connection", clientfd, "Http parse failure");
        ans = -1; // Internal error.
        break;
      } else {
        assert(ans == -2); // Partial parse.
        // Check for overflow, otherwise continue.
        if (buflen == buffer_size) {
          ans = -1; // Internal error.
          break;
        }
      }
    }

    int nbytes = 0;
    if (ans >= 0) {
    assert(ans < 3);
    if (ans == 1)
      nbytes = response_ok((uint8_t*)reqbuf, buffer_size, (uint8_t*)response_body, (uint32_t)strlen(response_body));
    else if (ans == 2)
      nbytes = response_ok((uint8_t*)reqbuf, buffer_size, (uint8_t*)"OK bye...\n", (uint32_t)strlen("OK bye...\n"));
    else
      nbytes = response_notfound((uint8_t*)reqbuf, buffer_size, NULL, 0);
    } else {
      nbytes = response_err((uint8_t*)reqbuf, buffer_size, (uint8_t*)"I/O failure", (uint32_t)strlen("I/O failure"));
    }

    nbytes = w_send(&wfd, clientfd, (uint8_t*)reqbuf, nbytes); // TODO(dhil): send may be partial
    if (nbytes < 0) {
      debug_println("handle_connection", clientfd, "I/O send failure");
    }
    if (ans == 2) { // quitting.
      cmd = cmd_make(QUIT, -1);
      return fiber_yield(&cmd);
    }
  }

  return NULL;
}

void* listener(wasio_fd_t sockfd) {
  wasio_fd_t mysock = sockfd;
  while (true) {
    // Keep yielding if we are out of space.
    while (clients == max_clients) {
      int ans = yield();
      if (ans == FIBER_KILL_SIGNAL) return (void*)(intptr_t)FIBER_KILL_SIGNAL;
    }
    printf("[listener(%d)] max_accept = %u, clients = %u\n", mysock, max_clients - clients, clients);
    (void)w_accept(&wfd, mysock, max_clients - clients, &clients);
    printf("[listener(%d)] accepted: %u\n", mysock, clients);
    cmd = cmd_make(RECV, mysock);
    debug_println("listener", mysock, "yielding");
    int ans = (int)(intptr_t)fiber_yield(&cmd);
    debug_println("listener", mysock, "continued");
    if (ans == FIBER_KILL_SIGNAL) return (void*)(intptr_t)FIBER_KILL_SIGNAL;
  }
  return (void*)(intptr_t)0;
}

static bool handle_request(struct wasio_pollfd *wfd, struct fiber_queue *rearq, struct fiber_closure clo, void *ret __attribute__((unused)), fiber_result_t res) {
  debug_println("handle_request", clo.fd, "entered");
  switch (res) {
  case FIBER_OK: {
    debug_println("handle_request", clo.fd, "FIBER_OK");
    assert(wasio_close(wfd, clo.fd) == WASIO_OK);
    fiber_free(clo.fiber);
    fibers[clo.fd].fd = -1;
    clients--;
    return true;
  }
    break;
  case FIBER_ERROR:
    debug_println("handle_request", clo.fd, "FIBER_ERROR");
    abort();
  case FIBER_YIELD: {
    debug_println("handle_request", clo.fd, "FIBER_YIELD");
    switch (cmd.tag) {
    case YIELD:
      debug_println("handle_request", clo.fd, " -> YIELD");
      fq_push(rearq, clo);
      return true;
    case RECV:
      debug_println("handle_request", clo.fd, " -> RECV");
      assert(wasio_notify_recv(wfd, cmd.fd) == WASIO_OK);
      return true;
    case SEND:
      debug_println("handle_request", clo.fd, " -> SEND");
      assert(wasio_notify_send(wfd, cmd.fd) == WASIO_OK);
      return true;
    case QUIT: {
      debug_println("handle_request", clo.fd, " -> QUIT");
      return false;
    }
    default:
      debug_println("handle_request", clo.fd, " -> UNKNOWN");
      abort();
    }
  }
  }
  return false;
}

int main(void) {
  fiber_init();
  // Setup fiber queues.
  frontq = fq_new(MAX_CONNECTIONS);
  rearq = fq_new(MAX_CONNECTIONS);

  // Initialise tracked fiber closures.
  for (uint32_t i = 0; i < MAX_CONNECTIONS; i++) {
    fibers[i] = (struct fiber_closure){ .fiber = NULL, .fd = -1 };
  }

  // Initialise the I/O subsystem.
  assert(wasio_init(&wfd, MAX_CONNECTIONS) == WASIO_OK);

  // Open the listener socket.
  wasio_fd_t servfd = -1;
  const uint32_t backlog = 1000;
  assert(wasio_listen(&wfd, &servfd, 8080, backlog) == WASIO_OK && servfd == 0);

  // Create a fiber for the listener.
  fiber_t lfiber = fiber_alloc((fiber_entry_point_t)(void*)listener);
  struct fiber_closure clo = { .fiber = lfiber, .fd = servfd };
  fibers[servfd] = clo;
  fq_push(frontq, clo);

  // Start server loop
  bool keep_going = true;
  fiber_result_t status = FIBER_OK;
  wasi_print("Ready...\n");
  while (keep_going) {
    debug_println("main", servfd, "Ready to run fibers");
    printf("[main(%d)] clients = %d\n", servfd, clients);
    // Run ready fibers.
    for (uint32_t i = 0; i < frontq->length; i++) {
      /* debug_println("main", servfd, "Extracting fiber closure"); */
      struct fiber_closure clo = frontq->q[i];
      //debug_println("main", servfd, "Resuming");
      /* printf("[main(%d)] Resuming fiber %p\n", servfd, (void*)clo.fiber); */
      void *ans = fiber_resume(clo.fiber, (void*)clo.fd, &status);
      keep_going = handle_request(&wfd, rearq, clo, ans, status);
      if (!keep_going) break;
    }
    if (!keep_going) break;
    // Poll I/O
    uint32_t nevents;
    assert(wasio_poll(&wfd, &ev, MAX_CONNECTIONS, &nevents, 100) == WASIO_OK);
    debug_println("main", servfd, "Poll OK");
    if (nevents > 0) {
      for (uint32_t i = 0; i < wfd.length; i++) {
        if (wfd.vfds[i].fd != -1 && wfd.vfds[i].revents != 0) {
          wfd.vfds[i].revents = 0;
          wasio_fd_t fd = i;
          void *ans = fiber_resume(fibers[fd].fiber, (void*)0, &status);
          keep_going = handle_request(&wfd, rearq, fibers[fd], ans, status);
          if (!keep_going) break;
        }
      }
    }
    /* WASIO_EVENT_FOREACH(&wfd, &ev, nevents, fd, { */
    /*     /\* debug_println("main", fd, "Foreach"); *\/ */
    /*     // Run the fiber. */
    /*     void *ans = fiber_resume(fibers[fd].fiber, (void*)0, &status); */
    /*     keep_going = handle_request(&wfd, rearq, fibers[fd], ans, status); */
    /*     if (!keep_going) break; */
    /*   }); */

    // Swap queues and reset new rear.
    fq_swap(&frontq, &rearq);
    rearq->length = 0;
  }

  for (uint32_t i = 0; i < MAX_CONNECTIONS; i++) {
    if (fibers[i].fd != -1) {
      printf("Killing %u\n", i);
      printf("Resuming %u with kill signal\n", i);
      (void)fiber_resume(fibers[i].fiber, (void*)(intptr_t)FIBER_KILL_SIGNAL, &status); // TODO(dhil): Double check that the fiber exited.
      assert(status == FIBER_OK);
      fibers[i].fd = -1;
      free(fibers[i].fiber);
      if (i != 0) clients--;
    }
  }

  assert(clients == 0);
  free(frontq);
  free(rearq);
  wasio_finalize(&wfd);
  fiber_finalize();

  return 0;
}
