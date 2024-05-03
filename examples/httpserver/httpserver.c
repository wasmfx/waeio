#include <assert.h>
#include <host/errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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


/* #define debug_println(fn, fd, msg) do {               \ */
/*     printf("[%s(%d)] %s\n", fn, fd, msg); \ */
/*     fflush(stdout);\ */
/*   } while(false); */
#define debug_println(fn, fd, msg) {};

static const uint32_t buffer_size = 1 << 16;
/* static const uint32_t max_headers = 100; */
static const uint32_t max_clients = 500;
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

/* static inline bool fq_is_empty(const struct fiber_queue *fq) { */
/*   return fq->length == 0; */
/* } */

static inline void fq_swap(struct fiber_queue **frontq, struct fiber_queue **rearq) {
  struct fiber_queue *q = *frontq;
  *frontq = *rearq;
  *rearq = q;
}

/* static inline struct fiber_closure fq_pop(struct fiber_queue *fq) { */
/*   if (!fq_is_empty(fq)) { */
/*     fq->q[fq->length++] = clo; */
/*   } else { */
/*     abort(); */
/*   } */
/* } */


static struct wasio_pollfd wfd;
static struct wasio_event ev;
static struct fiber_closure fibers[500];

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

static inline void yield() {
  cmd = cmd_make(YIELD, 0);
  (void)fiber_yield(&cmd);
}

static int32_t w_recv(struct wasio_pollfd *wfd, wasio_fd_t fd, uint8_t *buf, uint32_t bufsize) {
  cmd = cmd_make(RECV, fd);
  uint32_t nbytes;
  wasio_result_t ans;
  /* printf("[w_recv(%d)] receiving\n", fd); */
  while ( (ans = wasio_recv(wfd, fd, buf, bufsize, &nbytes)) == WASIO_EAGAIN ) {
    debug_println("w_recv", fd, "yielding");
    (void)fiber_yield(&cmd);
    debug_println("w_recv", fd, "continued");
  }
  return ans == WASIO_OK ? nbytes : -1;
}

static int32_t w_send(struct wasio_pollfd *wfd, wasio_fd_t fd, uint8_t *buf, uint32_t bufsize) {
  cmd = cmd_make(SEND, fd);
  uint32_t nbytes;
  wasio_result_t ans;
  while ( (ans = wasio_send(wfd, fd, buf, bufsize, &nbytes)) == WASIO_EAGAIN ) {
    debug_println("w_send", fd, "yielding");
    (void)fiber_yield(&cmd);
    debug_println("w_send", fd, "continued");
  }
  return ans == WASIO_OK ? nbytes : -1;
}

static wasio_fd_t w_accept(struct wasio_pollfd *wfd, wasio_fd_t fd) {

  while (true) {
    wasio_fd_t conn = -1;
    wasio_result_t ans = wasio_accept(wfd, fd, &conn);
    /* switch (ans) { */
    /* case WASIO_OK: */
    /*   debug_println("w_accept", fd, "ans is OK"); */
    /*   break; */
    /* case WASIO_EAGAIN: */
    /*   debug_println("w_accept", fd, "ans is EAGAIN"); */
    /*   break; */
    /* default: */
    /*   debug_println("w_accept", fd, "ans is UNKNOWN"); */
    /* } */
    if (ans != WASIO_EAGAIN) return ans == WASIO_OK ? conn : -1;
    cmd = cmd_make(RECV, fd);
    debug_println("w_accept", fd, "yielding");
    (void)fiber_yield(&cmd);
    debug_println("w_accept", fd, "continued");
  }
  abort();
  return -1;
}

static const char *daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int prepare_response(char *buffer, size_t buff_size, const char *body,
    int content_length) {
    size_t response_length = 0;
    time_t t = time(NULL);
    // DO NOT use localtime
    struct tm *tm = gmtime(&t);

    response_length +=
        snprintf(buffer + response_length, buff_size - response_length, "HTTP/1.1 200 OK");
    response_length += snprintf(buffer + response_length, buff_size - response_length, "\r\n");
    // Date: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
    // This is considerable faster than strftime
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n", daysOfWeek[tm->tm_wday], tm->tm_mday,
        months[tm->tm_mon], tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
    // TODO, maybe allow closing?
    response_length += snprintf(buffer + response_length, buff_size - response_length,
                                "Connection: close\r\n");
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Content-Length: %d\r\n", content_length);
    response_length += snprintf(buffer + response_length, buff_size - response_length,
        "Content-Type: text/plain\r\n");
    response_length += snprintf(buffer + response_length, buff_size - response_length, "\r\n");
    if (content_length > 0) {
        response_length +=
            snprintf(buffer + response_length, buff_size - response_length, "%s", body);
    }

    if (response_length < 0 || response_length >= buff_size) {
        return -1;
    }

    return response_length;
}

