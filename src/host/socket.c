// Host-defined socket implementation

#include <arpa/inet.h>
#include <error.h>
#include <fcntl.h>
#include <host/socket.h>
#include <host/wasmtime_utils.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasm.h>
#include <wasmtime.h>

static wasm_functype_t *listen_sig = NULL;  // i32 i32 i32 -> i64
static wasmtime_func_t listenfn;
static wasmtime_extern_t listenex;

static wasm_functype_t *connect_sig = NULL;  // i32 i32 i32 i32 -> i64
static wasmtime_func_t connectfn;
static wasmtime_extern_t connectex;

static wasm_functype_t *accept_sig = NULL; // i64 i32 -> i64
static wasmtime_func_t acceptfn;
static wasmtime_extern_t acceptex;

static wasm_functype_t *send_sig = NULL; // i64 i32 i32 i32 -> i32
static wasmtime_func_t sendfn;
static wasmtime_extern_t sendex;

static wasm_functype_t *recv_sig = NULL; // i64 i32 i32 i32 -> i32
static wasmtime_func_t recvfn;
static wasmtime_extern_t recvex;

static wasm_functype_t *close_sig = NULL;   // i64 i32 -> i32
static wasmtime_func_t closefn;
static wasmtime_extern_t closeex;

DEFINE_BINDING(host_connect) {
  assert(nargs == 4);
  assert(nresults == 1);

  // Unpack addr and port args.
  int32_t addr_offset = int32_t_of_wasmtime_val_t(args[0]);
  int32_t addr_len = int32_t_of_wasmtime_val_t(args[1]);
  int32_t port = int32_t_of_wasmtime_val_t(args[2]);

  // Load memory.
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_connect");

  // Create and connect the socket.
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    WRITE_ERRNO("host_connect", 2);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  assert((mem+addr_offset+addr_len) != NULL && *((char*)(mem+addr_offset+addr_len)) == '\0');
  addr.sin_addr.s_addr = inet_addr((char*)(mem+addr_offset)); // Read the address string.
  addr.sin_port = htons((int)port);

  int ans = connect((int)sockfd, (struct sockaddr *)&addr, sizeof(addr));
  if (ans < 0) {
    WRITE_ERRNO("host_connect", 3);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  // Set nonblocking
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != 0) {
    WRITE_ERRNO("host_connect", 3);
  }

  return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
}

DEFINE_BINDING(host_accept) {
  assert(nargs == 2);
  assert(nresults == 1);

  // Unpack socket fd.
  int64_t sockfd = int64_t_of_wasmtime_val_t(args[0]);

  // Perform the system call.
  int ans = accept(sockfd, NULL, 0);

  if (ans < 0) {
    WRITE_ERRNO("host_accept", 1);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  // Set nonblocking
  int flags = fcntl(ans, F_GETFL, 0);
  if (flags == -1 || fcntl(ans, F_SETFL, flags | O_NONBLOCK) != 0) {
    WRITE_ERRNO("host_accept", 1);
  }

  return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
}

DEFINE_BINDING(host_listen) {
  assert(nargs == 3);
  assert(nresults == 1);

  // Unpack port and backlog
  int32_t port = int32_t_of_wasmtime_val_t(args[0]);
  int32_t backlog = int32_t_of_wasmtime_val_t(args[1]);

  // Create the socket.
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    WRITE_ERRNO("host_listen", 2);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
  }

  // Bind the socket.
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  int ans = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
  if (ans < 0) {
    WRITE_ERRNO("host_listen", 2);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  // Start listening.
  ans = listen(sockfd, backlog);
  if (ans < 0) {
    WRITE_ERRNO("host_listen", 2);
    return result1(results, wasmtime_val_t_of_int64_t((int64_t)ans));
  }

  // Set nonblocking
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) != 0) {
    WRITE_ERRNO("host_listen", 2);
  }

  return result1(results, wasmtime_val_t_of_int64_t((int64_t)sockfd));
}

DEFINE_BINDING(host_recv) {
  assert(nargs == 4);
  assert(nresults == 1);

  // Unpack fd, buffer offset, and length
  int64_t sockfd = int64_t_of_wasmtime_val_t(args[0]);
  uint32_t boffset = uint32_t_of_wasmtime_val_t(args[1]);
  uint32_t blen = uint32_t_of_wasmtime_val_t(args[2]);

  // Load the buffer.
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_recv");

  // Perform the system call.
  int ans = recv((int)sockfd, mem+boffset, (size_t)blen, 0);

  if (ans < 0) {
    WRITE_ERRNO("host_recv", 3);
  }

  return result1(results, wasmtime_val_t_of_int32_t((int32_t)ans));
}

