#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_utils.h>

extern
__wasm_import__("host", "put_hello")
void put_hello(uint8_t *buf, uint32_t buf_len);

int main(void) {
  const size_t buf_len = strlen("hello world");
  //uint8_t *buf = (uint8_t*)malloc(buf_len);
  uint8_t buf[buf_len];
  memset(buf, '\0', buf_len);
  put_hello(buf, buf_len);
  printf("%s\n", (char*)buf);
  //free(buf);
  return 0;
}