const char *response_body =
    "CHAPTER I. Down the Rabbit-Hole  Alice was beginning to get very tired of sitting by her "
    "sister on the bank, and of having nothing to do: once or twice she had peeped into the book "
    "her sister was reading, but it had no pictures or conversations in it, <and what is the use "
    "of a book,> thought Alice <without pictures or conversations?> So she was considering in her "
    "own mind (as well as she could, for the hot day made her feel very sleepy and stupid), "
    "whether the pleasure of making a daisy-chain would be worth the trouble of getting up and "
    "picking the daisies, when suddenly a White Rabbit with pink eyes ran close by her. There was "
    "nothing so very remarkable in that; nor did Alice think it so very much out of the way to "
    "hear the Rabbit say to itself, <Oh dear! Oh dear! I shall be late!> (when she thought it over "
    "afterwards, it occurred to her that she ought to have wondered at this, but at the time it "
    "all seemed quite natural); but when the Rabbit actually took a watch out of its "
    "waistcoat-pocket, and looked at it, and then hurried on, Alice started to her feet, for it "
    "flashed across her mind that she had never before seen a rabbit with either a "
    "waistcoat-pocket, or a watch to take out of it, and burning with curiosity, she ran across "
    "the field after it, and fortunately was just in time to see it pop down a large rabbit-hole "
    "under the hedge. In another moment down went Alice after it, never once considering how in "
    "the world she was to get out again. The rabbit-hole went straight on like a tunnel for some "
    "way, and then dipped suddenly down, so suddenly that Alice had not a moment to think about "
    "stopping herself before she found herself falling down a very deep well. Either the well was "
    "very deep, or she fell very slowly, for she had plenty of time as she went down to look about "
    "her and to wonder what was going to happen next. First, she tried to look down and make out "
    "what she was coming to, but it was too dark to see anything; then she looked at the sides of "
    "the well, and noticed that they were filled with cupboards......\n";


static void* handle_connection(wasio_fd_t clientfd) {
  char reqbuf[buffer_size];
  /* const char *method; */
  /* size_t method_len; */
  /* const char *path; */
  /* size_t path_len; */
  /* int minor_version; */
  /* struct phr_header headers[max_headers]; */
  /* size_t num_headers; */
  size_t prevbuflen = 0, buflen = 0;

  int32_t ans = 0;
  while (true) {
    ans = w_recv(&wfd, clientfd, (uint8_t*)reqbuf+buflen, sizeof(reqbuf) - buflen);
    /* printf("[handle_connection(%d)] bytes recv'd: %d\n", clientfd, ans); */
    /* printf("[handle_connection(%d)] request:\n%s\n", clientfd, reqbuf); */
    if (ans < 0) {
      debug_println("handle_connection", clientfd, "I/O recv failure");
      abort();
    }
    prevbuflen = buflen;
    buflen += ans;

    /* ans = phr_parse_request(reqbuf, buflen, &method, &method_len, &path, &path_len, */
    /*                       &minor_version, headers, &num_headers, prevbuflen); */
    ans = strncmp(reqbuf, "GET / HTTP/1.1", strlen("GET / HTTP/1.1")) == 0 ? 0 : strncmp(reqbuf, "GET /favicon.ico HTTP/1.1", strlen("GET /favicon.ico HTTP/1.1"));
    if (ans == 0) break;
    else if (ans == -1) {
      debug_println("handle_connection", clientfd, "Http parse failure");
      abort();
    }
    assert(ans == -2);
    (void)prevbuflen;
    /* if (buflen == buffer_size) abort(); */
  }

  if (ans >= 0) {
    ans = prepare_response(reqbuf, buffer_size, response_body, strlen(response_body));
    if (ans < 0) {
      debug_println("handle_connection", clientfd, "failed to prepare response");
      abort();
    }
    ans = w_send(&wfd, clientfd, (uint8_t*)reqbuf, ans);
    if (ans < 0) {
      debug_println("handle_connection", clientfd, "I/O send failure");
      abort();
    }
  } else { // FAILED
    debug_println("handle_connection", clientfd, "failure");
    abort();
  }

  return NULL;
}

