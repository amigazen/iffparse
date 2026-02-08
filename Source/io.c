/*
** iffar - Amiga DOS I/O layer
** Provides open/read/write/seek/close and error output using Amiga DOS.
** Based on public domain iffar by Karl Lehenbauer; this layer is new.
** C89 compliant.
*/

#include "io.h"
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdarg.h>
#include <string.h>

static int bptr_to_fd(BPTR fh)
{
    if (fh == (BPTR)0) {
        return IFFAR_FD_INVALID;
    }
    return (int)(LONG)fh;
}

static BPTR fd_to_bptr(int fd)
{
    if (fd == IFFAR_FD_INVALID) {
        return (BPTR)0;
    }
    return (BPTR)(LONG)fd;
}

int io_open_read(const char *path)
{
    BPTR fh;

    fh = Open((STRPTR)path, MODE_OLDFILE);
    return bptr_to_fd(fh);
}

int io_open_append(const char *path)
{
    BPTR fh;

    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh == (BPTR)0) {
        return IFFAR_FD_INVALID;
    }
    if (Seek(fh, 0, OFFSET_END) == -1) {
        Close(fh);
        return IFFAR_FD_INVALID;
    }
    return bptr_to_fd(fh);
}

int io_open_create(const char *path)
{
    BPTR fh;

    fh = Open((STRPTR)path, MODE_NEWFILE);
    return bptr_to_fd(fh);
}

int io_open_write_new(const char *path)
{
    BPTR fh;

    DeleteFile((STRPTR)path);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    return bptr_to_fd(fh);
}

void io_close(int fd)
{
    BPTR fh;

    if (fd == IFFAR_FD_INVALID) {
        return;
    }
    fh = fd_to_bptr(fd);
    Close(fh);
}

long io_read(int fd, void *buf, long n)
{
    BPTR fh;
    long got;

    if (fd == IFFAR_FD_INVALID || n <= 0) {
        return -1;
    }
    fh = fd_to_bptr(fd);
    got = Read(fh, buf, n);
    if (got == -1) {
        return -1;
    }
    return got;
}

long io_write(int fd, const void *buf, long n)
{
    BPTR fh;
    long wrote;

    if (fd == IFFAR_FD_INVALID || n <= 0) {
        return -1;
    }
    fh = fd_to_bptr(fd);
    wrote = Write(fh, (APTR)buf, n);
    if (wrote == -1) {
        return -1;
    }
    return wrote;
}

long io_seek(int fd, long pos, int whence)
{
    BPTR fh;
    long amiga_whence;

    if (fd == IFFAR_FD_INVALID) {
        return -1;
    }
    fh = fd_to_bptr(fd);
    if (whence == IFFAR_SEEK_END) {
        amiga_whence = OFFSET_END;
    } else if (whence == IFFAR_SEEK_CUR) {
        amiga_whence = OFFSET_CURRENT;
    } else {
        amiga_whence = OFFSET_BEGINNING;
    }
    return Seek(fh, pos, amiga_whence);
}

int io_rename(const char *oldpath, const char *newpath)
{
    if (Rename((STRPTR)oldpath, (STRPTR)newpath) == DOSTRUE) {
        return 0;
    }
    return -1;
}

int io_unlink(const char *path)
{
    if (DeleteFile((STRPTR)path) == DOSTRUE) {
        return 0;
    }
    return -1;
}

void io_put_id(ULONG id)
{
    BPTR out;
    UBYTE c[4];

    out = ErrorOutput();
    if (out == (BPTR)0) {
        out = Output();
    }
    c[0] = (UBYTE)((id >> 24) & 0xFF);
    c[1] = (UBYTE)((id >> 16) & 0xFF);
    c[2] = (UBYTE)((id >> 8) & 0xFF);
    c[3] = (UBYTE)(id & 0xFF);
    FPutC(out, (LONG)c[0]);
    FPutC(out, (LONG)c[1]);
    FPutC(out, (LONG)c[2]);
    FPutC(out, (LONG)c[3]);
}

void io_err_fmt(const char *fmt, ...)
{
    BPTR out;
    va_list args;

    out = ErrorOutput();
    if (out == (BPTR)0) {
        out = Output();
    }
    va_start(args, fmt);
    VFPrintf(out, (STRPTR)fmt, args);
    va_end(args);
}

void io_perror(const char *path)
{
    LONG err;
    char buf[256];

    err = IoErr();
    Fault(err, (STRPTR)path, (STRPTR)buf, sizeof(buf));
    io_err_fmt("%s\n", buf);
}