DEFINE_BINDING(host_send) {
  assert(nargs == 4);
  assert(nresults == 1);

  // Unpack fd, buffer offset, and length
  int64_t sockfd = int64_t_of_wasmtime_val_t(args[0]);
  uint32_t boffset = uint32_t_of_wasmtime_val_t(args[1]);
  uint32_t blen = uint32_t_of_wasmtime_val_t(args[2]);

  // Load the buffer.
  uint8_t *mem;
  LOAD_MEMORY(mem, "host_send");

  // Perform the system call.
  int ans = send((int)sockfd, mem+boffset, (size_t)blen, 0);

  if (ans < 0) {
    WRITE_ERRNO("host_send", 3);
  }

  return result1(results, wasmtime_val_t_of_int32_t((int32_t)ans));
}

DEFINE_BINDING(host_close) {
  assert(nargs == 2);
  assert(nresults == 1);

  // Unpack fd.
  int64_t sockfd = int64_t_of_wasmtime_val_t(args[0]);

  // Perform the system call.
  int ans = close((int)sockfd);

  if (ans < 0) {
    WRITE_ERRNO("host_close", 1);
  }

  return result1(results, wasmtime_val_t_of_int32_t((int32_t)ans));
}

wasmtime_error_t* host_socket_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module) {
  wasmtime_error_t *error = NULL;

  if (accept_sig == NULL) {
    // Accept
    accept_sig = wasm_functype_new_2_1(NEW_WASM_I64, NEW_WASM_I32, NEW_WASM_I64);
    wasmtime_func_new(context, accept_sig, host_accept, NULL, NULL, &acceptfn);
    acceptex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = acceptfn } };
    LINK_HOST_FN("accept", acceptex);
  }

  if (listen_sig == NULL) {
    // Listen
    listen_sig = wasm_functype_new_3_1(NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I64);
    wasmtime_func_new(context, listen_sig, host_listen, NULL, NULL, &listenfn);
    listenex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = listenfn } };
    LINK_HOST_FN("listen", listenex);
  }

  if (connect_sig == NULL) {
    // Connect
    connect_sig = wasm_functype_new_4_1(NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I64);
    wasmtime_func_new(context, connect_sig, host_connect, NULL, NULL, &connectfn);
    connectex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = connectfn } };
    LINK_HOST_FN("connect", connectex);
  }

  if (recv_sig == NULL) {
    // Send and recv
    recv_sig = wasm_functype_new_4_1(NEW_WASM_I64, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32);
    wasmtime_func_new(context, recv_sig, host_recv, NULL, NULL, &recvfn);
    recvex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = recvfn } };
    LINK_HOST_FN("recv", recvex);
  }

  if (send_sig == NULL) {
    send_sig = wasm_functype_new_4_1(NEW_WASM_I64, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32, NEW_WASM_I32);
    wasmtime_func_new(context, send_sig, host_send, NULL, NULL, &sendfn);
    sendex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = sendfn } };
    LINK_HOST_FN("send", sendex);
  }

  if (close_sig == NULL) {
    // Close
    close_sig = wasm_functype_new_2_1(NEW_WASM_I64, NEW_WASM_I32, NEW_WASM_I32);
    wasmtime_func_new(context, close_sig, host_close, NULL, NULL, &closefn);
    closeex = (wasmtime_extern_t){ .kind = WASMTIME_EXTERN_FUNC, .of = { .func = closefn } };
    LINK_HOST_FN("close", closeex);
  }
  return error;
}

void host_socket_delete(void) {
  wasm_functype_delete(listen_sig);
  listen_sig = NULL;

  wasm_functype_delete(accept_sig);
  accept_sig = NULL;

  wasm_functype_delete(connect_sig);
  connect_sig = NULL;

  wasm_functype_delete(recv_sig);
  recv_sig = NULL;

  wasm_functype_delete(send_sig);
  send_sig = NULL;

  wasm_functype_delete(close_sig);
  close_sig = NULL;
}