void* listener(wasio_fd_t sockfd) {
  while (true) {
    // Keep yielding if we are out of space.
    while (clients == max_clients) {
      yield();
    }

    wasio_fd_t conn = w_accept(&wfd, sockfd);
    /* printf("[listener(%d)] accepted %d\n", sockfd, conn); */
    if (conn < 0) {
      debug_println("listener", sockfd, "I/O failure");
      break;
    }
    clients++;

    cmd.clo.fiber = fiber_alloc((fiber_entry_point_t)(void*)handle_connection);
    cmd.clo.fd = conn;
    cmd.tag = ASYNC;
    debug_println("listener", sockfd, "yielding");
    int ans = (int)fiber_yield(&cmd);
    debug_println("listener", sockfd, "continued");
    if (ans < 0) break;
  }
  return NULL;
}

static bool handle_request(struct wasio_pollfd *wfd, struct fiber_queue *rearq, struct fiber_closure clo, void *ret __attribute__((unused)), fiber_result_t res) {
  debug_println("handle_request", clo.fd, "entered");
  switch (res) {
  case FIBER_OK:
    debug_println("handle_request", clo.fd, "FIBER_OK");
    assert(wasio_close(wfd, clo.fd) == WASIO_OK);
    fiber_free(clo.fiber);
    clients--;
    return true;
    break;
  case FIBER_ERROR:
    debug_println("handle_request", clo.fd, "FIBER_ERROR");
    abort();
  case FIBER_YIELD: {
    debug_println("handle_request", clo.fd, "FIBER_YIELD");
    switch (cmd.tag) {
    case ASYNC:
      debug_println("handle_request", clo.fd, " -> ASYNC");
      debug_println("handle_request", cmd.clo.fd, " <- ASYNC");
      fibers[cmd.clo.fd] = cmd.clo;
      fq_push(rearq, cmd.clo); // fall-through
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
    case QUIT:
      debug_println("handle_request", clo.fd, " -> QUIT");
      return false;
    default:
      debug_println("handle_request", clo.fd, " -> UNKNOWN");
      abort();
    }
  }
  }
  return false;
}

int main(void) {
  // Setup fiber queues.
  struct fiber_queue *frontq = fq_new(max_clients),
                     *rearq = fq_new(max_clients);

  // Initialise the I/O subsystem.
  assert(wasio_init(&wfd, max_clients) == WASIO_OK);

  // Open the listener socket.
  wasio_fd_t servfd = -1;
  const uint32_t backlog = 64;
  assert(wasio_listen(&wfd, &servfd, 8080, backlog) == WASIO_OK);

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
    /* printf("[main(%d)] clients = %d\n", servfd, clients); */
    // Run ready fibers.
    for (uint32_t i = 0; i < frontq->length; i++) {
      debug_println("main", servfd, "Extracting fiber closure");
      struct fiber_closure clo = frontq->q[i];
      debug_println("main", servfd, "Resuming");
      void *ans = fiber_resume(clo.fiber, (void*)clo.fd, &status);
      keep_going = handle_request(&wfd, rearq, clo, ans, status);
      if (!keep_going) break;
    }
    if (!keep_going) break;

    // Poll I/O
    uint32_t nevents;
    assert(wasio_poll(&wfd, &ev, max_clients, &nevents, 1000) == WASIO_OK);
    debug_println("main", servfd, "Poll OK");
    WASIO_EVENT_FOREACH(&wfd, &ev, nevents, fd, {
        debug_println("main", fd, "Foreach");
        // Run the fiber.
        void *ans = fiber_resume(fibers[fd].fiber, (void*)0, &status);
        keep_going = handle_request(&wfd, rearq, fibers[fd], ans, status);
        if (!keep_going) break;
      });

    // Swap queues and reset new rear.
    fq_swap(&frontq, &rearq);
    rearq->length = 0;
  }

  wasio_finalize(&wfd);

  return 0;
}
