#include "string.h"

char *memcpy_bds(dest, src, n)
register char *dest;
register char *src;
unsigned n;
{
    char *orig;

    orig = dest;

    while (n--)
        *dest++ = *src++;

    return orig;
}

char *memmove_bds(dest, src, n)
register char *dest;
register char *src;
unsigned n;
{
    char *orig;

    orig = dest;

    if (dest == src || n == 0)
        return orig;

    if (dest < src)
    {
        while (n--)
            *dest++ = *src++;
    }
    else
    {
        dest += n;
        src += n;
        while (n--)
            *--dest = *--src;
    }

    return orig;
}

char *memset_bds(s, c, n)
register char *s;
char c;
unsigned n;
{
    char *orig;

    orig = s;

    while (n--)
        *s++ = c;

    return orig;
}

int memcmp_bds(s1, s2, n)
register char *s1;
register char *s2;
unsigned n;
{
    register int c1;
    register int c2;

    if (n == 0)
        return 0;

    while (n--)
    {
        c1 = *s1++ & 0xFF;
        c2 = *s2++ & 0xFF;

        if (c1 != c2)
            return c1 - c2;
    }

    return 0;
}

char *memchr_bds(s, c, n)
register char *s;
int c;
unsigned n;
{
    register int target;

    target = c & 0xFF;

    while (n--)
    {
        if ((*s & 0xFF) == target)
            return s;
        s++;
    }

    return 0;
}

unsigned strlen_bds(s)
register char *s;
{
    register char *p;

    p = s;

    while (*p)
        p++;

    return p - s;
}

char *strcpy_bds(dest, src)
register char *dest;
register char *src;
{
    char *orig;

    orig = dest;

    while ((*dest++ = *src++) != '\0')
        ;

    return orig;
}

char *strncpy_bds(dest, src, n)
register char *dest;
register char *src;
unsigned n;
{
    char *orig;

    orig = dest;

    while (n && (*dest++ = *src++) != '\0')
        n--;

    if (n)
    {
        dest--;
        while (n--)
            *dest++ = '\0';
    }

    return orig;
}

char *strcat_bds(dest, src)
register char *dest;
register char *src;
{
    char *orig;

    orig = dest;

    while (*dest)
        dest++;

    while ((*dest = *src))
    {
        dest++;
        src++;
    }

    return orig;
}

char *strncat_bds(dest, src, n)
register char *dest;
register char *src;
unsigned n;
{
    char *orig;

    orig = dest;

    while (*dest)
        dest++;

    while (n)
    {
        if (!(*dest = *src))
            return orig;
        dest++;
        src++;
        n--;
    }

    *dest = '\0';

    return orig;
}

int strcmp_bds(s1, s2)
register char *s1;
register char *s2;
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }

    return (*s1 & 0xFF) - (*s2 & 0xFF);
}

int strncmp_bds(s1, s2, n)
register char *s1;
register char *s2;
unsigned n;
{
    register int c1;
    register int c2;

    if (n == 0)
        return 0;

    while (n--)
    {
        c1 = *s1++ & 0xFF;
        c2 = *s2++ & 0xFF;

        if (c1 != c2)
            return c1 - c2;
        if (c1 == '\0')
            break;
    }

    return 0;
}

char *strchr_bds(s, c)
register char *s;
char c;
{
    while (*s)
    {
        if (*s == c)
            return s;
        s++;
    }

    if (c == '\0')
        return s;

    return 0;
}

char *strrchr_bds(s, c)
register char *s;
char c;
{
    char *last;

    last = 0;

    do
    {
        if (*s == c)
            last = s;
    } while (*s++);

    return last;
}

char *strstr_bds(haystack, needle)
register char *haystack;
register char *needle;
{
    char *h;
    char *n;

    if (*needle == '\0')
        return haystack;

    while (*haystack)
    {
        if (*haystack == *needle)
        {
            h = haystack;
            n = needle;
            while (*n && *h == *n)
            {
                h++;
                n++;
            }
            if (*n == '\0')
                return haystack;
        }
        haystack++;
    }

    return 0;
}
