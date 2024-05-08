#include <fiber.h>
#include <host/errno.h>
#include <host/poll.h>
#include <host/socket.h>
#include <http_utils.h>
#include <inttypes.h>
#include <limits.h>
#include <picohttpparser.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wassert.h>

#define FIBER_KILL_SIGNAL INT32_MIN

struct fiber_closure {
  fiber_t fiber;
  int32_t fd;
};

static int32_t timeout = 3 * 10 * 1000; // 30 secs
static uint32_t nfds = 0;
static bool end_server = false;
static struct fiber_closure fibers[MAX_CONNECTIONS];
static struct pollfd fds[MAX_CONNECTIONS];

// NOTE(dhil): The following variables ought to be local to
// `handle_connection`, however, due to the bad interaction between
// stack switching and the shadow stack it is currently necessary to
// hoist them to the global scope.
#define BUFFER_SIZE 8192
#define MAX_HEADERS 100
static uint8_t reqbuf[BUFFER_SIZE], resbuf[BUFFER_SIZE];
static const char *method;
static size_t method_len;
static const char *path;
static size_t path_len;
static int minor_version;
static struct phr_header headers[MAX_HEADERS];

static inline void reset_globals(void) {
  method = NULL;
  path = NULL;
  path_len = 0;
  minor_version = 0;
  memset(headers, 0, sizeof(struct phr_header)*MAX_HEADERS);
  memset(reqbuf, '\0', sizeof(uint8_t)*BUFFER_SIZE);
  memset(resbuf, '\0', sizeof(uint8_t)*BUFFER_SIZE);
}

static void* handle_connection(int32_t fd) {
  // Receive incoming data
  while (true) {
    int32_t rc = 0;
    while (true) {
      // NOTE(dhil): Since the data is global shared we reset
      // everything on entry.
      reset_globals();
      rc = host_recv(fd, (uint8_t*)reqbuf, BUFFER_SIZE, &host_errno);
      // Was the connection closed by the client?
      if (rc == 0) {
        conn_log("  [handle_connection(%" PRIi32 ")] Connection closed\n", fd);
        return NULL;
      } else if (rc < 0) {
        if (host_errno != HOST_EAGAIN) {
          conn_log("  [handle_connection(%" PRIi32 ")] recv() failed\n", fd);
          return NULL;
        }
        conn_logv("  [handle_connection(%" PRIi32 ")] yielding\n", fd);
        fd = (int32_t)(intptr_t)fiber_yield(NULL);
        conn_logv("  [handle_connection(%" PRIi32 ")] continued with %" PRIi32 "\n", fd, fd);
        if (fd == FIBER_KILL_SIGNAL) return NULL;
      } else {
        break;
      }
    }

    // Otherwise we must have received data
    conn_logv("  [handle_connection(%" PRIi32 ")] received %" PRIi32 " bytes\n", fd, rc);

    // Parse http request
    size_t prevbuflen = 0, buflen = 0;
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);
    conn_log("  [handle_connection(%" PRIi32 ")] parsing request...", fd);
    rc = phr_parse_request((const char*)reqbuf, sizeof(reqbuf) - buflen, &method, &method_len, &path, &path_len,
                           &minor_version, headers, &num_headers, prevbuflen);
    if (num_headers <= 0) {
      conn_log(" no headers!");
      return NULL;
    }

    if (rc > 0) {
      if (path_len == 1 && strncmp(path, "/", 1) == 0) {
        conn_log(" request OK / \n");
        rc = response_ok((uint8_t*)resbuf, BUFFER_SIZE, (uint8_t*)response_body, (uint32_t)strlen(response_body)); // OK
      } else if (path_len == strlen("/quit") && strncmp(path, "/quit", strlen("/quit")) == 0) {
        conn_log(" request OK /quit\n");
        rc = response_ok((uint8_t*)resbuf, BUFFER_SIZE, (uint8_t*)"OK bye...\n", (uint32_t)strlen("OK bye...\n")); // Quit
        end_server = true;
      } else {
        conn_log(" request Not Found\n");
        rc = response_notfound((uint8_t*)resbuf, BUFFER_SIZE, NULL, 0); // Not found
      }
    } else if (rc == -1) { // Parse failure
      conn_log(" request parse failure\n");
      rc = response_badrequest((uint8_t*)resbuf, BUFFER_SIZE, NULL, 0); // Parse error
    } else { // Partial parse
      conn_log(" partial request parse\n");
      wassert(rc == -2);
      rc = response_toolarge((uint8_t*)resbuf, BUFFER_SIZE, NULL, 0);
    }

    if (rc == -1) {
      conn_log("  [handle_connection(%" PRIi32 ")] response generation failed\n", fd);
      return NULL;
    }

    // Send the response
    rc = host_send(fd, (uint8_t*)resbuf, rc, &host_errno);
    if (rc < 0) {
      conn_log("  [handle_connection(%" PRIi32 ")] send() failed\n", fd);
    }
    if (end_server) return NULL;
  }

  return NULL;
}

