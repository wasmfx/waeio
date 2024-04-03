#include <stdio.h>
#include <stdlib.h>

#include <waeio.h>

void* my_main(void *arg) {
  (void)arg;
  return NULL;
}

int main(void) {
  waeio_main(my_main, NULL);
  return 0;
}
