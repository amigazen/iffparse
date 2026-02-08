/*
** iffar - cleanup management (based on public domain code by Peter da Silva, Karl Lehenbauer 1988)
** Ported to Amiga DOS and C89.
*/

#include <exec/memory.h>
#include <stdlib.h>

#include "io.h"

struct _clean {
    void (*function)(void);
    struct _clean *next;
};

static struct _clean *cleanlist = NULL;

void add_cleanup(void (*function)(void))
{
    struct _clean *ptr;

    ptr = (struct _clean *)AllocMem(sizeof(struct _clean), MEMF_PUBLIC);
    if (!ptr) {
        return;
    }
    ptr->function = function;
    ptr->next = cleanlist;
    cleanlist = ptr;
}

void cleanup(void)
{
    struct _clean *ptr;
    void (*f)(void);

    while (cleanlist) {
        ptr = cleanlist;
        cleanlist = cleanlist->next;
        f = ptr->function;
        FreeMem(ptr, sizeof(struct _clean));
        (*f)();
    }
}

static short panic_in_progress = 0;

void panic(const char *s)
{
    io_err_fmt("panic: %s\n", s);
    if (!panic_in_progress) {
        panic_in_progress = 1;
        cleanup();
        exit(10);
    }
    io_err_fmt("double panic!\n");
    exit(11);
}
