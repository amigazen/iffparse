/*
** Copyright (c) 2025 amigazen project
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
**    this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
**    this list of conditions and the following disclaimer in the documentation
**    and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
*/

#include "main.h"
#include <prefs/asl.h>
#include <prefs/font.h>
#include <prefs/icontrol.h>
#include <prefs/input.h>
#include <prefs/locale.h>
#include <prefs/overscan.h>
#include <prefs/palette.h>
#include <prefs/pointer.h>
#include <prefs/prefhdr.h>
#include <prefs/printergfx.h>
#include <prefs/printerps.h>
#include <prefs/printertxt.h>
#include <prefs/reaction.h>
#include <prefs/screenmode.h>
#include <prefs/serial.h>
#include <prefs/sound.h>
#include <prefs/wbpattern.h>
#include <prefs/workbench.h>

/* Amiga version strings - kept as static to prevent "unreachable" warnings */
/* These are referenced by the linker/loader, not by code */
static const char *verstag = "$VER: iffparse 1.0 (31.12.2025)";
static const char *stack_cookie = "$STACK: 4096";
long oslibversion  = 40L; 

/* Command-line template - one required positional file argument and optional QUICK switch */
static const char TEMPLATE[] = "FILE/A,QUICK/S";

/* Usage string */
static const char USAGE[] = "Usage: iffparse FILE/A [QUICK/S]\n"
                             "  FILE/A - Input IFF file to parse\n"
                             "  QUICK/S - Show only chunk structure (no descriptions or metadata details)\n";

/* Library base - needed for proto includes */
struct Library *IFFParseBase;

/* IFF chunk IDs */
#define MAKE_ID(a,b,c,d) \
        ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))

#define ID_FORM      MAKE_ID('F','O','R','M')
#define ID_ILBM      MAKE_ID('I','L','B','M')
#define ID_PBM       MAKE_ID('P','B','M',' ')
#define ID_RGBN      MAKE_ID('R','G','B','N')
#define ID_RGB8      MAKE_ID('R','G','B','8')
#define ID_DEEP      MAKE_ID('D','E','E','P')
#define ID_ACBM      MAKE_ID('A','C','B','M')
#define ID_FAXX      MAKE_ID('F','A','X','X')
#define ID_BMHD      MAKE_ID('B','M','H','D')
#define ID_CMAP      MAKE_ID('C','M','A','P')
#define ID_CAMG      MAKE_ID('C','A','M','G')
#define ID_BODY      MAKE_ID('B','O','D','Y')
#define ID_ABIT      MAKE_ID('A','B','I','T')
#define ID_FXHD      MAKE_ID('F','X','H','D')
#define ID_PAGE      MAKE_ID('P','A','G','E')
#define ID_FLOG      MAKE_ID('F','L','O','G')
#define ID_GRAB      MAKE_ID('G','R','A','B')
#define ID_DEST      MAKE_ID('D','E','S','T')
#define ID_SPRT      MAKE_ID('S','P','R','T')
#define ID_CRNG      MAKE_ID('C','R','N','G')
#define ID_CCRT      MAKE_ID('C','C','R','T')
#define ID_TEXT      MAKE_ID('T','E','X','T')
#define ID_ANNO      MAKE_ID('A','N','N','O')
#define ID_AUTH      MAKE_ID('A','U','T','H')
#define ID_COPYRIGHT MAKE_ID('(','c',')',' ')
#define ID_NAME      MAKE_ID('N','A','M','E')
#define ID_CHRS      MAKE_ID('C','H','R','S')
#define ID_LIST      MAKE_ID('L','I','S','T')
#define ID_CAT       MAKE_ID('C','A','T',' ')
#define ID_PROP      MAKE_ID('P','R','O','P')
#define ID_DGBL      MAKE_ID('D','G','B','L')
#define ID_DPEL      MAKE_ID('D','P','E','L')
#define ID_DLOC      MAKE_ID('D','L','O','C')
#define ID_DBOD      MAKE_ID('D','B','O','D')
#define ID_DCHG      MAKE_ID('D','C','H','G')
#define ID_GPHD      MAKE_ID('G','P','H','D')
#define ID_YUVN      MAKE_ID('Y','U','V','N')
#define ID_YCHD      MAKE_ID('Y','C','H','D')
#define ID_DATY      MAKE_ID('D','A','T','Y')
#define ID_DATU      MAKE_ID('D','A','T','U')
#define ID_DATV      MAKE_ID('D','A','T','V')
/* PREF chunk IDs */
#define ID_PREF      MAKE_ID('P','R','E','F')
#define ID_PRHD      MAKE_ID('P','R','H','D')

/*
** PrintChunkID - Print a 4-character chunk ID in readable format
*/
VOID PrintChunkID(ULONG id)
{
    UBYTE c1, c2, c3, c4;
    BPTR output;
    
    c1 = (UBYTE)((id >> 24) & 0xFF);
    c2 = (UBYTE)((id >> 16) & 0xFF);
    c3 = (UBYTE)((id >> 8) & 0xFF);
    c4 = (UBYTE)(id & 0xFF);
    
    output = Output();
    FPutC(output, (LONG)c1);
    FPutC(output, (LONG)c2);
    FPutC(output, (LONG)c3);
    FPutC(output, (LONG)c4);
}

/*
** GetChunkDescription - Get a human-readable description for a chunk ID
** Returns: Pointer to description string, or NULL if unknown
*/
STRPTR GetChunkDescription(ULONG chunkID)
{
    switch (chunkID) {
        case ID_FORM: return "IFF Form container";
        case ID_ILBM: return "InterLeaved BitMap image";
        case ID_PBM: return "Packed BitMap image";
        case ID_RGBN: return "RGB with N planes image";
        case ID_RGB8: return "RGB 8-bit image";
        case ID_DEEP: return "Deep format image";
        case ID_ACBM: return "Amiga Continuous BitMap image";
        case ID_FAXX: return "Facsimile image";
        case ID_BMHD: return "Bitmap Header";
        case ID_CMAP: return "Color Map (palette)";
        case ID_CAMG: return "Amiga Viewport Modes";
        case ID_BODY: return "Image body data";
        case ID_ABIT: return "Alpha bitmap data";
        case ID_FXHD: return "FAXX Header";
        case ID_PAGE: return "FAXX Page data";
        case ID_FLOG: return "FAXX Log";
        case ID_GRAB: return "Hotspot coordinates";
        case ID_DEST: return "Destination merge";
        case ID_SPRT: return "Sprite precedence";
        case ID_CRNG: return "Color range";
        case ID_CCRT: return "Color Cycling Range and Timing";
        case ID_COPYRIGHT: return "Copyright text";
        case ID_AUTH: return "Author text";
        case ID_ANNO: return "Annotation text";
        case ID_TEXT: return "Text data";
        case ID_NAME: return "Name of art/music";
        case ID_CHRS: return "Character string";
        case ID_DGBL: return "Deep Global information";
        case ID_DPEL: return "Deep Pixel Elements";
        case ID_DLOC: return "Deep display Location";
        case ID_DBOD: return "Deep Body data";
        case ID_DCHG: return "Deep Change buffer";
        case ID_GPHD: return "GPSoftware FAXX Header";
        case ID_YUVN: return "YUV image form";
        case ID_YCHD: return "YUV Header";
        case ID_DATY: return "YUV Y (luminance) data";
        case ID_DATU: return "YUV U (color-difference) data";
        case ID_DATV: return "YUV V (color-difference) data";
        case ID_PREF: return "Preferences Header";
        case ID_PRHD: return "Preferences Header";
        case ID_ASL: return "ASL Preferences";
        case ID_FONT: return "Font Preferences";
        case ID_ICTL: return "Intuition Control Preferences";
        case ID_IEXC: return "Intuition Exception Preferences";
        case ID_INPT: return "Input Preferences";
        case ID_LCLE: return "Locale Preferences";
        case ID_CTRY: return "Country Preferences";
        case ID_OSCN: return "Overscan Preferences";
        case ID_PALT: return "Palette Preferences";
        case ID_PNTR: return "Pointer Preferences";
        case ID_PGFX: return "Printer Graphics Preferences";
        case ID_PSPD: return "PostScript Printer Preferences";
        case ID_PTXT: return "Text Printer Preferences";
        case ID_PUNT: return "Printer Unit Preferences";
        case ID_PDEV: return "Printer Device Unit Preferences";
        case ID_RACT: return "Reaction Preferences";
        case ID_SCRM: return "Screen Mode Preferences";
        case ID_SERL: return "Serial Preferences";
        case ID_SOND: return "Sound Preferences";
        case ID_PTRN: return "Workbench Pattern Preferences";
        case ID_WBNC: return "Workbench Preferences";
        case ID_WBHD: return "Workbench Hidden Device Preferences";
        case ID_WBTF: return "Workbench Title Format Preferences";
        default: return NULL;
    }
}

