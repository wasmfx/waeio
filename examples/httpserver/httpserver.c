#include <assert.h>
#include <host/errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <picohttpparser.h>

//#include <waeio.h>

#include <wasio.h>

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
    "the well, and noticed that they were filled with cupboards......";

static const uint32_t max_clients = 512;
static const uint32_t buffer_size = 4096; // 64 * 1024

static const uint32_t max_headers = 100;

__attribute__((unused))
static void* handle_request(void *arg __attribute__((unused))) {
  const char reqbuf[buffer_size];
  const char *method;
  size_t method_len;
  const char *path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[max_headers];
  size_t num_headers;
  size_t prevbuflen = 0;
  int ans = phr_parse_request(reqbuf, buffer_size, &method, &method_len, &path, &path_len,
                              &minor_version, headers, &num_headers, prevbuflen);
  (void)ans;
  return NULL;
}

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