// NOTE(dhil): It appears the toolchain performs some optimisation
// that inadvertently sets `fd = 0` after a yield. To mitigate this,
// we can either use a global variable, make `fd` a static variable
// inside the function, or return the fd on each yield (effectively
// making each fiber routine a reader monad). I have opted for the
// last option.
// NOTE(dhil): The described phenomenon is most likely due to the
// shadow stack pointer getting out of sync with the Wasm stack
// pointer when stack switching.
static void* listener(int32_t fd) {
  while (fd != FIBER_KILL_SIGNAL) {
    int32_t new_fd = -1;
    while (nfds == MAX_CONNECTIONS) {
      conn_logv("  [listener(%" PRIi32 ")] queue is full, yielding.\n", fd);
      fd = (int32_t)(intptr_t)fiber_yield(NULL); // TODO.
      if (fd == FIBER_KILL_SIGNAL) {
        conn_logv("  [listener(%" PRIi32 ")] exiting\n", fd);
        return NULL;
      }
    }

    // Accept all incoming requests
    do {
      conn_logv("  [listener(%" PRIi32 ")] accept()\n", fd);
      new_fd = host_accept(fd, &host_errno);
      if (new_fd < 0) {
        if (host_errno != HOST_EAGAIN) {
          conn_log("  [listener(%" PRIi32 ")] accept() failed: %s\n", fd, host_strerror(host_errno));
          end_server = true;
          return NULL;
        }
        conn_logv("  [listener(%" PRIi32 ")] yielding\n", fd);
        fd = (int32_t)(intptr_t)fiber_yield(NULL); // TODO.
        conn_logv("  [listener(%" PRIi32 ")] continued with %" PRIi32 "\n", fd, fd);
        if (fd == FIBER_KILL_SIGNAL) {
          conn_logv("  [listener(%" PRIi32 ")] exiting\n", fd);
          return NULL;
        }
      } else {
        // Add the new connection to the poll structure.
        conn_log("  [listener(%" PRIi32 ")] new incoming connection: %" PRIi32 "\n", fd, new_fd);
        fds[nfds].fd = new_fd;
        fds[nfds].events = HOST_POLLIN;
        fibers[nfds] = (struct fiber_closure){ .fiber = fiber_alloc((fiber_entry_point_t)(void*)handle_connection), .fd = new_fd };
        nfds++;
      }
      conn_logv("  [listener(%" PRIi32 ")] connections: %" PRIu32 "\n", fd, nfds);
    } while (nfds < MAX_CONNECTIONS && new_fd != -1);
  }
  return NULL;
}

static bool handle_command(uint32_t i, struct fiber_closure clo, void *payload __attribute__((unused)), fiber_result_t status) {
  switch (status) {
  case FIBER_OK:
    conn_logv("[handle_command] fiber(%" PRIi32 ") finished\n", clo.fd);
    fiber_free(clo.fiber);
    host_close(clo.fd, &host_errno);
    fds[i].fd = -1;
    nfds--;
    return true;
  case FIBER_YIELD:
    conn_logv("[handle_command] fiber(%" PRIi32 ") yielded\n", clo.fd);
    fds[i].events = HOST_POLLIN;
    return false;
  case FIBER_ERROR:
  default:
    conn_logv("[handle_command] fiber(%" PRIi32 ") error\n", clo.fd);
    fiber_free(clo.fiber);
    host_close(clo.fd, &host_errno);
    fds[i].fd = -1;
    nfds--;
    end_server = true;
    return false;
  }
}