/*
** PrintTextChunk - Read and print text chunk contents
** For text chunks, outputs the actual text content
*/
VOID PrintTextChunk(struct IFFHandle *iff, ULONG chunkID, LONG indent)
{
    LONG i;
    LONG chunkSize;
    UBYTE *buffer;
    UBYTE c;
    LONG bytesRead;
    STRPTR desc;
    
    /* Get chunk size from current chunk */
    {
        struct ContextNode *cn;
        cn = CurrentChunk(iff);
        if (!cn) {
            return;
        }
        chunkSize = cn->cn_Size;
    }
    
    if (chunkSize <= 0) {
        return;
    }
    
    /* Allocate buffer for text (add 1 for null terminator) */
    buffer = (UBYTE *)AllocMem(chunkSize + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer) {
        return;
    }
    
    /* Read chunk data */
    bytesRead = ReadChunkBytes(iff, buffer, chunkSize);
    if (bytesRead != chunkSize) {
        FreeMem(buffer, chunkSize + 1);
        return;
    }
    
    /* Null-terminate the string */
    buffer[chunkSize] = '\0';
    
    /* Print indentation */
    for (i = 0; i < indent; i++) {
        PutStr("  ");
    }
    
    /* Print chunk description */
    desc = GetChunkDescription(chunkID);
    if (desc) {
        PutStr(desc);
        PutStr(": ");
    }
    
    /* Print text content, replacing non-printable characters */
    {
        BPTR output;
        output = Output();
        for (i = 0; i < chunkSize; i++) {
            c = buffer[i];
            if (c >= 32 && c <= 126) {
                /* Printable ASCII */
                FPutC(output, (LONG)c);
            } else if (c == '\n') {
            PutStr("\n");
            /* Print indentation for continuation lines */
            {
                LONG j;
                for (j = 0; j < indent + 1; j++) {
                    PutStr("  ");
                }
            }
            } else if (c == '\r') {
                /* Ignore carriage return */
            } else if (c == '\t') {
                PutStr("    ");
            } else if (c == 0) {
                /* Null terminator - stop here */
                break;
            } else {
                /* Non-printable - show as hex */
                {
                    UBYTE outputBuffer[8];
                    SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer), "\\x%02x", c);
                    PutStr((STRPTR)outputBuffer);
                }
            }
        }
    }
    
    PutStr("\n");
    
    /* Free buffer */
    FreeMem(buffer, chunkSize + 1);
}

