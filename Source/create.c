/*
** iffar - IFF CAT archiver output and file copy routines
** Based on public domain code by Karl Lehenbauer (1988), from EA IFF code.
** Ported to Amiga DOS I/O and C89.
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <string.h>
#include <proto/exec.h>

#include "iff.h"
#include "io.h"
#include "iffar_assert.h"

extern ULONG nextchunk(int fd, LONG *chunksize, LONG *chunk_bytes_left);
extern int readchunk(int fd, char *buf, LONG size, LONG *chunk_bytes_left);
extern int skipchunk(int fd, LONG chunksize, LONG *chunk_bytes_left);
extern int verbose;
extern char *basename(char *fname);

/* Forward declarations for functions defined later in this file (avoid implicit int vs void). */
static int WriteChunk(int fd, ULONG chunktype, char *data, long length);
extern int create_archive(char *archive_name, ULONG subtype);
extern void copychunkbytes(int infd, int outfd, long nbytes, long *filebytes_left_ptr);

#define ID_MISC MakeID('M','I','S','C')

int WriteCATheader(int fd)
{
    static ULONG dummy = ID_MISC;

    return WriteChunk(fd, (ULONG)ID_CAT, (char *)&dummy, sizeof(dummy));
}

int WriteChunkHeader(int fd, ULONG chunktype, long length)
{
    ChunkHeader chunkheader;

    chunkheader.ckID = (ID)chunktype;
    chunkheader.ckSize = length;

    if (io_write(fd, &chunkheader, sizeof(chunkheader)) == -1) {
        io_perror("WriteChunkHeader");
        return 0;
    }
    return 1;
}

int WriteSuperChunkHeader(int fd, ULONG chunktype, ULONG subtype, long length)
{
    if (!WriteChunkHeader(fd, chunktype, length + (long)sizeof(subtype))) {
        return 0;
    }
    return (io_write(fd, &subtype, sizeof(subtype)) != -1);
}

int WriteCATentry(int fd, char *fname, ULONG chunktype, ULONG subtype, long length)
{
    int fnamelen;
    int calc_chunk_length;

    fnamelen = (int)strlen(fname);
    calc_chunk_length = fnamelen;
    if (fnamelen & 1) {
        calc_chunk_length++;
    }
    calc_chunk_length += (int)length + (int)sizeof(ChunkHeader);

    if (!WriteSuperChunkHeader(fd, chunktype, subtype, (long)calc_chunk_length)) {
        return 0;
    }
    if (!WriteChunk(fd, (ULONG)ID_FNAM, fname, (long)strlen(fname))) {
        return 0;
    }
    return 1;
}

static int WriteChunk(int fd, ULONG chunktype, char *data, long length)
{
    static char zero = '\0';

    if (!WriteChunkHeader(fd, chunktype, length)) {
        return 0;
    }
    if (length != 0) {
        if (io_write(fd, data, length) == -1) {
            io_perror("WriteChunk");
            return 0;
        }
    }
    if (length & 1) {
        if (io_write(fd, &zero, 1) == -1) {
            io_perror("WriteChunk");
            return 0;
        }
    }
    return 1;
}

void checknew(char *fname)
{
    extern int suppress_creation_message;
    int fd;

    if (!suppress_creation_message) {
        fd = io_open_read(fname);
        if (fd == IFFAR_FD_INVALID) {
            io_err_fmt("Creating IFF CAT archive '%s'\n", fname);
        } else {
            io_close(fd);
        }
    }
}

int open_quick_append(char *fname)
{
    ChunkHeader mychunkheader;
    long filesize;
    int fd;

    fd = io_open_read(fname);
    if (fd == IFFAR_FD_INVALID) {
        fd = create_archive(fname, ID_MISC);
        if (fd == IFFAR_FD_INVALID) {
            io_perror(fname);
            return -1;
        }
        return fd;
    }

    if (io_read(fd, &mychunkheader, sizeof(mychunkheader)) != (long)sizeof(mychunkheader)) {
        io_perror(fname);
        io_err_fmt("couldn't read chunk header\n");
        io_close(fd);
        return -1;
    }

    if (mychunkheader.ckID != (ID)ID_CAT) {
        io_err_fmt("file '%s' is not an IFF CAT archive\n", fname);
        io_close(fd);
        return -1;
    }

    filesize = io_seek(fd, 0, IFFAR_SEEK_END);
    if (filesize == -1) {
        io_perror(fname);
        io_close(fd);
        return -1;
    }

    if ((long)(filesize - (long)sizeof(ChunkHeader)) != (long)mychunkheader.ckSize) {
        io_err_fmt("archive %s's CAT chunk size does not equal file size.\n", fname);
    }

    io_close(fd);
    fd = io_open_append(fname);
    if (fd == IFFAR_FD_INVALID) {
        io_perror(fname);
        return -1;
    }
    return fd;
}

#define COPY_BUFFER_SIZE 32768

static char *copy_buffer = 0;

void cleanup_copy_buffer(void);
extern void add_cleanup(void (*function)(void));
extern void panic(const char *s);

