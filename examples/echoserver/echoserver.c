#include <assert.h>
#include <host/errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <waeio.h>

#include <wasio.h>

int main(void) {
  uint8_t buf[256];
  uint32_t nbytes = 0;
  struct wasio_pollfd wfd;
  wasio_fd_t sockfd;
  printf("Setting up listener..\n");
  assert(wasio_listen(&wfd, &sockfd) == WASIO_OK);
  printf("Done\n");

  bool keep_going = true;
  while (keep_going) {
    printf("Awaiting connection...\n");
    wasio_fd_t clientfd;
    host_errno = 0;
    wasio_result_t ans = wasio_accept(&wfd, sockfd, &clientfd);
    printf("errno: %d - %s\n", host_errno, host_strerror(host_errno));
    printf("ans: %d\n", (int)ans);
    if (ans == WASIO_ERROR) {
      printf("ERROR\n");
      if (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK) continue;
      printf("FATAL ERROR\n");
      abort();
    }
    printf("Got a connection!\n");
    do {
      host_errno = 0;
      ans = wasio_recv(&wfd, clientfd, buf, 256, &nbytes);
      if (ans < 0) {
        printf("errno: %d - %s\n", host_errno, host_strerror(host_errno));
      }
    } while (ans == WASIO_ERROR || (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK));

    do {
      host_errno = 0;
      ans = wasio_send(&wfd, clientfd, buf, nbytes, &nbytes);
      if (ans < 0) {
        printf("errno: %d - %s\n", host_errno, host_strerror(host_errno));
      }
    } while (ans == WASIO_ERROR || (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK));
    keep_going = false;
  }

  return 0;
}
