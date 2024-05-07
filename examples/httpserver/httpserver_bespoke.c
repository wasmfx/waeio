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

#include <picohttpparser.h>

int main(void) {
  // For connection management
  int32_t rc = 0;
  int32_t listen_sd = -1, new_sd = -1;
  bool end_server = false, compress_array = false;
  bool close_conn;
  const int buffer_size = 4096;
  uint8_t reqbuf[buffer_size], resbuf[buffer_size];
  struct pollfd fds[MAX_CONNECTIONS];
  uint32_t nfds = 1, current_size = 0;

  // For http parser management
  const char *method;
  size_t method_len;
  const char *path;
  size_t path_len;
  int minor_version;
  const uint32_t max_headers = 100;
  struct phr_header headers[max_headers];

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
      if((fds[i].revents & HOST_POLLIN) == 0) {
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
            if (host_errno != HOST_EAGAIN) {
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
          conn_log("  %d bytes received\n", rc);

          // Parse http request
          size_t prevbuflen = 0, buflen = 0;
          size_t num_headers = sizeof(headers) / sizeof(headers[0]);
          rc = phr_parse_request((const char*)reqbuf, sizeof(reqbuf) - buflen, &method, &method_len, &path, &path_len,
                                 &minor_version, headers, &num_headers, prevbuflen);

          if (rc > 0) {
            if (path_len == 1 && strncmp(path, "/", 1) == 0) {
              conn_log("  request OK /\n");
              rc = response_ok((uint8_t*)resbuf, buffer_size, (uint8_t*)response_body, (uint32_t)strlen(response_body)); // OK
            } else if (path_len == strlen("/quit") && strncmp(path, "/quit", strlen("/quit")) == 0) {
              conn_log("  request OK /quit\n");
              rc = response_ok((uint8_t*)resbuf, buffer_size, (uint8_t*)"OK bye...\n", (uint32_t)strlen("OK bye...\n")); // Quit
              close_conn = true;
              end_server = true;
            } else {
              conn_log("  request Not Found\n");
              rc = response_notfound((uint8_t*)resbuf, buffer_size, NULL, 0); // Not found
            }
          } else if (rc == -1) { // Parse failure
            conn_log("  request parse failure\n");
            rc = response_badrequest((uint8_t*)resbuf, buffer_size, NULL, 0); // Parse error
          } else { // Partial parse
            conn_log("  partial request parse\n");
            assert(rc == -2);
            rc = response_toolarge((uint8_t*)resbuf, buffer_size, NULL, 0);
          }

          if (rc == -1) {
            perror("  response generation failed");
            close_conn = true;
            break;
          }

          // Send the response
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