/*
** PrintBinaryMetadata - Parse and display metadata from binary chunks
*/
VOID PrintBinaryMetadata(struct IFFHandle *iff, ULONG chunkID, ULONG formType, LONG indent)
{
    LONG i;
    LONG chunkSize;
    UBYTE *buffer;
    LONG bytesRead;
    UBYTE outputBuffer[256];
    
    /* Get chunk size */
    {
        struct ContextNode *cn;
        cn = CurrentChunk(iff);
        if (!cn) {
            return;
        }
        chunkSize = cn->cn_Size;
    }
    
    if (chunkSize <= 0) {
        return;
    }
    
    /* Allocate buffer */
    buffer = (UBYTE *)AllocMem(chunkSize, MEMF_PUBLIC);
    if (!buffer) {
        return;
    }
    
    /* Read chunk data */
    bytesRead = ReadChunkBytes(iff, buffer, chunkSize);
    if (bytesRead != chunkSize) {
        FreeMem(buffer, chunkSize);
        return;
    }
    
    /* Print indentation */
    for (i = 0; i < indent; i++) {
        PutStr("  ");
    }
    
    /* Parse based on chunk type */
    if (chunkID == ID_BMHD && chunkSize >= 20) {
        /* Bitmap Header */
        UWORD w, h;
        WORD x, y;
        UBYTE nPlanes, masking, compression;
        UWORD transparentColor;
        UBYTE xAspect, yAspect;
        WORD pageWidth, pageHeight;
        STRPTR maskingName;
        STRPTR compressionName;
        
        w = (UWORD)((buffer[0] << 8) | buffer[1]);
        h = (UWORD)((buffer[2] << 8) | buffer[3]);
        x = (WORD)((buffer[4] << 8) | buffer[5]);
        y = (WORD)((buffer[6] << 8) | buffer[7]);
        nPlanes = buffer[8];
        masking = buffer[9];
        compression = buffer[10];
        transparentColor = (UWORD)((buffer[12] << 8) | buffer[13]);
        xAspect = buffer[14];
        yAspect = buffer[15];
        pageWidth = (WORD)((buffer[16] << 8) | buffer[17]);
        pageHeight = (WORD)((buffer[18] << 8) | buffer[19]);
        
        /* Determine masking name */
        switch (masking) {
            case 0: maskingName = "None"; break;
            case 1: maskingName = "HasMask"; break;
            case 2: maskingName = "TransparentColor"; break;
            case 3: maskingName = "Lasso"; break;
            default: maskingName = "Unknown"; break;
        }
        
        /* Determine compression name */
        switch (compression) {
            case 0: compressionName = "None"; break;
            case 1: compressionName = "ByteRun1"; break;
            default: compressionName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Dimensions: %u x %u pixels, %u bitplanes\n", w, h, nPlanes);
        PutStr((STRPTR)outputBuffer);
        
        if (x != 0 || y != 0) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Position: (%d, %d)\n", (LONG)x, (LONG)y);
            PutStr((STRPTR)outputBuffer);
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Compression: %s, Masking: %s\n", compressionName, maskingName);
        PutStr((STRPTR)outputBuffer);
        
        if (masking == 2) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Transparent color: %u\n", transparentColor);
            PutStr((STRPTR)outputBuffer);
        }
        
        if (xAspect != 0 && yAspect != 0) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Aspect ratio: %u:%u\n", xAspect, yAspect);
            PutStr((STRPTR)outputBuffer);
        }
        
        if (pageWidth != w || pageHeight != h) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Page size: %d x %d pixels\n", (LONG)pageWidth, (LONG)pageHeight);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_CMAP && chunkSize >= 3) {
        /* Color Map */
        ULONG numColors;
        numColors = chunkSize / 3;
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  %lu color entries (RGB triplets)\n", numColors);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_CAMG && chunkSize >= 4) {
        /* Amiga Viewport Modes */
        ULONG modeID;
        STRPTR modeFlags[32];
        LONG flagCount;
        
        modeID = (ULONG)((buffer[0] << 24) | (buffer[1] << 16) | 
                        (buffer[2] << 8) | buffer[3]);
        
        flagCount = 0;
        if (modeID & 0x8000UL) modeFlags[flagCount++] = "HIRES";
        if (modeID & 0x0004UL) modeFlags[flagCount++] = "LACE";
        if (modeID & 0x0800UL) modeFlags[flagCount++] = "HAM";
        if (modeID & 0x0080UL) modeFlags[flagCount++] = "EHB";
        
        if (flagCount > 0) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  ModeID: 0x%08lx (", modeID);
            PutStr((STRPTR)outputBuffer);
            for (i = 0; i < flagCount; i++) {
                PutStr(modeFlags[i]);
                if (i < flagCount - 1) {
                    PutStr(", ");
                }
            }
            PutStr(")\n");
        } else {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  ModeID: 0x%08lx\n", modeID);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_GRAB && chunkSize >= 4) {
        /* Hotspot coordinates */
        WORD x, y;
        x = (WORD)((buffer[0] << 8) | buffer[1]);
        y = (WORD)((buffer[2] << 8) | buffer[3]);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Hotspot: (%d, %d)\n", (LONG)x, (LONG)y);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_SPRT && chunkSize >= 2) {
        /* Sprite precedence */
        UWORD precedence;
        precedence = (UWORD)((buffer[0] << 8) | buffer[1]);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Precedence: %u (0 = highest)\n", precedence);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_CRNG && chunkSize >= 8) {
        /* Color range */
        WORD rate, flags;
        UBYTE low, high;
        rate = (WORD)((buffer[2] << 8) | buffer[3]);
        flags = (WORD)((buffer[4] << 8) | buffer[5]);
        low = buffer[6];
        high = buffer[7];
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Range: %u-%u, Rate: %d, Flags: ", low, high, (LONG)rate);
        PutStr((STRPTR)outputBuffer);
        if (flags & 1) PutStr("ACTIVE ");
        if (flags & 2) PutStr("REVERSE");
        PutStr("\n");
    } else if (chunkID == ID_FXHD && chunkSize >= 16) {
        /* FAXX Header */
        UWORD width, height, lineLength, vRes;
        UBYTE compression;
        STRPTR compName;
        
        width = (UWORD)((buffer[0] << 8) | buffer[1]);
        height = (UWORD)((buffer[2] << 8) | buffer[3]);
        lineLength = (UWORD)((buffer[4] << 8) | buffer[5]);
        vRes = (UWORD)((buffer[6] << 8) | buffer[7]);
        compression = buffer[8];
        
        switch (compression) {
            case 0: compName = "None"; break;
            case 1: compName = "Modified Huffman (MH)"; break;
            case 2: compName = "Modified READ (MR)"; break;
            case 4: compName = "Modified Modified READ (MMR)"; break;
            default: compName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Dimensions: %u x %u pixels\n", width, height);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Line length: %u mm, VRes: %u lines/100mm\n", lineLength, vRes);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Compression: %s\n", compName);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_DGBL && chunkSize >= 6) {
        /* Deep Global information */
        UWORD displayWidth, displayHeight, compression;
        UBYTE xAspect, yAspect;
        STRPTR compName;
        
        displayWidth = (UWORD)((buffer[0] << 8) | buffer[1]);
        displayHeight = (UWORD)((buffer[2] << 8) | buffer[3]);
        compression = (UWORD)((buffer[4] << 8) | buffer[5]);
        xAspect = buffer[6];
        yAspect = buffer[7];
        
        switch (compression) {
            case 0: compName = "None"; break;
            case 1: compName = "RunLength"; break;
            case 2: compName = "Huffman"; break;
            case 3: compName = "DynamicHuff"; break;
            case 4: compName = "JPEG"; break;
            case 5: compName = "TVDC"; break;
            default: compName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Display: %u x %u pixels\n", displayWidth, displayHeight);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Compression: %s\n", compName);
        PutStr((STRPTR)outputBuffer);
        if (xAspect != 0 && yAspect != 0) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Aspect ratio: %u:%u\n", xAspect, yAspect);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_DLOC && chunkSize >= 8) {
        /* Deep Location */
        UWORD w, h;
        WORD x, y;
        w = (UWORD)((buffer[0] << 8) | buffer[1]);
        h = (UWORD)((buffer[2] << 8) | buffer[3]);
        x = (WORD)((buffer[4] << 8) | buffer[5]);
        y = (WORD)((buffer[6] << 8) | buffer[7]);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Size: %u x %u pixels, Position: (%d, %d)\n", 
                 w, h, (LONG)x, (LONG)y);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_DCHG && chunkSize >= 4) {
        /* Deep Change buffer */
        LONG frameRate;
        frameRate = (LONG)((buffer[0] << 24) | (buffer[1] << 16) | 
                          (buffer[2] << 8) | buffer[3]);
        if (frameRate == -1) {
            PutStr("  Frame separator (not animated)\n");
        } else if (frameRate == 0) {
            PutStr("  Frame rate: As fast as possible\n");
        } else {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Frame rate: %ld ms between frames\n", frameRate);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_YCHD && chunkSize >= 20) {
        /* YUV Header */
        UWORD width, height;
        UBYTE compress, flags, mode, norm;
        STRPTR modeName;
        STRPTR normName;
        
        width = (UWORD)((buffer[0] << 8) | buffer[1]);
        height = (UWORD)((buffer[2] << 8) | buffer[3]);
        /* pageWidth, pageHeight, leftEdge, topEdge, aspectX, aspectY are available but not displayed */
        compress = buffer[14];
        flags = buffer[15];
        mode = buffer[16];
        norm = buffer[17];
        
        switch (mode) {
            case 0: modeName = "400 (B&W)"; break;
            case 1: modeName = "411"; break;
            case 2: modeName = "422"; break;
            case 3: modeName = "444"; break;
            case 8: modeName = "200 (lores B&W)"; break;
            case 9: modeName = "211 (lores)"; break;
            case 10: modeName = "222 (lores)"; break;
            default: modeName = "Unknown"; break;
        }
        
        switch (norm) {
            case 0: normName = "Unknown"; break;
            case 1: normName = "PAL"; break;
            case 2: normName = "NTSC"; break;
            default: normName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Dimensions: %u x %u pixels, Mode: %s, Norm: %s\n",
                 width, height, modeName, normName);
        PutStr((STRPTR)outputBuffer);
        if (compress != 0) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Compression: %u\n", compress);
            PutStr((STRPTR)outputBuffer);
        }
        if (flags & 1) {
            PutStr("  Flags: LACE\n");
        }
    } else if (chunkID == ID_PREF || chunkID == ID_PRHD) {
        /* Preferences Header */
        if (chunkSize >= 6) {
            UBYTE version, type;
            ULONG flags;
            version = buffer[0];
            type = buffer[1];
            flags = (ULONG)((buffer[2] << 24) | (buffer[3] << 16) | 
                           (buffer[4] << 8) | buffer[5]);
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Version: %u, Type: %u, Flags: 0x%08lx\n", 
                     version, type, flags);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_ASL && chunkSize >= 16) {
        /* ASL Preferences */
        UBYTE sortBy, sortDrawers, sortOrder, sizePosition;
        WORD relLeft, relTop;
        UBYTE relWidth, relHeight;
        STRPTR sortByName;
        
        sortBy = buffer[16];
        sortDrawers = buffer[17];
        sortOrder = buffer[18];
        sizePosition = buffer[19];
        relLeft = (WORD)((buffer[20] << 8) | buffer[21]);
        relTop = (WORD)((buffer[22] << 8) | buffer[23]);
        relWidth = buffer[24];
        relHeight = buffer[25];
        
        switch (sortBy) {
            case 0: sortByName = "Name"; break;
            case 1: sortByName = "Date"; break;
            case 2: sortByName = "Size"; break;
            case 3: sortByName = "Type"; break;
            default: sortByName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Sort by: %s, Drawers: %s, Order: %s\n",
                 sortByName, (sortDrawers ? "First" : "Mixed"),
                 (sortOrder ? "Descending" : "Ascending"));
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Position: (%d, %d), Size: %u x %u\n",
                 (LONG)relLeft, (LONG)relTop, relWidth, relHeight);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_FONT && chunkSize >= 20) {
        /* Font Preferences */
        UWORD type;
        UBYTE frontPen, backPen, drawMode, specialDrawMode;
        STRPTR typeName;
        
        type = (UWORD)((buffer[14] << 8) | buffer[15]);
        frontPen = buffer[16];
        backPen = buffer[17];
        drawMode = buffer[18];
        specialDrawMode = buffer[19];
        
        switch (type) {
            case 0: typeName = "Workbench"; break;
            case 1: typeName = "System"; break;
            case 2: typeName = "Screen"; break;
            default: typeName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Type: %s, Pens: %u/%u, DrawMode: %u\n",
                 typeName, frontPen, backPen, drawMode);
        PutStr((STRPTR)outputBuffer);
        if (chunkSize >= 148) {
            /* Font name is at offset 20, up to 128 bytes */
            UBYTE fontName[129];
            LONG j;
            for (j = 0; j < 128 && (20 + j) < chunkSize; j++) {
                fontName[j] = buffer[20 + j];
            }
            fontName[j] = '\0';
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Font: %s\n", (STRPTR)fontName);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_ICTL && chunkSize >= 16) {
        /* Intuition Control Preferences */
        UWORD timeout;
        WORD metaDrag;
        ULONG flags;
        UBYTE wbToFront, frontToBack, reqTrue, reqFalse;
        
        timeout = (UWORD)((buffer[16] << 8) | buffer[17]);
        metaDrag = (WORD)((buffer[18] << 8) | buffer[19]);
        flags = (ULONG)((buffer[20] << 24) | (buffer[21] << 16) | 
                       (buffer[22] << 8) | buffer[23]);
        wbToFront = buffer[24];
        frontToBack = buffer[25];
        reqTrue = buffer[26];
        reqFalse = buffer[27];
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Timeout: %u, MetaDrag: %d, Flags: 0x%08lx\n",
                 timeout, (LONG)metaDrag, flags);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_IEXC && chunkSize >= 16) {
        /* Intuition Exception Preferences */
        /* Variable-length TagItem array starting at offset 16 */
        ULONG numTags;
        numTags = (chunkSize - 16) / 8; /* Each TagItem is 8 bytes */
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  %lu tag items\n", numTags);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_INPT && chunkSize >= 32) {
        /* Input Preferences */
        UWORD pointerTicks, mouseAccel;
        ULONG keyRptDelay, keyRptSpeed;
        UBYTE keymap[16];
        LONG j;
        
        /* InputPrefs has no reserved fields - starts at offset 0 */
        for (j = 0; j < 16; j++) {
            keymap[j] = buffer[j];
        }
        pointerTicks = (UWORD)((buffer[16] << 8) | buffer[17]);
        /* TimeVal structures follow at offset 18 (each TimeVal is 8 bytes) */
        /* ip_MouseAccel is at offset 16 + 2 + 8 + 8 + 8 = 42, but let's use 30 for safety */
        if (chunkSize >= 44) {
            mouseAccel = (UWORD)((buffer[42] << 8) | buffer[43]);
        } else {
            mouseAccel = 0;
        }
        
        if (chunkSize >= 44) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Pointer ticks: %u, Mouse accel: %u\n",
                     pointerTicks, mouseAccel);
            PutStr((STRPTR)outputBuffer);
        } else {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Pointer ticks: %u\n", pointerTicks);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_LCLE && chunkSize >= 20) {
        /* Locale Preferences */
        char countryName[33];
        LONG j;
        LONG gmtoffset;
        ULONG flags;
        
        for (j = 0; j < 32 && (16 + j) < chunkSize; j++) {
            countryName[j] = (char)buffer[16 + j];
        }
        countryName[j] = '\0';
        gmtoffset = (LONG)((buffer[48] << 24) | (buffer[49] << 16) | 
                          (buffer[50] << 8) | buffer[51]);
        flags = (ULONG)((buffer[52] << 24) | (buffer[53] << 16) | 
                       (buffer[54] << 8) | buffer[55]);
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Country: %s, GMT offset: %ld, Flags: 0x%08lx\n",
                 (STRPTR)countryName, gmtoffset, flags);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_CTRY && chunkSize >= 20) {
        /* Country Preferences */
        ULONG countryCode, telephoneCode;
        UBYTE measuringSystem;
        
        countryCode = (ULONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                             (buffer[18] << 8) | buffer[19]);
        telephoneCode = (ULONG)((buffer[20] << 24) | (buffer[21] << 16) | 
                                (buffer[22] << 8) | buffer[23]);
        measuringSystem = buffer[24];
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Country code: %lu, Telephone: %lu, Measure: %u\n",
                 countryCode, telephoneCode, measuringSystem);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_OSCN && chunkSize >= 28) {
        /* Overscan Preferences */
        ULONG magic;
        UWORD hStart, hStop, vStart, vStop;
        ULONG displayID;
        
        magic = (ULONG)((buffer[4] << 24) | (buffer[5] << 16) | 
                       (buffer[6] << 8) | buffer[7]);
        hStart = (UWORD)((buffer[8] << 8) | buffer[9]);
        hStop = (UWORD)((buffer[10] << 8) | buffer[11]);
        vStart = (UWORD)((buffer[12] << 8) | buffer[13]);
        vStop = (UWORD)((buffer[14] << 8) | buffer[15]);
        displayID = (ULONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                           (buffer[18] << 8) | buffer[19]);
        
        if (magic == 0xFEDCBA89UL) {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  H: %u-%u, V: %u-%u, DisplayID: 0x%08lx\n",
                     hStart, hStop, vStart, vStop, displayID);
            PutStr((STRPTR)outputBuffer);
        } else {
            PutStr("  Magic value invalid\n");
        }
    } else if (chunkID == ID_PALT && chunkSize >= 16) {
        /* Palette Preferences */
        ULONG numColors;
        numColors = (chunkSize - 16) / 6; /* Each ColorSpec is 6 bytes */
        if (numColors > 32) numColors = 32;
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  %lu color entries\n", numColors);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PNTR && chunkSize >= 20) {
        /* Pointer Preferences */
        UWORD which, size, width, height, depth, ySize;
        WORD x, y;
        STRPTR whichName;
        
        which = (UWORD)((buffer[16] << 8) | buffer[17]);
        size = (UWORD)((buffer[18] << 8) | buffer[19]);
        width = (UWORD)((buffer[20] << 8) | buffer[21]);
        height = (UWORD)((buffer[22] << 8) | buffer[23]);
        depth = (UWORD)((buffer[24] << 8) | buffer[25]);
        ySize = (UWORD)((buffer[26] << 8) | buffer[27]);
        x = (WORD)((buffer[28] << 8) | buffer[29]);
        y = (WORD)((buffer[30] << 8) | buffer[31]);
        
        whichName = (which == 0) ? "Normal" : "Busy";
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Type: %s, Size: %u x %u, Depth: %u, Hotspot: (%d, %d)\n",
                 whichName, width, height, depth, (LONG)x, (LONG)y);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PGFX && chunkSize >= 28) {
        /* Printer Graphics Preferences */
        UWORD aspect, shade, image, threshold;
        UBYTE colorCorrect, dimensions, dithering;
        UWORD graphicFlags;
        UBYTE printDensity;
        UWORD printMaxWidth, printMaxHeight;
        UBYTE printXOffset, printYOffset;
        STRPTR aspectName, shadeName, imageName;
        
        aspect = (UWORD)((buffer[16] << 8) | buffer[17]);
        shade = (UWORD)((buffer[18] << 8) | buffer[19]);
        image = (UWORD)((buffer[20] << 8) | buffer[21]);
        threshold = (WORD)((buffer[22] << 8) | buffer[23]);
        colorCorrect = buffer[24];
        dimensions = buffer[25];
        dithering = buffer[26];
        graphicFlags = (UWORD)((buffer[27] << 8) | buffer[28]);
        printDensity = buffer[29];
        printMaxWidth = (UWORD)((buffer[30] << 8) | buffer[31]);
        printMaxHeight = (UWORD)((buffer[32] << 8) | buffer[33]);
        printXOffset = buffer[34];
        printYOffset = buffer[35];
        
        aspectName = (aspect == 0) ? "Horizontal" : "Vertical";
        switch (shade) {
            case 0: shadeName = "B&W"; break;
            case 1: shadeName = "Grayscale"; break;
            case 2: shadeName = "Color"; break;
            case 3: shadeName = "Grayscale2"; break;
            default: shadeName = "Unknown"; break;
        }
        imageName = (image == 0) ? "Positive" : "Negative";
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Aspect: %s, Shade: %s, Image: %s, Density: %u\n",
                 aspectName, shadeName, imageName, printDensity);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Max size: %u x %u, Offset: (%u, %u)\n",
                 printMaxWidth, printMaxHeight, printXOffset, printYOffset);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PSPD && chunkSize >= 16) {
        /* PostScript Printer Preferences */
        UBYTE driverMode, paperFormat;
        LONG copies, paperWidth, paperHeight;
        LONG hDPI, vDPI;
        STRPTR modeName, formatName;
        
        driverMode = buffer[20];
        paperFormat = buffer[21];
        copies = (LONG)((buffer[24] << 24) | (buffer[25] << 16) | 
                       (buffer[26] << 8) | buffer[27]);
        paperWidth = (LONG)((buffer[28] << 24) | (buffer[29] << 16) | 
                           (buffer[30] << 8) | buffer[31]);
        paperHeight = (LONG)((buffer[32] << 24) | (buffer[33] << 16) | 
                            (buffer[34] << 8) | buffer[35]);
        hDPI = (LONG)((buffer[36] << 24) | (buffer[37] << 16) | 
                     (buffer[38] << 8) | buffer[39]);
        vDPI = (LONG)((buffer[40] << 24) | (buffer[41] << 16) | 
                     (buffer[42] << 8) | buffer[43]);
        
        modeName = (driverMode == 0) ? "PostScript" : "Passthrough";
        switch (paperFormat) {
            case 0: formatName = "US Letter"; break;
            case 1: formatName = "US Legal"; break;
            case 2: formatName = "A4"; break;
            case 3: formatName = "Custom"; break;
            default: formatName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Mode: %s, Format: %s, Copies: %ld\n",
                 modeName, formatName, copies);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Paper: %ld x %ld, DPI: %ld x %ld\n",
                 paperWidth, paperHeight, hDPI, vDPI);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PTXT && chunkSize >= 32) {
        /* Text Printer Preferences */
        char driver[31];
        UBYTE port;
        UWORD paperType, paperSize, paperLength;
        UWORD pitch, spacing, leftMargin, rightMargin, quality;
        STRPTR portName, paperTypeName, qualityName;
        LONG j;
        
        for (j = 0; j < 30 && (16 + j) < chunkSize; j++) {
            driver[j] = (char)buffer[16 + j];
        }
        driver[j] = '\0';
        port = buffer[46];
        paperType = (UWORD)((buffer[47] << 8) | buffer[48]);
        paperSize = (UWORD)((buffer[49] << 8) | buffer[50]);
        paperLength = (UWORD)((buffer[51] << 8) | buffer[52]);
        pitch = (UWORD)((buffer[53] << 8) | buffer[54]);
        spacing = (UWORD)((buffer[55] << 8) | buffer[56]);
        leftMargin = (UWORD)((buffer[57] << 8) | buffer[58]);
        rightMargin = (UWORD)((buffer[59] << 8) | buffer[60]);
        quality = (UWORD)((buffer[61] << 8) | buffer[62]);
        
        portName = (port == 0) ? "Parallel" : "Serial";
        paperTypeName = (paperType == 0) ? "Fanfold" : "Single";
        qualityName = (quality == 0) ? "Draft" : "Letter";
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Driver: %s, Port: %s, Type: %s, Quality: %s\n",
                 (STRPTR)driver, portName, paperTypeName, qualityName);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Paper size: %u, Length: %u lines, Margins: %u-%u\n",
                 paperSize, paperLength, leftMargin, rightMargin);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PUNT && chunkSize >= 20) {
        /* Printer Unit Preferences */
        LONG unitNum;
        ULONG openDeviceFlags;
        char deviceName[33];
        LONG j;
        
        unitNum = (LONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                        (buffer[18] << 8) | buffer[19]);
        openDeviceFlags = (ULONG)((buffer[20] << 24) | (buffer[21] << 16) | 
                                 (buffer[22] << 8) | buffer[23]);
        for (j = 0; j < 32 && (24 + j) < chunkSize; j++) {
            deviceName[j] = (char)buffer[24 + j];
        }
        deviceName[j] = '\0';
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Unit: %ld, Flags: 0x%08lx, Device: %s\n",
                 unitNum, openDeviceFlags, (STRPTR)deviceName);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_PDEV && chunkSize >= 20) {
        /* Printer Device Unit Preferences */
        LONG unitNum;
        char unitName[33];
        LONG j;
        
        unitNum = (LONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                        (buffer[18] << 8) | buffer[19]);
        for (j = 0; j < 32 && (20 + j) < chunkSize; j++) {
            unitName[j] = (char)buffer[20 + j];
        }
        unitName[j] = '\0';
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Unit: %ld, Name: %s\n", unitNum, (STRPTR)unitName);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_RACT && chunkSize >= 16) {
        /* Reaction Preferences */
        UWORD bevelType, glyphType, layoutSpacing, labelPen, labelPlace;
        BOOL prop3D, label3D, simpleRefresh, look3D;
        char fallbackName[129], labelName[129];
        LONG j;
        
        bevelType = (UWORD)((buffer[16] << 8) | buffer[17]);
        glyphType = (UWORD)((buffer[18] << 8) | buffer[19]);
        layoutSpacing = (UWORD)((buffer[20] << 8) | buffer[21]);
        prop3D = (BOOL)buffer[22];
        labelPen = (UWORD)((buffer[23] << 8) | buffer[24]);
        labelPlace = (UWORD)((buffer[25] << 8) | buffer[26]);
        label3D = (BOOL)buffer[27];
        simpleRefresh = (BOOL)buffer[28];
        look3D = (BOOL)buffer[29];
        
        /* Font names start after TextAttr structures */
        /* TextAttr is 8 bytes, so fallbackName starts at offset 16+8+8=32 */
        if (chunkSize >= 160) {
            for (j = 0; j < 128 && (32 + j) < chunkSize; j++) {
                fallbackName[j] = (char)buffer[32 + j];
            }
            fallbackName[j] = '\0';
            for (j = 0; j < 128 && (160 + j) < chunkSize; j++) {
                labelName[j] = (char)buffer[160 + j];
            }
            labelName[j] = '\0';
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Bevel: %u, Glyph: %u, Spacing: %u, LabelPen: %u\n",
                 bevelType, glyphType, layoutSpacing, labelPen);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  3D: Prop=%s Label=%s Look=%s, SimpleRefresh: %s\n",
                 (prop3D ? "Yes" : "No"), (label3D ? "Yes" : "No"),
                 (look3D ? "Yes" : "No"), (simpleRefresh ? "Yes" : "No"));
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_SCRM && chunkSize >= 24) {
        /* Screen Mode Preferences */
        ULONG displayID;
        UWORD width, height, depth, control;
        
        displayID = (ULONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                           (buffer[18] << 8) | buffer[19]);
        width = (UWORD)((buffer[20] << 8) | buffer[21]);
        height = (UWORD)((buffer[22] << 8) | buffer[23]);
        depth = (UWORD)((buffer[24] << 8) | buffer[25]);
        control = (UWORD)((buffer[26] << 8) | buffer[27]);
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  DisplayID: 0x%08lx, Size: %u x %u, Depth: %u\n",
                 displayID, width, height, depth);
        PutStr((STRPTR)outputBuffer);
        if (control & 1) {
            PutStr("  Flags: AutoScroll\n");
        }
    } else if (chunkID == ID_SERL && chunkSize >= 24) {
        /* Serial Preferences */
        /* SerialPrefs has 3 LONG reserved (12 bytes) */
        ULONG unit0Map, baudRate;
        ULONG inputBuffer, outputBuffer;
        UBYTE inputHandshake, outputHandshake;
        UBYTE parity, bitsPerChar, stopBits;
        STRPTR parityName, handshakeName;
        
        unit0Map = (ULONG)((buffer[12] << 24) | (buffer[13] << 16) | 
                          (buffer[14] << 8) | buffer[15]);
        baudRate = (ULONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                          (buffer[18] << 8) | buffer[19]);
        inputBuffer = (ULONG)((buffer[20] << 24) | (buffer[21] << 16) | 
                             (buffer[22] << 8) | buffer[23]);
        outputBuffer = (ULONG)((buffer[24] << 24) | (buffer[25] << 16) | 
                              (buffer[26] << 8) | buffer[27]);
        inputHandshake = buffer[28];
        outputHandshake = buffer[29];
        parity = buffer[30];
        bitsPerChar = buffer[31];
        stopBits = buffer[32];
        
        switch (parity) {
            case 0: parityName = "None"; break;
            case 1: parityName = "Even"; break;
            case 2: parityName = "Odd"; break;
            default: parityName = "Unknown"; break;
        }
        switch (inputHandshake) {
            case 0: handshakeName = "XON"; break;
            case 1: handshakeName = "RTS"; break;
            case 2: handshakeName = "None"; break;
            default: handshakeName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Baud: %lu, Parity: %s, Bits: %u, Stop: %u\n",
                 baudRate, parityName, bitsPerChar, stopBits);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Handshake: %s, Buffers: %lu/%lu\n",
                 handshakeName, inputBuffer, outputBuffer);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_SOND && chunkSize >= 20) {
        /* Sound Preferences */
        BOOL displayQueue, audioQueue;
        UWORD audioType, audioVolume, audioPeriod, audioDuration;
        char audioFileName[257];
        STRPTR typeName;
        LONG j;
        
        displayQueue = (BOOL)buffer[16];
        audioQueue = (BOOL)buffer[17];
        audioType = (UWORD)((buffer[18] << 8) | buffer[19]);
        audioVolume = (UWORD)((buffer[20] << 8) | buffer[21]);
        audioPeriod = (UWORD)((buffer[22] << 8) | buffer[23]);
        audioDuration = (UWORD)((buffer[24] << 8) | buffer[25]);
        
        for (j = 0; j < 256 && (26 + j) < chunkSize; j++) {
            audioFileName[j] = (char)buffer[26 + j];
        }
        audioFileName[j] = '\0';
        
        typeName = (audioType == 0) ? "Beep" : "Sample";
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Type: %s, Volume: %u, Period: %u, Duration: %u\n",
                 typeName, audioVolume, audioPeriod, audioDuration);
        PutStr((STRPTR)outputBuffer);
        if (audioFileName[0] != '\0') {
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  File: %s\n", (STRPTR)audioFileName);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_PTRN && chunkSize >= 16) {
        /* Workbench Pattern Preferences */
        UWORD which, flags;
        BYTE revision, depth;
        UWORD dataLength;
        STRPTR whichName;
        
        which = (UWORD)((buffer[16] << 8) | buffer[17]);
        flags = (UWORD)((buffer[18] << 8) | buffer[19]);
        revision = (BYTE)buffer[20];
        depth = (BYTE)buffer[21];
        dataLength = (UWORD)((buffer[22] << 8) | buffer[23]);
        
        switch (which) {
            case 0: whichName = "Root"; break;
            case 1: whichName = "Drawer"; break;
            case 2: whichName = "Screen"; break;
            default: whichName = "Unknown"; break;
        }
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Type: %s, Depth: %d, Data length: %u, Flags: 0x%04x\n",
                 whichName, (LONG)depth, dataLength, flags);
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_WBNC && chunkSize >= 16) {
        /* Workbench Preferences */
        ULONG defaultStackSize, typeRestartTime, iconPrecision;
        BOOL borderless, newIconsSupport, colorIconSupport;
        LONG maxNameLength;
        
        defaultStackSize = (ULONG)((buffer[16] << 24) | (buffer[17] << 16) | 
                                  (buffer[18] << 8) | buffer[19]);
        typeRestartTime = (ULONG)((buffer[20] << 24) | (buffer[21] << 16) | 
                                 (buffer[22] << 8) | buffer[23]);
        iconPrecision = (ULONG)((buffer[24] << 24) | (buffer[25] << 16) | 
                               (buffer[26] << 8) | buffer[27]);
        borderless = (BOOL)buffer[28];
        maxNameLength = (LONG)((buffer[29] << 24) | (buffer[30] << 16) | 
                             (buffer[31] << 8) | buffer[32]);
        newIconsSupport = (BOOL)buffer[33];
        colorIconSupport = (BOOL)buffer[34];
        
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Stack: %lu, Restart: %lu, Precision: %lu\n",
                 defaultStackSize, typeRestartTime, iconPrecision);
        PutStr((STRPTR)outputBuffer);
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                 "  Max name: %ld, Borderless: %s, NewIcons: %s, ColorIcons: %s\n",
                 maxNameLength, (borderless ? "Yes" : "No"),
                 (newIconsSupport ? "Yes" : "No"),
                 (colorIconSupport ? "Yes" : "No"));
        PutStr((STRPTR)outputBuffer);
    } else if (chunkID == ID_WBHD && chunkSize >= 16) {
        /* Workbench Hidden Device Preferences */
        /* Variable-length string starting at offset 16 */
        if (chunkSize > 16) {
            UBYTE nameBuffer[256];
            LONG j;
            LONG maxLen;
            maxLen = (chunkSize - 16 > 255) ? 255 : (chunkSize - 16);
            for (j = 0; j < maxLen; j++) {
                nameBuffer[j] = buffer[16 + j];
            }
            nameBuffer[j] = '\0';
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Device: %s\n", (STRPTR)nameBuffer);
            PutStr((STRPTR)outputBuffer);
        }
    } else if (chunkID == ID_WBTF && chunkSize >= 16) {
        /* Workbench Title Format Preferences */
        /* Variable-length string starting at offset 16 */
        if (chunkSize > 16) {
            UBYTE formatBuffer[256];
            LONG j;
            LONG maxLen;
            maxLen = (chunkSize - 16 > 255) ? 255 : (chunkSize - 16);
            for (j = 0; j < maxLen; j++) {
                formatBuffer[j] = buffer[16 + j];
            }
            formatBuffer[j] = '\0';
            SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer),
                     "  Format: %s\n", (STRPTR)formatBuffer);
            PutStr((STRPTR)outputBuffer);
        }
    }
    
    FreeMem(buffer, chunkSize);
}

