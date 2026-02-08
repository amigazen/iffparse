/*
** iffar - IFF CAT/LIST archiver, Amiga-native CLI
** Based on public domain iffar by Karl Lehenbauer (1988).
** This version uses ReadArgs and Amiga DOS; same archive logic.
** C89 compliant.
*/

static const char *verstag = "$VER: iffar 1.0 (08.02.2025)";
static const char *stack_cookie = "$STACK: 8192";

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>

#include "io.h"
#include "iff.h"

#define CMD_LIST   1
#define CMD_EXTRACT 2
#define CMD_DELETE  3
#define CMD_REPLACE 4
#define CMD_QUICKAPPEND 5

int verbose = 0;
int insert_after = 0;
int insert_before = 0;
int suppress_creation_message = 0;
char *location_modifier_name = NULL;

extern int table_of_contents(char *fname);
extern int extract(char *archive_name, char *fnames[], int nfiles);
extern int delete_entries(char *archive_name, char *fnames[], int nfiles);
extern int replace_entries(char *archive_name, char *fnames[], int nfiles);
extern int open_quick_append(char *fname);
extern void quickappend_entries(int archive_fd, char *entryname_pointers[], int entrycount);
extern void checknew(char *fname);
extern void cleanup(void);

static const char TEMPLATE[] = "ARCHIVE/K,LIST/S,EXTRACT/S,DELETE/S,REPLACE/S,QUICKAPPEND/S,AFTER/K,BEFORE/K,VERBOSE/S,SUPPRESSCREATE/S,FILES/M";
static const char USAGE[] =
    "Usage: iffar ARCHIVE=<file> [LIST] [EXTRACT] [DELETE] [REPLACE] [QUICKAPPEND]\n"
    "            [AFTER=<name>] [BEFORE=<name>] [VERBOSE] [SUPPRESSCREATE] [FILES=<file> ...]\n"
    "  LIST         - table of contents\n"
    "  EXTRACT      - extract (all if no FILES)\n"
    "  DELETE       - delete named entries\n"
    "  REPLACE      - replace/append named entries\n"
    "  QUICKAPPEND  - append without rewriting archive\n"
    "  AFTER/BEFORE - position for REPLACE\n";

static int count_files(char **files)
{
    int n;

    n = 0;
    if (files) {
        while (files[n] != NULL) {
            n++;
        }
    }
    return n;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[11];
    char archive_name[256];
    char **files;
    int nfiles;
    int cmd;
    int archive_fd;

    (void)argc;
    (void)argv;

    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        io_err_fmt("iffar: bad arguments (use LIST, EXTRACT, DELETE, REPLACE or QUICKAPPEND)\n");
        PutStr((STRPTR)USAGE);
        return RETURN_FAIL;
    }

    if (!args[0]) {
        io_err_fmt("iffar: ARCHIVE= required\n");
        PutStr((STRPTR)USAGE);
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    strncpy(archive_name, (char *)args[0], sizeof(archive_name) - 1);
    archive_name[sizeof(archive_name) - 1] = '\0';

    cmd = 0;
    if (args[1]) {
        cmd = CMD_LIST;
    } else if (args[2]) {
        cmd = CMD_EXTRACT;
    } else if (args[3]) {
        cmd = CMD_DELETE;
    } else if (args[4]) {
        cmd = CMD_REPLACE;
    } else if (args[5]) {
        cmd = CMD_QUICKAPPEND;
    }

    if (!cmd) {
        io_err_fmt("iffar: specify one of LIST, EXTRACT, DELETE, REPLACE, QUICKAPPEND\n");
        PutStr((STRPTR)USAGE);
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    if (args[8]) { verbose = 1; }
    if (args[9]) { suppress_creation_message = 1; }
    if (args[6]) { insert_after = 1; location_modifier_name = (char *)args[6]; }
    if (args[7]) {
        if (insert_after) {
            io_err_fmt("iffar: cannot use both AFTER and BEFORE\n");
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        insert_before = 1;
        location_modifier_name = (char *)args[7];
    }

    files = (args[10] != 0) ? (char **)args[10] : NULL;
    nfiles = count_files(files);

    if (cmd != CMD_LIST && cmd != CMD_EXTRACT && nfiles < 1) {
        io_err_fmt("iffar: at least one FILES= entry required for this command\n");
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    if ((insert_before || insert_after) && cmd != CMD_REPLACE) {
        io_err_fmt("iffar: AFTER/BEFORE only valid with REPLACE\n");
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    if ((insert_before || insert_after) && nfiles < 1) {
        io_err_fmt("iffar: AFTER/BEFORE requires at least one FILES= entry\n");
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    switch (cmd) {
    case CMD_LIST:
        if (!table_of_contents(archive_name)) {
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        break;

    case CMD_EXTRACT:
        if (!extract(archive_name, files, nfiles)) {
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        break;

    case CMD_DELETE:
        if (!delete_entries(archive_name, files, nfiles)) {
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        break;

    case CMD_REPLACE:
        if (!replace_entries(archive_name, files, nfiles)) {
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        break;

    case CMD_QUICKAPPEND:
        checknew(archive_name);
        archive_fd = open_quick_append(archive_name);
        if (archive_fd == IFFAR_FD_INVALID) {
            FreeArgs(rdargs);
            return RETURN_FAIL;
        }
        quickappend_entries(archive_fd, files, nfiles);
        break;
    }

    cleanup();
    FreeArgs(rdargs);
    return RETURN_OK;
}
