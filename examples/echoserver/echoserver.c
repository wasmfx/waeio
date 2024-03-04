#include <stdio.h>
#include <stdlib.h>

#include <waeio.h>

#define TMP_FILE "/dev/stdin"

int main(void) {
  FILE *fptr = fopen(TMP_FILE, "r");
  char buf[6];
  fgets(buf, 6, fptr);
  printf("%s\n", buf);
  waeio_close(0);
  return 0;
}