/*
** PrintChunkInfo - Print chunk information (ID, size, description)
** For text chunks, also prints the text content
** For binary metadata chunks, parses and displays structured data
*/
VOID PrintChunkInfo(struct ContextNode *cn, LONG indent, struct IFFHandle *iff, ULONG formType)
{
    LONG i;
    ULONG chunkID;
    ULONG chunkSize;
    STRPTR desc;
    BOOL isTextChunk;
    BOOL isMetadataChunk;
    
    if (!cn) {
        return;
    }
    
    chunkID = cn->cn_ID;
    chunkSize = cn->cn_Size;
    
    /* Check if this is a text chunk */
    isTextChunk = (chunkID == ID_TEXT || chunkID == ID_ANNO || 
                   chunkID == ID_AUTH || chunkID == ID_COPYRIGHT ||
                   chunkID == ID_NAME || chunkID == ID_CHRS);
    
    /* Check if this is a metadata chunk we can parse */
    isMetadataChunk = (chunkID == ID_BMHD || chunkID == ID_CMAP || 
                       chunkID == ID_CAMG || chunkID == ID_GRAB ||
                       chunkID == ID_SPRT || chunkID == ID_CRNG ||
                       chunkID == ID_FXHD || chunkID == ID_DGBL ||
                       chunkID == ID_DLOC || chunkID == ID_DCHG ||
                       chunkID == ID_YCHD ||
                       chunkID == ID_PREF || chunkID == ID_PRHD ||
                       chunkID == ID_ASL || chunkID == ID_FONT ||
                       chunkID == ID_ICTL || chunkID == ID_IEXC ||
                       chunkID == ID_INPT || chunkID == ID_LCLE ||
                       chunkID == ID_CTRY || chunkID == ID_OSCN ||
                       chunkID == ID_PALT || chunkID == ID_PNTR ||
                       chunkID == ID_PGFX || chunkID == ID_PSPD ||
                       chunkID == ID_PTXT || chunkID == ID_PUNT ||
                       chunkID == ID_PDEV || chunkID == ID_RACT ||
                       chunkID == ID_SCRM || chunkID == ID_SERL ||
                       chunkID == ID_SOND || chunkID == ID_PTRN ||
                       chunkID == ID_WBNC || chunkID == ID_WBHD ||
                       chunkID == ID_WBTF);
    
    /* Print indentation */
    for (i = 0; i < indent; i++) {
        PutStr("  ");
    }
    
    /* Print chunk ID */
    {
        BPTR output;
        output = Output();
        FPutC(output, (LONG)'[');
        PrintChunkID(chunkID);
        FPutC(output, (LONG)']');
    }
    
    /* Print chunk size */
    {
        UBYTE outputBuffer[64];
        SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer), " %lu bytes", chunkSize);
        PutStr((STRPTR)outputBuffer);
    }
    
    /* Print description if available */
    desc = GetChunkDescription(chunkID);
    if (desc) {
        PutStr(" - ");
        PutStr(desc);
    }
    
    PutStr("\n");
    
    /* If this is a metadata chunk, parse and display its contents */
    /* Note: We always call PrintBinaryMetadata for metadata chunks (it reads the data) */
    /* The caller will handle quiet mode by skipping data if needed */
    if (isMetadataChunk && !isTextChunk) {
        PrintBinaryMetadata(iff, chunkID, formType, indent);
    }
}

