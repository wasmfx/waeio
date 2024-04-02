MODE=debug
ASYNCIFY=../benchfx/binaryenfx/bin/wasm-opt --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations -O2 --asyncify --pass-arg=asyncify-ignore-imports
WASICC=../benchfx/wasi-sdk-21.0/bin/clang-17
COMMON_FLAGS=--std=c17 -Wall -Wextra -Werror -Wpedantic -Wno-strict-prototypes -O3 -I inc -DMAX_CONNECTIONS=16384
ifeq ($(MODE), debug)
COMMON_FLAGS:=$(COMMON_FLAGS) -g
endif
WASIFLAGS=$(COMMON_FLAGS)
CC=clang
CFLAGS=$(COMMON_FLAGS) -I ../wasmtime/crates/c-api/include -I ../wasmtime/crates/c-api/wasm-c-api/include ../wasmtime/target/$(MODE)/libwasmtime.a -lpthread -ldl -lm

.PHONY: echoserver_wasi
echoserver_wasi: examples/echoserver/echoserver.c
	$(WASICC) src/freelist.c src/fiber_asyncify.c src/wasio_wasi.c src/waeio.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_wasi.wasm
	$(ASYNCIFY) echoserver_wasi.wasm -o echoserver_wasi_asyncify.wasm
	chmod +x echoserver_wasi_asyncify.wasm

.PHONY: echoserver_host
echoserver_host: examples/echoserver/echoserver.c
	$(WASICC) src/fiber_asyncify.c src/wasio_host.c src/waeio.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_host.wasm
	$(ASYNCIFY) echoserver_host.wasm -o echoserver_host_asyncify.wasm
	chmod +x echoserver_host_asyncify.wasm

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


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.wasm
	rm -f freelist_tests
