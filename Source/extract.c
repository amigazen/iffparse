/*
** iffar - IFF CAT archiver extractor functions
** Based on public domain code by Karl Lehenbauer (1988).
** Ported to Amiga DOS I/O and C89.
*/

#include <exec/types.h>
#include <exec/memory.h>

#include "iff.h"
#include "io.h"

extern int OpenCAT(char *archive_name, ULONG *subtype_ptr, LONG *length_ptr);
extern ULONG nextCATchunk(int fd, ULONG *subtype_ptr, char *fname_ptr,
                         LONG *chunk_length_ptr, LONG *metachunk_length_ptr);
extern ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left);
extern int skipchunk(int fd, LONG chunksize, LONG *chunk_bytes_left);
extern int WriteSuperChunkHeader(int fd, ULONG chunktype, ULONG subtype, long length);
extern void copychunkbytes(int infd, int outfd, long nbytes, long *filebytes_left_ptr);
extern char *basename(char *fname);
extern int iffar_strnicmp(char *s1, char *s2, int len);

extern int verbose;

int extract(char *archive_name, char *fnames[], int nfiles)
{
    int archive_fd;
    int outfd;
    ULONG chunkid;
    ULONG subtype;
    long chunksize;
    long filesize;
    char textbuf[256];
    char *extract_name;
    int do_extract;
    int i;

    extract_name = textbuf;
    i = 0;

    archive_fd = OpenCAT(archive_name, &subtype, &filesize);
    if (archive_fd == IFFAR_FD_INVALID) {
        io_err_fmt("Can't open archive '%s'\n", archive_name);
        return 0;
    }

    while ((chunkid = nextCATchunk(archive_fd, &subtype, &textbuf[0], &chunksize, &filesize)) != 0L) {
        if (chunkid != (ULONG)ID_FORM && chunkid != (ULONG)ID_CAT && chunkid != (ULONG)ID_LIST) {
            if (!skipchunk(archive_fd, chunksize, &filesize)) {
                io_perror(archive_name);
                io_err_fmt("extract: skipchunk failed\n");
                io_close(archive_fd);
                return 0;
            }
            break;
        }

        do_extract = 0;
        if (nfiles == 0) {
            do_extract = 1;
            extract_name = textbuf;
        } else {
            for (i = 0; i < nfiles; i++) {
                if (iffar_strnicmp(basename(fnames[i]), textbuf, 128) == 0) {
                    do_extract = 1;
                    extract_name = fnames[i];
                    break;
                }
            }
        }

        if (do_extract) {
            if (verbose) {
                io_err_fmt("extracting %s\n", extract_name);
            }

            outfd = io_open_write_new(extract_name);
            if (outfd == IFFAR_FD_INVALID) {
                io_perror(textbuf);
            } else {
                if (!WriteSuperChunkHeader(outfd, chunkid, subtype, chunksize)) {
                    io_close(outfd);
                    io_close(archive_fd);
                    return 0;
                }
                copychunkbytes(archive_fd, outfd, chunksize, &filesize);
                io_close(outfd);
            }

            if (nfiles != 0) {
                fnames[i][0] = '\0';
            }
        } else {
            if (!skipchunk(archive_fd, chunksize, &filesize)) {
                io_perror(archive_name);
                io_err_fmt("extract: skipchunk failed\n");
                io_close(archive_fd);
                return 0;
            }
        }
    }

    for (i = 0; i < nfiles; i++) {
        if (fnames[i][0] != '\0') {
            io_err_fmt("%s: no such archive entry\n", fnames[i]);
        }
    }

    io_close(archive_fd);
    return 1;
}
