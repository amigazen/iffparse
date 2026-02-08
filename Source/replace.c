/*
** iffar - IFF CAT archiver replace functions
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
extern int append_file_to_archive(char *fname, int archive_fd);
extern int OpenCAT(char *archive_name, ULONG *subtype_ptr, LONG *length_ptr);
extern ULONG nextCATchunk(int fd, ULONG *subtype_ptr, char *fname_ptr,
                         LONG *chunk_length_ptr, LONG *metachunk_length_ptr);
extern char *basename(char *fname);
extern int iffar_strnicmp(char *s1, char *s2, int len);

extern int verbose;
extern int insert_before;
extern int insert_after;
extern char *location_modifier_name;

int replace_entries(char *archive_name, char *fnames[], int nfiles)
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
    int replace_file;
    int entryindex;
    int insert_next_time;
    int modifier_matches;
    int did_insert;

    modifier_matches = 0;
    insert_next_time = 0;
    did_insert = 0;

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
            break;
        }

        if (textbuf[0] == '\0') {
            io_err_fmt("FORM, CAT or LIST in archive doesn't have an FNAM chunk, abandoning\n");
            io_close(old_archive_fd);
            io_close(new_archive_fd);
            return 0;
        }

        if (insert_before || insert_after) {
            modifier_matches = (iffar_strnicmp(location_modifier_name, textbuf, 128) == 0);
        }

        if ((modifier_matches && insert_before) || insert_next_time) {
            insert_next_time = 0;
            for (entryindex = 0; entryindex < nfiles; entryindex++) {
                if (verbose) {
                    if (insert_before) {
                        io_err_fmt("inserting %s\n", fnames[entryindex]);
                    } else {
                        io_err_fmt("appending %s\n", fnames[entryindex]);
                    }
                }
                append_file_to_archive(fnames[entryindex], new_archive_fd);
            }
            did_insert = 1;
        }

        if (modifier_matches && insert_after) {
            insert_next_time = 1;
        }

        replace_file = 0;
        for (i = 0; i < nfiles; i++) {
            if (iffar_strnicmp(basename(fnames[i]), textbuf, 128) == 0) {
                replace_file = 1;
                break;
            }
        }

        if (replace_file) {
            if (!insert_before && !insert_after) {
                if (verbose) {
                    io_err_fmt("replacing %s\n", textbuf);
                }
                append_file_to_archive(fnames[i], new_archive_fd);
                fnames[i][0] = '\0';
            } else if (verbose) {
                io_err_fmt("removing old %s\n", textbuf);
            }
            if (!skipchunk(old_archive_fd, chunksize, &filesize)) {
                io_err_fmt("replace: skipchunk failed\n");
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

    if ((!insert_before && !insert_after) || !did_insert) {
        if (insert_before || (insert_after && !insert_next_time)) {
            io_err_fmt("couldn't find entry specified as position modifier, appending your entries\n");
        }
        for (i = 0; i < nfiles; i++) {
            if (fnames[i][0] != '\0') {
                if (verbose) {
                    io_err_fmt("appending %s\n", fnames[i]);
                }
                append_file_to_archive(fnames[i], new_archive_fd);
            }
        }
    }

    rewrite_archive_header(new_archive_fd);
    io_close(old_archive_fd);
    io_close(new_archive_fd);
    return 1;
}
