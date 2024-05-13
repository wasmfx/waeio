MAX_CONNECTIONS=1024
ASYNCIFY_DEFAULT_STACK_SIZE?=2097152
WASMFX_PRESERVE_SHADOW_STACK?=1
# Only relevant if WASMFX_PRESERVE_SHADOW_STACK is 1
WASMFX_CONT_SHADOW_STACK_SIZE?=65536
MODE?=release
VERBOSE?=0
ASYNCIFY=../benchfx/binaryenfx/bin/wasm-opt --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations -O2 --asyncify
WASICC=../benchfx/wasi-sdk-22.0/bin/clang
WASM_INTERP=../spec/interpreter/wasm
WASM_MERGE=../benchfx/binaryenfx/bin/wasm-merge --enable-multimemory --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations
COMMON_FLAGS=--std=c17 -Wall -Wextra -Werror -Wpedantic -Wno-strict-prototypes -O3 -I inc -I vendor/fiber-c/inc -DMAX_CONNECTIONS=$(MAX_CONNECTIONS)
ifeq ($(MODE), debug)
COMMON_FLAGS:=$(COMMON_FLAGS) -DDEBUG -g -ftrapv -fno-split-stack -fsanitize-trap -fstack-protector
ifeq ($(VERBOSE), 1)
COMMON_FLAGS:=$(COMMON_FLAGS) -DVERBOSE
endif
endif
WASIFLAGS=$(COMMON_FLAGS) --sysroot=../benchfx/wasi-sdk-22.0/share/wasi-sysroot
CC=clang
CFLAGS=$(COMMON_FLAGS) -I ../wasmtime/crates/c-api/include -I ../wasmtime/crates/c-api/wasm-c-api/include ../wasmtime/target/$(MODE)/libwasmtime.a -lpthread -ldl -lm -fuse-ld=mold
ifeq ($(WASMFX_PRESERVE_SHADOW_STACK),1)
  SHADOW_STACK_FLAG=-DFIBER_WASMFX_PRESERVE_SHADOW_STACK
else
  SHADOW_STACK_FLAG=
endif


.PHONY: echoserver_wasi
echoserver_wasi: examples/echoserver/echoserver.c
	$(WASICC) -DWASIO_BACKEND=1 -DASYNCIFY_DEFAULT_STACK_SIZE=$(ASYNCIFY_DEFAULT_STACK_SIZE) src/freelist.c vendor/fiber-c/src/asyncify/asyncify_impl.c src/wasio_wasi.c src/waeio.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_wasi.wasm
	$(ASYNCIFY) echoserver_wasi.wasm -o echoserver_wasi_asyncify.wasm
	chmod +x echoserver_wasi_asyncify.wasm

.PHONY: echoserver_host
echoserver_host: inc/host/errno.h src/host/errno.c examples/echoserver/echoserver.c
	$(WASICC) -DWASIO_BACKEND=2 src/freelist.c src/host/errno.c src/wasio_host.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_host.wasm
	$(ASYNCIFY) echoserver_host.wasm -o echoserver_host_asyncify.wasm
	$(CC) src/host/socket.c src/host/poll.c examples/echoserver/driver.c -o echoserver_driver $(CFLAGS)
	chmod +x echoserver_host_asyncify.wasm

httpserver_host_asyncify.wasm:  inc/host/errno.h src/host/errno.c inc/host/poll.h examples/httpserver/http_utils.h examples/httpserver/httpserver_fiber.c
	$(WASICC) -DASYNCIFY_DEFAULT_STACK_SIZE=$(ASYNCIFY_DEFAULT_STACK_SIZE) vendor/picohttpparser/picohttpparser.c src/host/errno.c vendor/fiber-c/src/asyncify/asyncify_impl.c $(WASIFLAGS) -I examples/httpserver examples/httpserver/httpserver_fiber.c -o httpserver_host_asyncfiy.pre.wasm -I vendor/picohttpparser
	$(ASYNCIFY) httpserver_host_asyncfiy.pre.wasm -o httpserver_host_asyncify.wasm
	chmod +x httpserver_host_asyncify.wasm

