MODE=debug
ASYNCIFY=../benchfx/binaryenfx/bin/wasm-opt --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations -O2 --asyncify
WASICC=../benchfx/wasi-sdk-22.0/bin/clang
WASM_INTERP=../spec/interpreter/wasm
WASM_MERGE=../benchfx/binaryenfx/bin/wasm-merge --enable-multimemory --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations
COMMON_FLAGS=--std=c17 -Wall -Wextra -Werror -Wpedantic -Wno-strict-prototypes -O3 -I inc -DMAX_CONNECTIONS=1024
ifeq ($(MODE), debug)
COMMON_FLAGS:=$(COMMON_FLAGS) -g
endif
WASIFLAGS=$(COMMON_FLAGS)
CC=clang
CFLAGS=$(COMMON_FLAGS) -I ../wasmtime/crates/c-api/include -I ../wasmtime/crates/c-api/wasm-c-api/include ../wasmtime/target/$(MODE)/libwasmtime.a -lpthread -ldl -lm -fuse-ld=mold

.PHONY: echoserver_wasi
echoserver_wasi: examples/echoserver/echoserver.c
	$(WASICC) -DWASIO_BACKEND=1 src/freelist.c src/fiber_asyncify.c src/wasio_wasi.c src/waeio.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_wasi.wasm
	$(ASYNCIFY) echoserver_wasi.wasm -o echoserver_wasi_asyncify.wasm
	chmod +x echoserver_wasi_asyncify.wasm

.PHONY: echoserver_host
echoserver_host: inc/host/errno.h src/host/errno.c examples/echoserver/echoserver.c
	$(WASICC) -DWASIO_BACKEND=2 src/freelist.c src/host/errno.c src/wasio_host.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_host.wasm
	$(ASYNCIFY) echoserver_host.wasm -o echoserver_host_asyncify.wasm
	$(CC) src/host/socket.c src/host/poll.c examples/echoserver/driver.c -o echoserver_driver $(CFLAGS)
	chmod +x echoserver_host_asyncify.wasm

httpserver_host_asyncify.wasm:  inc/host/errno.h src/host/errno.c examples/httpserver/httpserver.c src/**/*.c inc/**/*.h
	$(WASICC) -DWASIO_BACKEND=2 vendor/picohttpparser/picohttpparser.c src/freelist.c src/host/errno.c src/fiber_asyncify.c src/wasio_host.c src/waeio.c $(WASIFLAGS) examples/httpserver/httpserver.c -o httpserver_host_asyncfiy.pre.wasm -I vendor/picohttpparser
	$(ASYNCIFY) httpserver_host_asyncfiy.pre.wasm -o httpserver_host_asyncify.wasm
	chmod +x httpserver_host_asyncify.wasm

httpserver_host_wasmfx.wasm: inc/host/errno.h src/host/errno.c examples/httpserver/httpserver.c src/**/*.c inc/**/*.h
	$(WASICC) -Wl,--export-table,--export-memory -DWASIO_BACKEND=2 vendor/picohttpparser/picohttpparser.c src/freelist.c src/host/errno.c src/fiber_wasmfx.c src/wasio_host.c src/waeio.c $(WASIFLAGS) examples/httpserver/httpserver.c -o httpserver_host_wasmfx.pre.wasm -I vendor/picohttpparser
	$(WASM_INTERP) -d -i src/fiber_wasmfx_imports.wat -o fiber_wasmfx_imports.wasm
	$(WASM_MERGE) fiber_wasmfx_imports.wasm "fiber_wasmfx_imports" httpserver_host_wasmfx.pre.wasm "benchmark" -o httpserver_host_wasmfx.wasm
	chmod +x httpserver_host_wasmfx.wasm

.PHONY: httpserver_host
httpserver_host: inc/host/errno.h src/host/errno.c examples/httpserver/httpserver.c httpserver_host_asyncify.wasm httpserver_host_wasmfx.wasm
	$(CC) src/host/socket.c src/host/poll.c examples/httpserver/driver.c -o httpserver_driver $(CFLAGS)

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

inc/host/errno.h: hosterrno.py
	python3 hosterrno.py h

src/host/errno.c: hosterrno.py
	python3 hosterrno.py c


.PHONY: clean
clean:
	rm -f *.o
	rm -f *.wasm
	rm -f freelist_tests
	rm -f hello_driver echoserver_driver httpserver_driver
	rm -f src/host/errno.c inc/host/errno.h
