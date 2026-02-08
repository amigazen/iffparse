/*
** iff.h - IFF-85 chunk and group definitions for iffar
** Based on public domain IFF code by Jerry Morrison, Steve Shaw (EA), Karl Lehenbauer.
** C89 compliant.
*/

#ifndef IFFAR_IFF_H
#define IFFAR_IFF_H

#include <exec/types.h>

typedef LONG ID;

#define MakeID(a,b,c,d)  ((LONG)(a)<<24L | (LONG)(b)<<16L | (LONG)(c)<<8 | (d))

#define ID_FORM   MakeID('F','O','R','M')
#define ID_PROP   MakeID('P','R','O','P')
#define ID_LIST   MakeID('L','I','S','T')
#define ID_CAT    MakeID('C','A','T',' ')
#define ID_FILLER MakeID(' ',' ',' ',' ')
#define ID_FNAM   MakeID('F','N','A','M')
#define ID_MISC   MakeID('M','I','S','C')

typedef struct {
    ID   ckID;
    LONG ckSize;
} ChunkHeader;

typedef struct {
    ID    ckID;
    LONG  ckSize;
    ID    grpSubID;
} GroupHeader;

#define MAXCHUNKSIZE 900000L

#endif