int main(void) {
  fiber_init();

  // Set up listener
  int32_t listen_fd = host_listen(8080, 1000, &host_errno);
  if (listen_fd < 0) {
    conn_log("socket() failed\n");
    exit(-1);
  }
  conn_logv("[main] listener is bound to socket %" PRIi32 "\n", listen_fd);

  // Initialise poll structure
  for (uint32_t i = 0; i < MAX_CONNECTIONS; i++) {
    fds[i] = (struct pollfd){ .fd = -1, .events = 0, .revents = 0 };
  }
  fds[0].fd = listen_fd;
  fds[0].events = HOST_POLLIN;
  nfds++;

  // Allocate fiber for listener
  fiber_t listener_fiber = fiber_alloc((fiber_entry_point_t)(void*)listener);
  fibers[0] = (struct fiber_closure){ .fiber = listener_fiber, .fd = listen_fd };

  printf("[main] ready...\n");

  // Request loop
  while (!end_server) {
    uint32_t pollfd_size = 0;
    bool compress_pollfd = false;
    conn_log("[main] waiting on poll()...\n");
    int32_t rc = host_poll(fds, nfds, timeout, &host_errno);
    if (rc <= 0) {
      if (rc == 0) {
        conn_log("[main] poll timed out... shutting down\n");
        end_server = true;
        break;
      }
      conn_log("  [main] poll() failed: error(%" PRIi32 "): %s\n", host_errno, host_strerror(host_errno));
      break;
    }
    pollfd_size = nfds;
    for (uint32_t i = 0; i < pollfd_size; i++) {
      if (fds[i].revents == 0) continue;

      if (fds[i].revents & HOST_POLLHUP) {
        conn_log("  [main] connection %" PRIi32 " hung up\n", fds[i].fd);
        fiber_free(fibers[i].fiber);
        fibers[i].fd = -1;
        fds[i].fd = -1;
        compress_pollfd = true;
        nfds--;
        continue;
      }

      if ((fds[i].revents & HOST_POLLIN) == 0) {
        conn_log("  [main] error! revents = %d\n", fds[i].revents);
        end_server = true;
        break;
      }

      // Resume fiber.
      conn_log("[main] descriptor %" PRIi32 " is readable.. resuming fiber\n", fds[i].fd);
      fiber_result_t status = FIBER_ERROR;
      void *ans = fiber_resume(fibers[i].fiber, (void*)(intptr_t)fibers[i].fd, &status);
      compress_pollfd = handle_command(i, fibers[i], ans, status);
      if (compress_pollfd) fds[i].fd = -1;
    }

    if (compress_pollfd) {
      for (uint32_t i = 0; i < nfds; i++) {
        if (fds[i].fd == -1) {
          for (uint32_t j = i; j < nfds-1; j++) {
            fds[j].fd = fds[j+1].fd;
            fibers[j] = fibers[j+1];
          }
          i--;
          nfds--;
        }
      }
    }
  }

  // Clean up
  conn_logv("[main] nfds = %u\n", nfds);
  wassert(0 < nfds && nfds <= MAX_CONNECTIONS);
  for (uint32_t i = 0; i < nfds; i++) {
    if (fds[i].fd == -1) continue;

    fiber_result_t status = FIBER_ERROR;
    conn_logv("[main] killing %" PRIu32 " -> %" PRIi32 "\n", i, fibers[i].fd);
    (void)fiber_resume(fibers[i].fiber, (void*)(intptr_t)FIBER_KILL_SIGNAL, &status);
    wassert(status == FIBER_OK);
    host_close(fds[i].fd, &host_errno);
    nfds--;
  }
  wassert(nfds == 0);
  fiber_finalize();

  return 0;
}

#undef BUFFER_SIZE
#undef MAX_HEADERS
#undef FIBER_KILL_SIGNAL
