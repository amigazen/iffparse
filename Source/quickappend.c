/*
** iffar - IFF CAT archiver quick append
** Based on public domain code by Karl Lehenbauer (1988).
** Ported to Amiga DOS I/O and C89. No wildcard expansion (use explicit paths).
*/

#include "io.h"

extern int verbose;
extern int append_file_to_archive(char *fname, int archive_fd);
extern int rewrite_archive_header(int fd);

void quickappend_entries(int archive_fd, char *entryname_pointers[], int entrycount)
{
    int files;

    for (files = 0; files < entrycount; files++) {
        if (append_file_to_archive(entryname_pointers[files], archive_fd)) {
            if (verbose) {
                io_err_fmt("appended %s\n", entryname_pointers[files]);
            }
        }
    }
    if (!rewrite_archive_header(archive_fd)) {
        io_err_fmt("rewrite of archive header failed... archive is presumed blown\n");
    }
    io_close(archive_fd);
}
