// Host socket bindings
#ifndef WAEIO_HOST_DRIVER_SOCKET_H
#define WAEIO_HOST_DRIVER_SOCKET_H

#include <wasmtime.h>

wasmtime_error_t* host_socket_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module);
void host_socket_delete(void);

#endif
