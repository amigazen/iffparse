/*
** iffparse - Parse and display IFF file contents
** Main header file with all includes and function prototypes
**
** All C code must be C89/ANSI C compliant for SAS/C compiler
*/

#ifndef IFFPARSE_MAIN_H
#define IFFPARSE_MAIN_H

/* AmigaOS includes */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <libraries/iffparse.h>
#include <utility/tagitem.h>

/* AmigaOS proto includes */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/utility.h>

/* Function prototypes */
int main(int argc, char **argv);
VOID PrintChunkID(ULONG id);
VOID PrintChunkInfo(struct ContextNode *cn, LONG indent, struct IFFHandle *iff, ULONG formType);
VOID PrintTextChunk(struct IFFHandle *iff, ULONG chunkID, LONG indent);
VOID PrintBinaryMetadata(struct IFFHandle *iff, ULONG chunkID, ULONG formType, LONG indent);
STRPTR GetChunkDescription(ULONG chunkID);

#endif /* IFFPARSE_MAIN_H */