httpserver_host_wasmfx.wasm: inc/host/errno.h src/host/errno.c inc/host/poll.h examples/httpserver/httpserver_fiber.c examples/httpserver/http_utils.h src/fiber_wasmfx_imports.wat
	$(WASICC) $(SHADOW_STACK_FLAG) -DWASMFX_CONT_SHADOW_STACK_SIZE=$(WASMFX_CONT_SHADOW_STACK_SIZE) -DWASMFX_CONT_TABLE_INITIAL_CAPACITY=$(MAX_CONNECTIONS) -Wl,--export-table,--export-memory vendor/picohttpparser/picohttpparser.c src/host/errno.c vendor/fiber-c/src/wasmfx/wasmfx_impl.c $(WASIFLAGS) -I examples/httpserver examples/httpserver/httpserver_fiber.c -o httpserver_host_wasmfx.pre.wasm -I vendor/picohttpparser

	# Export stack pointer global as __exported_shadow_stack_pointer
	$(WASM_INTERP) -d -i httpserver_host_wasmfx.pre.wasm -o httpserver_host_wasmfx.pre.wat
	vendor/fiber-c/src/wasmfx/export_shadow_stack_ptr.sh httpserver_host_wasmfx.pre.wat httpserver_host_wasmfx.patched_pre.wat
	$(WASM_INTERP) -d -i httpserver_host_wasmfx.patched_pre.wat -o httpserver_host_wasmfx.patched_pre.wasm

	$(WASM_INTERP) -d -i src/fiber_wasmfx_imports.wat -o fiber_wasmfx_imports.wasm
	$(WASM_MERGE) fiber_wasmfx_imports.wasm "fiber_wasmfx_imports" httpserver_host_wasmfx.patched_pre.wasm "main"  -o httpserver_host_wasmfx.merged.wasm

	# HOTFIX: We need to delete the stack pointer export for now (wasmtime doesn't like it)
	$(WASM_INTERP) -d -i httpserver_host_wasmfx.merged.wasm -o httpserver_host_wasmfx.merged.wat
	sed  's/.*(export "__exported_shadow_stack_pointer".*//' httpserver_host_wasmfx.merged.wat > httpserver_host_wasmfx.wat
	$(WASM_INTERP) -d -i httpserver_host_wasmfx.wat -o httpserver_host_wasmfx.wasm

	chmod +x httpserver_host_wasmfx.wasm

httpserver_wasio_host: inc/wasio.h httpserver_wasio_host_wasmfx.wasm httpserver_wasio_host_asyncify.wasm

httpserver_wasio_host_asyncify.wasm: inc/wasio.h inc/host/errno.h src/wasio/host_poll.c examples/httpserver/http_utils.h examples/httpserver/httpserver_wasio_fiber.c
	$(WASICC) -DASYNCIFY_DEFAULT_STACK_SIZE=$(ASYNCIFY_DEFAULT_STACK_SIZE) -DWASIO_BACKEND=2 vendor/picohttpparser/picohttpparser.c src/host/errno.c src/wasio/host_poll.c vendor/fiber-c/src/asyncify/asyncify_impl.c $(WASIFLAGS) -I examples/httpserver examples/httpserver/httpserver_wasio_fiber.c -o httpserver_wasio_host_asyncfiy.pre.wasm -I vendor/picohttpparser
	$(ASYNCIFY) httpserver_wasio_host_asyncfiy.pre.wasm -o httpserver_wasio_host_asyncify.wasm
	chmod +x httpserver_wasio_host_asyncify.wasm

