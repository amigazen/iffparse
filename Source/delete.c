/*
** iffar - IFF CAT archiver delete functions
** Based on public domain code by Karl Lehenbauer (1988).
** Ported to Amiga DOS I/O and C89.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>

#include "iff.h"
#include "io.h"

extern ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left);
extern int skipchunk(int fd, LONG chunksize, LONG *chunk_bytes_left);
extern int create_archive(char *archive_name, ULONG subtype);
extern int WriteChunkHeader(int fd, ULONG chunktype, long length);
extern int WriteCATentry(int fd, char *fname, ULONG chunktype, ULONG subtype, long length);
extern int rewrite_archive_header(int fd);
extern void copychunkbytes(int infd, int outfd, long nbytes, long *filebytes_left_ptr);
extern int OpenCAT(char *archive_name, ULONG *subtype_ptr, LONG *length_ptr);
extern ULONG nextCATchunk(int fd, ULONG *subtype_ptr, char *fname_ptr,
                         LONG *chunk_length_ptr, LONG *metachunk_length_ptr);
extern char *basename(char *fname);
extern int iffar_strnicmp(char *s1, char *s2, int len);

extern int verbose;

int delete_entries(char *archive_name, char *fnames[], int nfiles)
{
    int old_archive_fd;
    int new_archive_fd;
    ULONG chunkid;
    ULONG subtype;
    long chunksize;
    long filesize;
    char textbuf[128];
    char old_archive_name[128];
    int i;
    int delete_file;

    sprintf(old_archive_name, "%s.old", archive_name);
    io_unlink(old_archive_name);
    io_rename(archive_name, old_archive_name);

    old_archive_fd = OpenCAT(old_archive_name, &subtype, &filesize);
    if (old_archive_fd == IFFAR_FD_INVALID) {
        io_err_fmt("Can't open archive '%s'\n", old_archive_name);
        return 0;
    }

    new_archive_fd = create_archive(archive_name, (ULONG)ID_MISC);
    if (new_archive_fd == IFFAR_FD_INVALID) {
        io_close(old_archive_fd);
        return 0;
    }

    while ((chunkid = nextCATchunk(old_archive_fd, &subtype, &textbuf[0], &chunksize, &filesize)) != 0L) {
        if (chunkid != (ULONG)ID_FORM && chunkid != (ULONG)ID_CAT && chunkid != (ULONG)ID_LIST) {
            if (!WriteChunkHeader(new_archive_fd, chunkid, chunksize)) {
                io_close(old_archive_fd);
                io_close(new_archive_fd);
                return 0;
            }
            copychunkbytes(old_archive_fd, new_archive_fd, chunksize, &filesize);
        } else {
            delete_file = 0;
            for (i = 0; i < nfiles; i++) {
                if (iffar_strnicmp(basename(fnames[i]), textbuf, 128) == 0) {
                    delete_file = 1;
                    break;
                }
            }
            if (delete_file) {
                if (verbose) {
                    io_err_fmt("deleting %s\n", textbuf);
                }
                if (!skipchunk(old_archive_fd, chunksize, &filesize)) {
                    io_err_fmt("delete: skipchunk failed\n");
                    io_close(old_archive_fd);
                    io_close(new_archive_fd);
                    return 0;
                }
            } else {
                if (!WriteCATentry(new_archive_fd, textbuf, chunkid, subtype, chunksize)) {
                    io_close(old_archive_fd);
                    io_close(new_archive_fd);
                    return 0;
                }
                copychunkbytes(old_archive_fd, new_archive_fd, chunksize, &filesize);
            }
        }
    }

    rewrite_archive_header(new_archive_fd);
    io_close(old_archive_fd);
    io_close(new_archive_fd);
    return 1;
}
