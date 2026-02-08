/*
** iffar - IFF CAT archiver, IFF support functions
** Based on public domain code by Karl Lehenbauer (1988), from EA IFF code.
** Ported to Amiga DOS I/O and C89.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <ctype.h>

#include "iff.h"
#include "io.h"
#include "iffar_assert.h"

ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left);

void PutID(ID id)
{
    io_put_id((ULONG)id);
}

UBYTE *MyAllocMem(ULONG bytes, ULONG type)
{
    UBYTE *tmp;

    tmp = (UBYTE *)AllocMem(bytes, type);
    return tmp;
}

ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left)
{
    int sawsize;
    int i;
    int blown;
    ChunkHeader mychunkheader;
    char checkchar;

    blown = 0;

    if (*chunk_bytes_left == 0) {
        return 0;
    }

    sawsize = (int)io_read(fd, &mychunkheader, sizeof(mychunkheader));
    if (sawsize != (int)sizeof(mychunkheader)) {
        if (sawsize != 0) {
            io_err_fmt("Something's wrong with nextchunk! (sawsize %d)\n", sawsize);
        }
        *chunksize = 0;
        return 0;
    }

    *chunksize = mychunkheader.ckSize;

    for (i = 0; i < 4; i++) {
        checkchar = (char)((mychunkheader.ckID >> (i * 8)) & 0xff);
        if (!isprint((unsigned char)checkchar)) {
            if (!blown) {
                blown = 1;
                io_err_fmt("nextchunk: chunk ID contains unprintable character (0x%x)\n",
                           (unsigned char)checkchar);
            }
            break;
        }
    }

    if (mychunkheader.ckSize < 0 || (ULONG)mychunkheader.ckSize > (ULONG)MAXCHUNKSIZE) {
        io_err_fmt("nextchunk: chunk length of %ld is unreasonable\n", (long)mychunkheader.ckSize);
        blown = 1;
    }

    if (blown) {
        io_err_fmt("nextchunk: I either got lost or the archive is blown\n");
        return 0;
    }

    *chunk_bytes_left -= (LONG)sizeof(mychunkheader);

    if (*chunk_bytes_left < 0) {
        io_err_fmt("nextchunk: chunk overran its parent by %d bytes\n", (int)(0 - *chunk_bytes_left));
        *chunksize = 0;
        *chunk_bytes_left = 0;
        return 0;
    }

    return (ULONG)mychunkheader.ckID;
}

int readchunk(int fd, char *buf, LONG size, LONG *chunk_bytes_left)
{
    *chunk_bytes_left -= size;

    if (*chunk_bytes_left < 0) {
        io_err_fmt("readchunk: chunk requested passed the end of its parent chunk\n");
        *chunk_bytes_left = 0;
        return 0;
    }

    if (io_read(fd, buf, size) != size) {
        io_perror("readchunk");
        io_err_fmt("read of IFF chunk failed\n");
        return 0;
    }

    if (size & 1) {
        io_seek(fd, 1L, IFFAR_SEEK_CUR);
        (*chunk_bytes_left)--;
    }
    return 1;
}

int skipchunk(int fd, LONG chunksize, LONG *chunk_bytes_left)
{
    *chunk_bytes_left -= chunksize;
    if (chunksize & 1) {
        (*chunk_bytes_left)--;
    }
    if (*chunk_bytes_left < 0) {
        io_err_fmt("skipchunk: chunk size passes end of parent chunk by %d bytes\n",
                   (int)(0 - *chunk_bytes_left));
        return 0;
    }
    if (io_seek(fd, (long)chunksize, IFFAR_SEEK_CUR) == -1) {
        return 0;
    }
    if (chunksize & 1) {
        io_seek(fd, 1L, IFFAR_SEEK_CUR);
    }
    return 1;
}

int OpenIFF(char *fname, LONG expected_formtype, LONG *length_ptr)
{
    int iffile;
    ChunkHeader chunkhead;
    LONG formtype;

    iffile = io_open_read(fname);
    if (iffile == IFFAR_FD_INVALID) {
        io_err_fmt("OpenIFF: can't open IFF file %s\n", fname);
        io_perror(fname);
        return -1;
    }

    *length_ptr = io_seek(iffile, 0, IFFAR_SEEK_END);
    io_seek(iffile, 0, IFFAR_SEEK_SET);

    if (io_read(iffile, &chunkhead, sizeof(chunkhead)) < 0) {
        io_err_fmt("OpenIFF: initial read from IFF file %s failed!\n", fname);
        io_close(iffile);
        return -1;
    }

    if (chunkhead.ckID != (ID)ID_FORM) {
        io_err_fmt("OpenIFF: File %s isn't IFF FORM\n", fname);
        io_close(iffile);
        return -1;
    }

    if (io_read(iffile, &formtype, sizeof(formtype)) != sizeof(formtype)) {
        io_close(iffile);
        return -1;
    }

    if (formtype != expected_formtype) {
        io_err_fmt("OpenIFF: File %s is IFF ", fname);
        PutID((ID)formtype);
        io_err_fmt(" rather than requested ");
        PutID((ID)expected_formtype);
        io_err_fmt("\n");
        io_close(iffile);
        return -1;
    }
    return iffile;
}

LONG chunkuntil(int fd, ULONG chunktype, long *file_bytes_left)
{
    ULONG currentchunk;
    LONG chunksize;

    while ((currentchunk = nextchunk(fd, &chunksize, file_bytes_left)) != 0) {
        if (currentchunk == chunktype) {
            return chunksize;
        }
        skipchunk(fd, chunksize, file_bytes_left);
    }
    return 0;
}

int OpenCAT(char *archive_name, ULONG *subtype_ptr, LONG *length_ptr)
{
    ChunkHeader mychunkheader;
    int archive_fd;
    long start_of_body;
    long filesize;

    archive_fd = io_open_read(archive_name);
    if (archive_fd == IFFAR_FD_INVALID) {
        return -1;
    }

    if (io_read(archive_fd, &mychunkheader, sizeof(mychunkheader)) != (long)sizeof(mychunkheader)) {
        io_perror(archive_name);
        io_err_fmt("couldn't read chunk header\n");
        io_close(archive_fd);
        return -1;
    }

    if (mychunkheader.ckID != (ID)ID_CAT) {
        io_err_fmt("file '%s' is not an IFF CAT archive\n", archive_name);
        io_close(archive_fd);
        return -1;
    }

    if (io_read(archive_fd, subtype_ptr, sizeof(ULONG)) != (long)sizeof(ULONG)) {
        io_err_fmt("error reading archive header - subtype\n");
        io_close(archive_fd);
        return -1;
    }

    start_of_body = io_seek(archive_fd, 0, IFFAR_SEEK_CUR);
    if (start_of_body == -1) {
        io_perror(archive_name);
        io_close(archive_fd);
        return -1;
    }

    filesize = io_seek(archive_fd, 0, IFFAR_SEEK_END);
    if (filesize == -1) {
        io_perror(archive_name);
        io_close(archive_fd);
        return -1;
    }

    if ((long)(filesize - (long)sizeof(ChunkHeader)) != (long)mychunkheader.ckSize) {
        io_err_fmt("archive %s's CAT chunk size does not match file size.\n", archive_name);
        io_close(archive_fd);
        return -1;
    }

    if (io_seek(archive_fd, start_of_body, IFFAR_SEEK_SET) == -1) {
        io_perror(archive_name);
        io_close(archive_fd);
        return -1;
    }

    *length_ptr = (LONG)filesize;
    return archive_fd;
}

ULONG nextCATchunk(int fd, ULONG *subtype_ptr, char *fname_ptr,
                  LONG *chunk_length_ptr, LONG *metachunk_length_ptr)
{
    ULONG chunkid;
    ULONG innerchunkid;
    long innerchunkposition;
    long chunksize;
    long innerchunksize;
    int odd;

    *subtype_ptr = 0L;
    *fname_ptr = '\0';

    chunkid = nextchunk(fd, chunk_length_ptr, metachunk_length_ptr);
    if (chunkid == 0L) {
        return 0L;
    }

    if (chunkid != (ULONG)ID_FORM && chunkid != (ULONG)ID_CAT && chunkid != (ULONG)ID_LIST) {
        return chunkid;
    }

    if (io_read(fd, subtype_ptr, 4) != 4) {
        io_perror("reading subtype");
        return 0;
    }

    *chunk_length_ptr -= (LONG)sizeof(ULONG);
    *metachunk_length_ptr -= (LONG)sizeof(ULONG);

    IFFAR_ASSERT(*chunk_length_ptr > 0);

    innerchunkposition = io_seek(fd, 0L, IFFAR_SEEK_CUR);
    chunksize = *chunk_length_ptr;
    innerchunkid = nextchunk(fd, &innerchunksize, &chunksize);
    if (innerchunkid != (ULONG)ID_FNAM) {
        io_seek(fd, innerchunkposition, IFFAR_SEEK_SET);
        return chunkid;
    }

    odd = (int)(innerchunksize & 1);

    if (!readchunk(fd, fname_ptr, innerchunksize, &chunksize)) {
        io_err_fmt("nextCATchunk: got into trouble reading chunk text\n");
        return 0;
    }
    fname_ptr[innerchunksize] = '\0';

    *chunk_length_ptr -= (LONG)(sizeof(ChunkHeader) + innerchunksize);
    *metachunk_length_ptr -= (LONG)(sizeof(ChunkHeader) + innerchunksize);
    if (odd) {
        (*chunk_length_ptr)--;
        (*metachunk_length_ptr)--;
    }
    return chunkid;
}