httpserver_wasio_host_wasmfx.wasm: inc/host/errno.h src/host/errno.c inc/host/poll.h src/wasio/host_poll.c examples/httpserver/httpserver_wasio_fiber.c examples/httpserver/http_utils.h src/fiber_wasmfx_imports.wat
	$(WASICC) $(SHADOW_STACK_FLAG) -DWASMFX_CONT_SHADOW_STACK_SIZE=$(WASMFX_CONT_SHADOW_STACK_SIZE) -DWASIO_BACKEND=2 -DWASMFX_CONT_TABLE_INITIAL_CAPACITY=$(MAX_CONNECTIONS) -Wl,--export-table,--export-memory vendor/picohttpparser/picohttpparser.c src/host/errno.c vendor/fiber-c/src/wasmfx/wasmfx_impl.c src/wasio/host_poll.c $(WASIFLAGS) -I examples/httpserver examples/httpserver/httpserver_wasio_fiber.c -o httpserver_wasio_host_wasmfx.pre.wasm -I vendor/picohttpparser

	# Export stack pointer global as __exported_shadow_stack_pointer
	$(WASM_INTERP) -d -i httpserver_wasio_host_wasmfx.pre.wasm -o httpserver_wasio_host_wasmfx.pre.wat
	vendor/fiber-c/src/wasmfx/export_shadow_stack_ptr.sh httpserver_wasio_host_wasmfx.pre.wat httpserver_wasio_host_wasmfx.patched_pre.wat
	$(WASM_INTERP) -d -i httpserver_wasio_host_wasmfx.patched_pre.wat -o httpserver_wasio_host_wasmfx.patched_pre.wasm

	$(WASM_INTERP) -d -i src/fiber_wasmfx_imports.wat -o fiber_wasmfx_imports.wasm
	$(WASM_MERGE) fiber_wasmfx_imports.wasm "fiber_wasmfx_imports" httpserver_wasio_host_wasmfx.patched_pre.wasm "benchmark" -o httpserver_wasio_host_wasmfx.wasm
	chmod +x httpserver_wasio_host_wasmfx.wasm

httpserver_host_bespoke.wasm: inc/host/errno.h src/host/errno.c inc/host/poll.h examples/httpserver/httpserver_bespoke.c examples/httpserver/http_utils.h
	$(WASICC) src/host/errno.c vendor/picohttpparser/picohttpparser.c $(WASIFLAGS) -I vendor/picohttpparser -I examples/httpserver examples/httpserver/httpserver_bespoke.c -o httpserver_host_bespoke.wasm

src/fiber_wasmfx_imports.wat: vendor/fiber-c/src/wasmfx/imports.wat.pp
	$(CC) -xc $(SHADOW_STACK_FLAG) -DWASMFX_CONT_TABLE_INITIAL_CAPACITY=$(MAX_CONNECTIONS) -E vendor/fiber-c/src/wasmfx/imports.wat.pp | sed 's/^#.*//g' > src/fiber_wasmfx_imports.wat

.PHONY: httpserver_host
httpserver_host: inc/host/errno.h src/host/errno.c examples/httpserver/driver.c httpserver_host_asyncify.wasm httpserver_host_wasmfx.wasm httpserver_host_bespoke.wasm httpserver_wasio_host
	$(CC) src/host/driver/socket.c src/host/driver/poll.c examples/httpserver/driver.c -o httpserver_driver $(CFLAGS)

.PHONY: hello
hello: examples/hello/hello.c examples/hello/driver.c
	$(WASICC) $(WASIFLAGS) examples/hello/hello.c -o hello.wasm
	$(CC) examples/hello/driver.c -o hello_driver $(CFLAGS)

.PHONY: freelist
freelist: src/freelist.c
	$(WASICC) $(WASIFLAGS) src/freelist.c -o freelist.wasm

.PHONY: test-freelist
test-freelist: test/freelist_tests.c
	$(CC) $(COMMON_FLAGS) src/freelist.c test/freelist_tests.c -o freelist_tests

hostgen: utils/hostgen.c
	$(CC) $(COMMON_FLAGS) utils/hostgen.c -o hostgen

inc/host/errno.h: hostgen
	./hostgen "errno.h" > inc/host/errno.h

src/host/errno.c: hostgen
	./hostgen "errno.c" > src/host/errno.c

inc/host/poll.h: hostgen
	./hostgen "poll.h" > inc/host/poll.h


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.wasm
	rm -f *.wat
	rm -f hostgen
	rm -f freelist_tests
	rm -f hello_driver echoserver_driver httpserver_driver
	rm -f src/host/errno.c inc/host/errno.h inc/host/poll.h
	rm -f src/fiber_wasmfx_imports.wat
