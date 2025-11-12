/* string.h - BDS-style drop-in (K&R) */
#ifndef BDS_STRING_H
#define BDS_STRING_H

#define memcpy memcpy_bds
#define memmove memmove_bds
#define memset memset_bds
#define memcmp memcmp_bds
#define memchr memchr_bds
#define strlen strlen_bds
#define strcpy strcpy_bds
#define strncpy strncpy_bds
#define strcat strcat_bds
#define strncat strncat_bds
#define strcmp strcmp_bds
#define strncmp strncmp_bds
#define strchr strchr_bds
#define strrchr strrchr_bds
#define strstr strstr_bds

char *memcpy_bds(dest, src, n);
char *memmove_bds(dest, src, n);
char *memset_bds(s, c, n);
int memcmp_bds(s1, s2, n);
char *memchr_bds(s, c, n);
unsigned strlen_bds(s);
char *strcpy_bds(dest, src);
char *strncpy_bds(dest, src, n);
char *strcat_bds(dest, src);
char *strncat_bds(dest, src, n);
int strcmp_bds(s1, s2);
int strncmp_bds(s1, s2, n);
char *strchr_bds(s, c);
char *strrchr_bds(s, c);
char *strstr_bds(haystack, needle);

#endif