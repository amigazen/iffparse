/*
** iffar - miscellaneous (basename, strnicmp)
** Based on public domain code by Karl Lehenbauer (1988).
** C89 compliant.
*/

#include <ctype.h>
#include <string.h>

/* Case-insensitive strncmp; returns 0 when equal, non-zero otherwise. Named to avoid conflict with library. */
int iffar_strnicmp(char *s1, char *s2, int len)
{
    char c1;
    char c2;

    c1 = *s1;
    c2 = *s2;
    while (len-- > 0 && *s1) {
        c1 = (char)(isupper((unsigned char)*s1) ? tolower((unsigned char)*s1) : *s1);
        s1++;
        c2 = (char)(isupper((unsigned char)*s2) ? tolower((unsigned char)*s2) : *s2);
        s2++;
        if (c1 != c2) {
            break;
        }
    }
    return (int)((unsigned char)c1 - (unsigned char)c2);
}

/* Return pointer to base portion of path (last '/' or ':' component). */
char *basename(char *fname)
{
    char *basename_ptr;
    int i;
    int fnamelen;

    fnamelen = (int)strlen(fname);
    basename_ptr = fname;
    for (i = fnamelen - 1; i > 0; i--) {
        if (fname[i] == '/' || fname[i] == ':') {
            basename_ptr = &fname[i + 1];
            break;
        }
    }
    return basename_ptr;
}