/*
** main - Entry point for AmigaDOS command
** Returns: RETURN_OK on success, RETURN_FAIL on error
*/
int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[2]; /* FILE, QUICK */
    char inputFile[256]; /* Local copy of filename */
    struct IFFHandle *iff;
    BPTR filehandle;
    LONG error;
    struct ContextNode *cn;
    LONG indent;
    BOOL quick;
    ULONG formType;
    UBYTE outputBuffer[256];
    STRPTR desc;
    
    /* Open iffparse.library */
    IFFParseBase = OpenLibrary("iffparse.library", 0);
    if (!IFFParseBase) {
        PutStr("Error: Cannot open iffparse.library\n");
        return (int)RETURN_FAIL;
    }
    
    /* Initialize args array - ReadArgs will fill with pointers to strings */
    args[0] = 0; /* FILE */
    args[1] = 0; /* QUICK (boolean) */
    
    /* Parse command-line arguments */
    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        /* ReadArgs returns NULL on failure (e.g., missing required /A arguments) */
        PutStr((STRPTR)USAGE);
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
        return (int)RETURN_FAIL;
    }
    
    /* With /A modifier, ReadArgs ensures args are filled, but check anyway */
    if (!args[0]) {
        PutStr("Error: Missing required FILE argument\n");
        PutStr((STRPTR)USAGE);
        FreeArgs(rdargs);
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
        return (int)RETURN_FAIL;
    }
    
    /* Copy string from ReadArgs before calling FreeArgs() */
    Strncpy(inputFile, (STRPTR)args[0], sizeof(inputFile) - 1);
    inputFile[sizeof(inputFile) - 1] = '\0';
    
    /* Get switch value (non-zero if set) */
    quick = (args[1] != 0);
    
    /* Free ReadArgs memory now that we've copied the string we need */
    FreeArgs(rdargs);
    
    /* Check if input file exists */
    {
        BPTR lock;
        struct FileInfoBlock fib;
        
        lock = Lock((STRPTR)inputFile, ACCESS_READ);
        if (!lock) {
            PutStr("Error: Input file does not exist: ");
            PutStr((STRPTR)inputFile);
            PutStr("\n");
            CloseLibrary(IFFParseBase);
            IFFParseBase = NULL;
            return (int)RETURN_FAIL;
        }
        
        /* Check if it's actually a file (not a directory) */
        if (Examine(lock, &fib)) {
            if (fib.fib_DirEntryType > 0) {
                /* It's a directory, not a file */
                UnLock(lock);
                PutStr("Error: Input path is a directory, not a file: ");
                PutStr((STRPTR)inputFile);
                PutStr("\n");
                CloseLibrary(IFFParseBase);
                IFFParseBase = NULL;
                return (int)RETURN_FAIL;
            }
        }
        UnLock(lock);
    }
    
    /* Open file with DOS */
    filehandle = Open((STRPTR)inputFile, MODE_OLDFILE);
    if (!filehandle) {
        PutStr("Error: Cannot open IFF file: ");
        PutStr((STRPTR)inputFile);
        PutStr("\n");
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
        return (int)RETURN_FAIL;
    }
    
    /* Allocate IFF handle */
    iff = AllocIFF();
    if (!iff) {
        PutStr("Error: Cannot allocate IFF handle\n");
        Close(filehandle);
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
        return (int)RETURN_FAIL;
    }
    
    /* Initialize IFF handle for DOS stream */
    InitIFFasDOS(iff);
    iff->iff_Stream = (ULONG)filehandle;
    
    /* Open IFF for reading */
    error = OpenIFF(iff, IFFF_READ);
    if (error != 0) {
        PutStr("Error: Cannot open IFF stream: ");
        PutStr((STRPTR)inputFile);
        PutStr("\n");
        FreeIFF(iff);
        Close(filehandle);
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
        return (int)RETURN_FAIL;
    }
    
    /* Print header */
    if (!quick) {
        PutStr("IFF File: ");
        PutStr((STRPTR)inputFile);
        PutStr("\n");
        PutStr("========================================\n\n");
    }
    
    /* Parse IFF structure and print chunks */
    /* We don't declare any chunks - we want to see all of them */
    indent = 0;
    formType = 0;
    
    while (1) {
        /* Parse one step - this will step through all chunks */
        error = ParseIFF(iff, IFFPARSE_STEP);
        
        if (error == IFFERR_EOC) {
            /* End of context - decrease indent */
            if (indent > 0) {
                indent--;
            }
            continue;
        }
        
        if (error != 0) {
            /* Error or end of file */
            if (error != IFFERR_EOF) {
                /* Real error */
                SNPrintf((STRPTR)outputBuffer, sizeof(outputBuffer), 
                         "Error parsing IFF file: %ld\n", error);
                PutStr((STRPTR)outputBuffer);
            }
            break;
        }
        
        /* Get current chunk */
        cn = CurrentChunk(iff);
        if (!cn) {
            continue;
        }
        
        /* Check if this is a FORM chunk (start of new context) */
        if (cn->cn_ID == ID_FORM) {
            formType = cn->cn_Type;
            indent = 0;
            
            if (quick) {
                /* Quick mode - just show FORM type */
                PutStr("FORM [");
                PrintChunkID(formType);
                PutStr("]\n");
            } else {
                /* Normal mode - show FORM type and description */
                PutStr("FORM [");
                PrintChunkID(formType);
                PutStr("]\n");
                
                desc = GetChunkDescription(formType);
                if (desc) {
                    PutStr("  ");
                    PutStr(desc);
                    PutStr("\n");
                }
                PutStr("\n");
            }
            
            /* Increase indent for nested chunks */
            indent++;
            continue;
        }
        
        /* Print chunk information */
        if (quick) {
            /* Quick mode - just show chunk ID */
            LONG i;
            for (i = 0; i < indent; i++) {
                PutStr("  ");
            }
            {
                BPTR output;
                output = Output();
                FPutC(output, (LONG)'[');
                PrintChunkID(cn->cn_ID);
                FPutC(output, (LONG)']');
            }
            PutStr("\n");
        } else {
            /* Normal mode - show full information */
            PrintChunkInfo(cn, indent, iff, formType);
        }
        
        /* Read chunk data - we must read it to advance the parser */
        /* Check what type of chunk this is */
        {
            BOOL isTextChunk;
            BOOL isMetadataChunk;
            
            isTextChunk = (cn->cn_ID == ID_TEXT || cn->cn_ID == ID_ANNO || 
                           cn->cn_ID == ID_AUTH || cn->cn_ID == ID_COPYRIGHT ||
                           cn->cn_ID == ID_NAME || cn->cn_ID == ID_CHRS);
            
            isMetadataChunk = (cn->cn_ID == ID_BMHD || cn->cn_ID == ID_CMAP || 
                               cn->cn_ID == ID_CAMG || cn->cn_ID == ID_GRAB ||
                               cn->cn_ID == ID_SPRT || cn->cn_ID == ID_CRNG ||
                               cn->cn_ID == ID_FXHD || cn->cn_ID == ID_DGBL ||
                               cn->cn_ID == ID_DPEL || cn->cn_ID == ID_DLOC ||
                               cn->cn_ID == ID_DCHG || cn->cn_ID == ID_YCHD ||
                               cn->cn_ID == ID_PREF || cn->cn_ID == ID_PRHD ||
                               cn->cn_ID == ID_ASL || cn->cn_ID == ID_FONT ||
                               cn->cn_ID == ID_ICTL || cn->cn_ID == ID_IEXC ||
                               cn->cn_ID == ID_INPT || cn->cn_ID == ID_LCLE ||
                               cn->cn_ID == ID_CTRY || cn->cn_ID == ID_OSCN ||
                               cn->cn_ID == ID_PALT || cn->cn_ID == ID_PNTR ||
                               cn->cn_ID == ID_PGFX || cn->cn_ID == ID_PSPD ||
                               cn->cn_ID == ID_PTXT || cn->cn_ID == ID_PUNT ||
                               cn->cn_ID == ID_PDEV || cn->cn_ID == ID_RACT ||
                               cn->cn_ID == ID_SCRM || cn->cn_ID == ID_SERL ||
                               cn->cn_ID == ID_SOND || cn->cn_ID == ID_PTRN ||
                               cn->cn_ID == ID_WBNC || cn->cn_ID == ID_WBHD ||
                               cn->cn_ID == ID_WBTF);
            
            if (isTextChunk) {
                /* Text chunk - read and print the text */
                if (!quick) {
                    PrintTextChunk(iff, cn->cn_ID, indent + 1);
                } else {
                    /* In quick mode, skip the chunk data */
                    {
                        UBYTE *dummyBuffer;
                        LONG bytesToSkip;
                        LONG bytesRead;
                        
                        bytesToSkip = cn->cn_Size;
                        if (bytesToSkip > 0) {
                            dummyBuffer = (UBYTE *)AllocMem(1024, MEMF_PUBLIC);
                            if (dummyBuffer) {
                                while (bytesToSkip > 0) {
                                    LONG toRead;
                                    toRead = (bytesToSkip > 1024) ? 1024 : bytesToSkip;
                                    bytesRead = ReadChunkBytes(iff, dummyBuffer, toRead);
                                    if (bytesRead <= 0) {
                                        break;
                                    }
                                    bytesToSkip -= bytesRead;
                                }
                                FreeMem(dummyBuffer, 1024);
                            }
                        }
                    }
                }
            } else if (isMetadataChunk) {
                /* Metadata chunk - PrintBinaryMetadata reads the data if !quick */
                /* In quick mode, we need to skip the data */
                if (quick) {
                    UBYTE *dummyBuffer;
                    LONG bytesToSkip;
                    LONG bytesRead;
                    
                    bytesToSkip = cn->cn_Size;
                    if (bytesToSkip > 0) {
                        dummyBuffer = (UBYTE *)AllocMem(1024, MEMF_PUBLIC);
                        if (dummyBuffer) {
                            while (bytesToSkip > 0) {
                                LONG toRead;
                                toRead = (bytesToSkip > 1024) ? 1024 : bytesToSkip;
                                bytesRead = ReadChunkBytes(iff, dummyBuffer, toRead);
                                if (bytesRead <= 0) {
                                    break;
                                }
                                bytesToSkip -= bytesRead;
                            }
                            FreeMem(dummyBuffer, 1024);
                        }
                    }
                }
                /* If !quick, PrintBinaryMetadata already read the data */
            } else {
                /* Data chunk or unknown - skip the data */
                {
                    UBYTE *dummyBuffer;
                    LONG bytesToSkip;
                    LONG bytesRead;
                    
                    bytesToSkip = cn->cn_Size;
                    if (bytesToSkip > 0) {
                        dummyBuffer = (UBYTE *)AllocMem(1024, MEMF_PUBLIC);
                        if (dummyBuffer) {
                            while (bytesToSkip > 0) {
                                LONG toRead;
                                toRead = (bytesToSkip > 1024) ? 1024 : bytesToSkip;
                                bytesRead = ReadChunkBytes(iff, dummyBuffer, toRead);
                                if (bytesRead <= 0) {
                                    break;
                                }
                                bytesToSkip -= bytesRead;
                            }
                            FreeMem(dummyBuffer, 1024);
                        }
                    }
                }
            }
        }
        
        /* Check if this chunk starts a new context (like LIST, CAT, PROP) */
        if (cn->cn_ID == ID_LIST || cn->cn_ID == ID_CAT || cn->cn_ID == ID_PROP) {
            indent++;
        }
    }
    
    /* Close IFF context */
    CloseIFF(iff);
    
    /* Close file handle */
    Close(filehandle);
    
    /* Free IFF handle */
    FreeIFF(iff);
    
    /* Close iffparse.library */
    if (IFFParseBase) {
        CloseLibrary(IFFParseBase);
        IFFParseBase = NULL;
    }
    
    return (int)RETURN_OK;
}

