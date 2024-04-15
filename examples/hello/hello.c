#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_utils.h>

extern
__wasm_import__("host", "put_hello")
void put_hello(uint8_t *buf, uint32_t buf_len);

static char global[12] = "xxxxx yyyyy";

int main(void) {
  const size_t buf_len = strlen("hello world");

  // Stack allocation.
  uint8_t stk[buf_len]; // stack allocated.
  memset(stk, '\0', buf_len);
  put_hello(stk, buf_len);
  printf(" stack: %s\n", (char*)stk);

  // Heap allocation.
  uint8_t *heap = (uint8_t*)malloc(sizeof(uint8_t)*buf_len);
  put_hello(heap, buf_len);
  printf("  heap: %s\n", (char*)heap);
  free(heap);

  // Global/static allocation.
  put_hello((uint8_t*)global, buf_len);
  printf("static: %s\n", (char*)global);
  return 0;
}