int append_file_to_archive(char *fname, int archive_fd)
{
    char *basename_ptr;
    int basenamelen;
    int infd;
    ULONG chunkid;
    ULONG subtype;
    long chunksize;
    long new_chunk_address;
    ULONG subchunkid;
    long subchunksize;
    long calculated_chunk_size;
    long inputfilesize;
    long placeholder;

    io_seek(archive_fd, 0, IFFAR_SEEK_END);

    infd = io_open_read(fname);
    if (infd == IFFAR_FD_INVALID) {
        io_perror(fname);
        return 0;
    }

    inputfilesize = io_seek(infd, 0, IFFAR_SEEK_END);
    io_seek(infd, 0, IFFAR_SEEK_SET);

    chunkid = nextchunk(infd, &chunksize, &inputfilesize);
    if (chunkid == 0) {
        io_err_fmt("couldn't get header chunk from file %s\n", fname);
        io_close(infd);
        return 0;
    }

    if (chunkid != (ULONG)ID_CAT && chunkid != (ULONG)ID_FORM && chunkid != (ULONG)ID_LIST) {
        io_err_fmt("file %s is not an IFF CAT, FORM or LIST, ignored\n", fname);
        io_close(infd);
        return 0;
    }

    if (io_read(infd, &subtype, 4) != 4) {
        io_perror("copy subtype");
        io_close(infd);
        return 0;
    }
    inputfilesize -= 4;

    new_chunk_address = io_seek(archive_fd, 0, IFFAR_SEEK_CUR) + 4;

    if (!WriteChunk(archive_fd, chunkid, (char *)&subtype, 4)) {
        io_perror("append WriteChunk");
        io_close(infd);
        return 0;
    }

    calculated_chunk_size = 4;

    basename_ptr = basename(fname);
    basenamelen = (int)strlen(basename_ptr);

    if (!WriteChunk(archive_fd, (ULONG)ID_FNAM, basename_ptr, (long)basenamelen)) {
        io_close(infd);
        return 0;
    }

    calculated_chunk_size += (long)sizeof(ChunkHeader) + basenamelen;
    if (basenamelen & 1) {
        calculated_chunk_size++;
    }

    while ((subchunkid = nextchunk(infd, &subchunksize, &inputfilesize)) != 0) {
        if (subchunkid == (ULONG)ID_FNAM) {
            skipchunk(infd, subchunksize, &inputfilesize);
        } else {
            calculated_chunk_size += subchunksize + (long)sizeof(ChunkHeader);
            if (subchunksize & 1) {
                calculated_chunk_size++;
            }
            if (!WriteChunkHeader(archive_fd, subchunkid, subchunksize)) {
                io_close(infd);
                return 0;
            }
            copychunkbytes(infd, archive_fd, subchunksize, &inputfilesize);
        }
    }

    placeholder = io_seek(archive_fd, 0, IFFAR_SEEK_CUR);
    io_seek(archive_fd, new_chunk_address, IFFAR_SEEK_SET);
    if (io_write(archive_fd, &calculated_chunk_size, 4) != 4) {
        io_perror("archive subheader rewrite");
        io_err_fmt("archive is blown.\n");
        io_close(infd);
        return 0;
    }
    io_seek(archive_fd, placeholder, IFFAR_SEEK_SET);
    io_close(infd);
    return 1;
}

int rewrite_archive_header(int fd)
{
    long filesize;

    filesize = io_seek(fd, 0, IFFAR_SEEK_END);
    if (filesize == -1) {
        io_perror("archive");
        return 0;
    }
    if (io_seek(fd, 0, IFFAR_SEEK_SET) == -1) {
        io_perror("archive cleanup seek");
        return 0;
    }
    if (!WriteChunkHeader(fd, (ULONG)ID_CAT, filesize - (long)sizeof(ChunkHeader))) {
        io_perror("archive cleanup");
        return 0;
    }
    return 1;
}

void cleanup_copy_buffer(void)
{
    if (copy_buffer) {
        FreeMem(copy_buffer, COPY_BUFFER_SIZE);
        copy_buffer = 0;
    }
}

void copychunkbytes(int infd, int outfd, long nbytes, long *filebytes_left_ptr)
{
    int copysize;
    int odd;

    if (!copy_buffer) {
        copy_buffer = (char *)AllocMem(COPY_BUFFER_SIZE, MEMF_PUBLIC);
        if (!copy_buffer) {
            panic("couldn't allocate copy buffer");
        }
        add_cleanup(cleanup_copy_buffer);
    }

    if (nbytes > *filebytes_left_ptr) {
        io_err_fmt("copychunkbytes: chunk size exceeds superchunk - truncating\n");
        nbytes = *filebytes_left_ptr;
    }

    odd = (int)(nbytes & 1);

    while (nbytes > 0) {
        copysize = (nbytes > COPY_BUFFER_SIZE) ? COPY_BUFFER_SIZE : (int)nbytes;

        if (io_read(infd, copy_buffer, copysize) != copysize) {
            io_perror("copybytes input");
            io_err_fmt("archive is blown.\n");
            io_close(infd);
            return;
        }
        if (io_write(outfd, copy_buffer, copysize) != copysize) {
            io_perror("copybytes output");
            io_err_fmt("archive is blown.\n");
            io_close(infd);
            return;
        }
        nbytes -= copysize;
        *filebytes_left_ptr -= copysize;
    }

    if (odd) {
        if (io_read(infd, copy_buffer, 1) != 1) {
            io_perror("copychunkbytes: failed to skip input byte");
        }
        io_write(outfd, copy_buffer, 1);
        (*filebytes_left_ptr)--;
    }
}

int create_archive(char *archive_name, ULONG subtype)
{
    int archive_fd;

    (void)subtype;

    archive_fd = io_open_create(archive_name);
    if (archive_fd == IFFAR_FD_INVALID) {
        io_perror(archive_name);
        return -1;
    }

    if (!WriteCATheader(archive_fd)) {
        io_err_fmt("create_archive: couldn't write CAT chunk header\n");
        io_close(archive_fd);
        return -1;
    }

    return archive_fd;
}
