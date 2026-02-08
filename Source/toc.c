/*
** iffar - IFF CAT archiver table of contents
** Based on public domain code by Karl Lehenbauer (1988).
** Ported to Amiga DOS I/O and C89.
*/

#include <exec/types.h>
#include <exec/memory.h>

#include "iff.h"
#include "io.h"
#include "iffar_assert.h"

#define ID_NAME       MakeID('N','A','M','E')
#define ID_AUTH       MakeID('A','U','T','H')
#define ID_ANNO       MakeID('A','N','N','O')
#define ID_Copyright  MakeID('(','c',')',' ')

extern int OpenCAT(char *archive_name, ULONG *subtype_ptr, LONG *length_ptr);
extern ULONG nextCATchunk(int fd, ULONG *subtype_ptr, char *fname_ptr,
                         LONG *chunk_length_ptr, LONG *metachunk_length_ptr);
extern ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left);
extern int readchunk(int fd, char *buf, LONG size, LONG *chunk_bytes_left);
extern int skipchunk(int fd, LONG chunksize, LONG *chunk_bytes_left);
extern void PutID(ID id);
extern void panic(const char *s);

extern int verbose;

int table_of_contents(char *fname)
{
    int fd;
    ULONG cat_type;
    ULONG chunkid;
    ULONG innerchunkid;
    ULONG subtype;
    long chunksize;
    long innerchunksize;
    long filesize;
    char namebuf[256];

    fd = OpenCAT(fname, &cat_type, &filesize);
    if (fd == IFFAR_FD_INVALID) {
        io_err_fmt("Can't open archive '%s'\n", fname);
        return 0;
    }

    if (verbose) {
        io_err_fmt("CAT subtype is ");
        PutID((ID)cat_type);
        io_err_fmt("\n");
    }

    while ((chunkid = nextCATchunk(fd, &subtype, &namebuf[0], &chunksize, &filesize)) != 0L) {
        if (chunkid != (ULONG)ID_FORM && chunkid != (ULONG)ID_CAT && chunkid != (ULONG)ID_LIST) {
            if (verbose) {
                PutID((ID)chunkid);
                io_err_fmt(" chunk is being skipped\n");
            }
            skipchunk(fd, chunksize, &filesize);
        } else {
            if (namebuf[0] == '\0') {
                io_err_fmt("CAT entry didn't contain a FNAM chunk - skipping\n");
                skipchunk(fd, chunksize, &filesize);
            } else {
                PutID((ID)chunkid);
                io_err_fmt(" ");
                PutID((ID)subtype);
                io_err_fmt(" %6ld  %s\n", (long)chunksize, namebuf);

                if (!verbose) {
                    skipchunk(fd, chunksize, &filesize);
                } else {
                    filesize -= chunksize;
                    if (chunksize & 1) {
                        filesize--;
                    }

                    while (chunksize != 0) {
                        innerchunkid = nextchunk(fd, &innerchunksize, &chunksize);
                        if (chunksize == 0) {
                            break;
                        }

                        io_err_fmt("   ");
                        PutID((ID)innerchunkid);

                        if (innerchunkid == (ULONG)ID_FNAM) {
                            panic("more than one FNAM chunk in an embedded FORM");
                        } else if (innerchunkid == (ULONG)ID_NAME ||
                                   innerchunkid == (ULONG)ID_AUTH ||
                                   innerchunkid == (ULONG)ID_ANNO ||
                                   innerchunkid == (ULONG)ID_Copyright) {
                            if (innerchunksize >= (long)sizeof(namebuf)) {
                                io_err_fmt("[too long]\n");
                                skipchunk(fd, innerchunksize, &chunksize);
                            } else {
                                if (!readchunk(fd, namebuf, innerchunksize, &chunksize)) {
                                    io_err_fmt("got into trouble reading chunk text\n");
                                    io_close(fd);
                                    return 0;
                                }
                                namebuf[innerchunksize] = '\0';
                                io_err_fmt(", %s\n", namebuf);
                            }
                        } else {
                            io_err_fmt(", size %ld\n", (long)innerchunksize);
                            skipchunk(fd, innerchunksize, &chunksize);
                        }
                    }
                }
            }
        }
    }

    io_close(fd);
    return 1;
}
