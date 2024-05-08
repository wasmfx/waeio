#ifndef WAEIO_WASSERT_H
#define WAEIO_WASSERT_H

#if defined DEBUG
#include <assert.h>
#define wassert(x) assert((x))
#else
#define wassert(_) {}
#endif
#endif
