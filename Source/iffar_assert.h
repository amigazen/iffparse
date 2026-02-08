/*
** iffar - assertion macro for debug builds
** C89 compliant. Include io.h before this header (for io_err_fmt).
*/

#ifndef IFFAR_ASSERT_H
#define IFFAR_ASSERT_H

#include <stdlib.h>

#ifndef NDEBUG
extern void cleanup(void);
#define IFFAR_ASSERT(x) \
    do { if (!(x)) { io_err_fmt("Assertion failed: %s, file %s, line %d\n", #x, __FILE__, __LINE__); cleanup(); exit(1); } } while (0)
#else
#define IFFAR_ASSERT(x) ((void)0)
#endif

#endif
