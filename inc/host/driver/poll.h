// Host poll bindings
#ifndef WAEIO_HOST_DRIVER_POLL_H
#define WAEIO_HOST_DRIVER_POLL_H

#include <wasmtime.h>

wasmtime_error_t* host_poll_init(wasmtime_linker_t *linker, wasmtime_context_t *context, const char *export_module);
void host_poll_delete(void);

#endif
