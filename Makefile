ASYNCIFY=../benchfx/binaryenfx/bin/wasm-opt --enable-exception-handling --enable-reference-types --enable-multivalue --enable-bulk-memory --enable-gc --enable-typed-continuations -O2 --asyncify --pass-arg=asyncify-ignore-imports
WASICC=../benchfx/wasi-sdk-21.0/bin/clang-17
WASIFLAGS=--std=c17 -Wall -I inc -O3
CC=clang-17
CFLAGS=--std=c17 -Wall -I inc -I ../wasmtime/crates/c-api/include -I ../wasmtime/crates/c-api/wasm-c-api/include ../wasmtime/target/release/libwasmtime.a -lpthread -ldl -lm -O3

echoserver_wasi: examples/echoserver/echoserver.c
	$(WASICC) src/fiber_asyncify.c src/wasio_wasi.c src/waeio.c $(WASIFLAGS) examples/echoserver/echoserver.c -o echoserver_wasi.wasm
	$(ASYNCIFY) echoserver_wasi.wasm -o echoserver_wasi_asyncify.wasm
	chmod +x echoserver_wasi_asyncify.wasm

.PHONY: clean
clean:
	rm -f *.o
	rm -f echoserver_*.wasm
