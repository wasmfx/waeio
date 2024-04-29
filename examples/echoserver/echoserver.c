#include <assert.h>
#include <host/errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <waeio.h>

#include <wasio.h>

static const uint32_t max_clients = 512;
static const uint32_t buffer_size = 4096;

int main(void) {
  uint32_t nbytes = 0;
  uint32_t nclients = 0;
  struct wasio_pollfd *wfd = (struct wasio_pollfd*)malloc(sizeof(struct wasio_pollfd)*max_clients);
  uint8_t **buffers = (uint8_t**)malloc(sizeof(uint8_t*)*max_clients);
  for (uint32_t i = 0; i < max_clients; i++)
    buffers[i] = NULL;
  struct wasio_event ev;
  wasio_fd_t sockfd, clientfd;
  assert(wasio_init(wfd, max_clients) == WASIO_OK);
  //printf("Setting up listener..\n");
  assert(wasio_listen(wfd, &sockfd, 8080, 64) == WASIO_OK && sockfd == 0);
  assert(wasio_notify_recv(wfd, sockfd) == WASIO_OK);
  //printf("Done\n");

  bool keep_going = true;
  while (keep_going) {
    uint32_t nevents = 0;
    wasio_result_t ans = wasio_poll(wfd, &ev, max_clients, &nevents, 1000);
    if (ans != WASIO_OK) {
      //printf("Poll failed: error(%d): %s\n", (int)host_errno, host_strerror(host_errno));
      break;
    }

    WASIO_EVENT_FOREACH(wfd, &ev, nevents, vfd, {
        printf("vfd: %lld\n", vfd);
        if (vfd == sockfd) {
          //printf("Attempting to accept new client...\n");
          ans = wasio_accept(wfd, sockfd, &clientfd);
          if (ans != WASIO_OK) {
            abort();
          }
          nclients++;
          assert(wasio_notify_recv(wfd, clientfd) == WASIO_OK);
          assert(wasio_notify_recv(wfd, sockfd) == WASIO_OK);
        } else if (buffers[vfd] == NULL) { // Ready to receive
          uint32_t nrecv = 0;
          buffers[vfd] = (uint8_t*)malloc(sizeof(uint8_t)*buffer_size);
          ans = wasio_recv(wfd, vfd, buffers[vfd], buffer_size, &nrecv);
          if (ans != WASIO_OK) {
            abort();
          }
          if (strncmp((const char*)buffers[vfd], "quit", strlen("quit")) == 0) {
            keep_going = false;
            while (nclients > 0) {
              wasio_close(wfd, nclients--);
            }
            break;
          } else {
            uint32_t nsent = 0;
            ans = wasio_send(wfd, vfd, buffers[vfd], nrecv, &nsent);
            if (ans == WASIO_ERROR && (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK)) {
              assert(wasio_notify_send(wfd, vfd) == WASIO_OK);
              continue;
            }
            if (ans == WASIO_OK && nsent < nrecv) {
              assert(wasio_notify_send(wfd, vfd) == WASIO_OK); // TODO(dhil): remember nrecv - nsent
              continue;
            }
            wasio_close(wfd, vfd);
          }
        } else { // Ready to send
          ans = wasio_send(wfd, vfd, buffers[vfd], buffer_size, &nbytes);
          if (ans != WASIO_OK) {
            abort();
          }
          free(buffers[vfd]);
          buffers[vfd] = NULL;
          wasio_close(wfd, vfd);
          nclients--;
        }
      });

    /* //printf("Awaiting connection...\n"); */
    /* wasio_fd_t clientfd; */
    /* host_errno = 0; */
    /* wasio_result_t ans = wasio_accept(wfd, sockfd, &clientfd); */
    /* //printf("errno: %d - %s\n", host_errno, host_strerror(host_errno)); */
    /* //printf("ans: %d\n", (int)ans); */
    /* if (ans == WASIO_ERROR) { */
    /*   //printf("ERROR\n"); */
    /*   if (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK) continue; */
    /*   //printf("FATAL ERROR\n"); */
    /*   abort(); */
    /* } */
    /* //printf("Got a connection!\n"); */
    /* do { */
    /*   host_errno = 0; */
    /*   ans = wasio_recv(wfd, clientfd, readbuf, read_buffer_size, &nbytes); */
    /*   if (ans < 0) { */
    /*     //printf("errno: %d - %s\n", host_errno, host_strerror(host_errno)); */
    /*     goto cleanup; */
    /*   } */
    /*   memcpy(writebuf, readbuf, nbytes); */
    /* } while (ans == WASIO_ERROR && (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK)); */

    /* do { */
    /*   host_errno = 0; */
    /*   ans = wasio_send(wfd, clientfd, writebuf, nbytes, &nbytes); */
    /*   if (ans < 0) { */
    /*     //printf("errno: %d - %s\n", host_errno, host_strerror(host_errno)); */
    /*     goto cleanup; */
    /*   } */
    /* } while (ans == WASIO_ERROR && (host_errno == HOST_EAGAIN || host_errno == HOST_EWOULDBLOCK)); */
    /* wasio_close(wfd, clientfd); */
    /* keep_going = false; */
  }
  wasio_close(wfd, sockfd);
  wasio_finalize(wfd);
  free(wfd);
  free(buffers);
  return 0;
}
