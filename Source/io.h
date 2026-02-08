/*
** iffar - Amiga DOS I/O and error output layer
** Replaces Unix-style open/read/write/lseek/close and stderr for portability.
** C89 compliant.
*/

#ifndef IFFAR_IO_H
#define IFFAR_IO_H

#include <exec/types.h>

#define IFFAR_FD_INVALID (-1)
#define IFFAR_SEEK_SET 0
#define IFFAR_SEEK_CUR 1
#define IFFAR_SEEK_END 2

int io_open_read(const char *path);
int io_open_append(const char *path);
int io_open_create(const char *path);
int io_open_write_new(const char *path);
void io_close(int fd);
long io_read(int fd, void *buf, long n);
long io_write(int fd, const void *buf, long n);
long io_seek(int fd, long pos, int whence);
int io_rename(const char *oldpath, const char *newpath);
int io_unlink(const char *path);
void io_put_id(ULONG id);
void io_err_fmt(const char *fmt, ...);
void io_perror(const char *path);

#endif
