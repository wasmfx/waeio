#include <assert.h>
#include <host/errno.h>
#include <host/poll.h>
#include <host/socket.h>
#include <http_utils.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
  int32_t len = 0, rc = 0;
  int32_t listen_sd = -1, new_sd = -1;
  bool end_server = false, compress_array = false;
  bool close_conn;
  const int buffer_size = 2048;
  uint8_t reqbuf[buffer_size], resbuf[buffer_size];
  struct pollfd fds[200];
  uint32_t nfds = 1, current_size = 0;

  // Set up listener
  listen_sd = host_listen(8080, 1000, &host_errno);
  if (listen_sd < 0) {
    perror("socket() failed");
    exit(-1);
  }

  // Initialise poll structure
  memset(fds, 0 , sizeof(fds));
  fds[0].fd = listen_sd;
  fds[0].events = HOST_POLLIN;

  printf("Ready...\n");
  // Request loop
  do {
    // Poll for incoming requests
    conn_log("Waiting on poll()...\n");
    rc = host_poll(fds, nfds, 3 * 60 * 1000, &host_errno);

    // Check whether an I/O error occurred
    if (rc < 0) {
      perror("  poll() failed");
      break;
    }

    // Check whether poll timed out
    if (rc == 0){
      conn_log("  poll() timed out.  End program.\n");
      break;
    }

    // One or more descriptors are ready
    current_size = nfds;
    for (uint32_t i = 0; i < current_size; i++)
    {
      // Ignore those with no revents
      if(fds[i].revents == 0) continue;

      // Expect revent to be POLLIN.
      if(fds[i].revents != HOST_POLLIN) {
        conn_log("  Error! revents = %d\n", fds[i].revents);
        end_server = true;
        break;
      }

      // If the listener is ready then accept all incoming connections
      if (fds[i].fd == listen_sd) {
        conn_log("  Listening socket is readable\n");

        do {
          new_sd = host_accept(listen_sd, &host_errno);
          if (new_sd < 0) {
            if (host_errno != HOST_EWOULDBLOCK) {
              perror("  accept() failed");
              end_server = true;
            }
            break;
          }

          // Add the new connection to the poll structure.
          conn_log("  New incoming connection - %d\n", new_sd);
          fds[nfds].fd = new_sd;
          fds[nfds].events = HOST_POLLIN;
          nfds++;
        } while (new_sd != -1);
      } else {
        // Otherwise it must be that a client connection is readable
        conn_log("  Descriptor %d is readable\n", fds[i].fd);
        close_conn = false;
        do {
          // Receive incoming data
          rc = host_recv(fds[i].fd, (uint8_t*)reqbuf, 2048, &host_errno);
          if (rc < 0) {
            if (host_errno != HOST_EWOULDBLOCK) {
              perror("  recv() failed");
              close_conn = true;
            }
            break;
          }

          // Was the connection closed by the client?
          if (rc == 0) {
            conn_log("  Connection closed\n");
            close_conn = true;
            break;
          }

          // Otherwise we must have received data
          len = rc;
          conn_log("  %d bytes received\n", len);

          // Echo the request back as a response
          rc = make_response(resbuf, buffer_size, "200 OK", reqbuf, len);
          if (rc < 0) {
            perror("  response generation failed");
            close_conn = true;
            break;
          }
          rc = host_send(fds[i].fd, (uint8_t*)resbuf, rc, &host_errno);
          if (rc < 0) {
            perror("  send() failed");
            close_conn = true;
            break;
          }

        } while(true);

        // Clean up
        if (close_conn) {
          host_close(fds[i].fd, &host_errno);
          fds[i].fd = -1;
          compress_array = true;
        }
      }
    }

    // Compress the poll structure if necessary
    if (compress_array) {
      compress_array = false;
      for (uint32_t i = 0; i < nfds; i++) {
        if (fds[i].fd == -1) {
          for (uint32_t j = i; j < nfds-1; j++) {
            fds[j].fd = fds[j+1].fd;
          }
          i--;
          nfds--;
        }
      }
    }

  } while (end_server == false);

  // Clean up open sockets
  for (uint32_t i = 0; i < nfds; i++) {
    if (fds[i].fd >= 0)
      host_close(fds[i].fd, &host_errno);
  }

  return 0;
}
