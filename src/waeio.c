#include <fiber.h>
#include <unistd.h>
#include <waeio.h>
#include <wasio.h>

enum io_command {
  ACCEPT,
  RECV,
  SEND
};

typedef struct io_command {
  enum io_command tag;
  int fd;
  union {
    struct {
      struct sockaddr *addr;
      socklen_t *addr_len;
    };
    struct {
      char *buf;
      size_t len;
    }
  };
} io_cmd_t;


typedef struct waeio_state {
  fiber_t blocked;
} waeio_state_t;

int waeio_run(void (*main)(void)) {
  return 0;
}

int waeio_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addr_len) {
  return wasio_accept(fd, addr, addr_len);
}

int waeio_recv(int fd, char *buf, size_t len) {
  return wasio_recv(fd, buf, len);
}

int waeio_send(int fd, char *buf, size_t len) {
  return wasio_send(fd, buf, len);
}

int waeio_close(int fd) {
  return wasio_close(fd);
}
