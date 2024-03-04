# WebAssembly Effect-based I/O

Waeio provides a prototype effects-based direct-style I/O model for
Wasm. The main purpose of this model is to illustrate the use of stack
switching to interleave multiple I/O operations at the same
time. Waeio supports two stack switching backends, Asyncify and
WasmFX, and two I/O backends, host-defined I/O and WASI.
