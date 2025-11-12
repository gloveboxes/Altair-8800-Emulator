#include "stdio.h"
#include "string.h"
#include "dxsys.h"

char *alloc();
int free();
unsigned x_rand();

int t_tot;
int t_fail;

int eq_buf(a, b, n)
char *a;
char *b;
int n;
{
    while (n--)
    {
        if (*a++ != *b++)
            return 0;
    }
    return 1;
}

int eq_str(a, b)
char *a;
char *b;
{
    while (*a || *b)
    {
        if (*a++ != *b++)
            return 0;
    }
    return 1;
}

int t_res(name, ok)
char *name;
int ok;
{
    t_tot++;
    if (ok)
    {
        printf("PASS %s\n", name);
    }
    else
    {
        printf("FAIL %s\n", name);
        t_fail++;
    }
    return 0;
}

int t_mc()
{
    char *src;
    char *dst;
    char *ret;
    int i;
    int ok;

    src = alloc(16);
    dst = alloc(16);
    if (!src || !dst)
    {
        t_res("alloc1", 0);
        return 0;
    }

    for (i = 0; i < 10; i++)
        src[i] = 'A' + i;
    src[10] = '\0';

    for (i = 0; i < 16; i++)
        dst[i] = '#';

    ret = memcpy_bds(dst, src, 11);
    t_res("memcp1", ret == dst);
    t_res("memcp2", eq_buf(dst, src, 11));

    for (i = 0; i < 16; i++)
        dst[i] = 0x7E;
    ret = memcpy_bds(dst, src, 0);
    t_res("memcp3", ret == dst && dst[0] == 0x7E);

    for (i = 0; i < 10; i++)
        src[i] = 0x80 + i;
    memcpy_bds(dst, src, 10);
    ok = 1;
    for (i = 0; i < 10; i++)
        if ((dst[i] & 0xFF) != (0x80 + i))
            ok = 0;
    t_res("memcp4", ok);

    for (i = 0; i < 5; i++)
        src[i] = 'X';
    src[5] = '\0';
    memcpy_bds(dst + 5, src, 6);
    t_res("memcp5", dst[5] == 'X' && dst[9] == 'X' && dst[10] == '\0');

    free(dst);
    free(src);
    return 0;
}

int t_mm()
{
    char *buf;
    int i;
    int ok;

    buf = alloc(16);
    if (!buf)
    {
        t_res("alloc2", 0);
        return 0;
    }

    for (i = 0; i < 10; i++)
        buf[i] = '0' + i;

    memmove_bds(buf + 2, buf, 8);
    ok = 1;
    for (i = 0; i < 2; i++)
        if (buf[i] != ('0' + i))
            ok = 0;
    for (i = 0; i < 8; i++)
        if (buf[i + 2] != ('0' + i))
            ok = 0;
    t_res("memmv1", ok);

    for (i = 0; i < 10; i++)
        buf[i] = '0' + i;

    memmove_bds(buf, buf + 2, 8);
    ok = 1;
    for (i = 0; i < 8; i++)
        if (buf[i] != ('0' + i + 2))
            ok = 0;
    if (buf[8] != '8' || buf[9] != '9')
        ok = 0;
    t_res("memmv2", ok);

    for (i = 0; i < 10; i++)
        buf[i] = '0' + i;
    memmove_bds(buf, buf, 10);
    ok = 1;
    for (i = 0; i < 10; i++)
        if (buf[i] != ('0' + i))
            ok = 0;
    t_res("memmv3", ok);

    for (i = 0; i < 5; i++)
        buf[i] = 'M';
    memmove_bds(buf + 1, buf, 5);
    ok = 1;
    if (buf[0] != 'M')
        ok = 0;
    for (i = 1; i < 6; i++)
        if (buf[i] != 'M')
            ok = 0;
    t_res("memmv4", ok);

    for (i = 0; i < 10; i++)
        buf[i] = 'a' + i;
    memmove_bds(buf, buf + 5, 5);
    ok = 1;
    for (i = 0; i < 5; i++)
        if (buf[i] != ('a' + i + 5))
            ok = 0;
    t_res("memmv5", ok);

    free(buf);
    return 0;
}

int t_ms()
{
    char *buf;
    int i;
    int ok;

    buf = alloc(12);
    if (!buf)
    {
        t_res("alloc3", 0);
        return 0;
    }

    for (i = 0; i < 12; i++)
        buf[i] = 'x';

    memset_bds(buf, 'Q', 8);

    for (i = 0; i < 8; i++)
        if (buf[i] != 'Q')
            break;
    t_res("memst1", i == 8);
    t_res("memst2", buf[9] == 'x');

    memset_bds(buf, 'R', 0);
    t_res("memst3", buf[0] == 'Q');

    for (i = 0; i < 12; i++)
        buf[i] = 'z';
    memset_bds(buf, '\0', 5);
    ok = 1;
    for (i = 0; i < 5; i++)
        if (buf[i] != '\0')
            ok = 0;
    t_res("memst4", ok && buf[5] == 'z');

    for (i = 0; i < 12; i++)
        buf[i] = i;
    memset_bds(buf, 0xFF, 8);
    ok = 1;
    for (i = 0; i < 8; i++)
        if ((buf[i] & 0xFF) != 0xFF)
            ok = 0;
    t_res("memst5", ok);

    free(buf);
    return 0;
}

int t_mcmp()
{
    char a1[5];
    char a2[5];
    int r;

    a1[0] = 'a'; a1[1] = 'b'; a1[2] = 'c'; a1[3] = 'd'; a1[4] = 0;
    a2[0] = 'a'; a2[1] = 'b'; a2[2] = 'c'; a2[3] = 'd'; a2[4] = 0;

    r = memcmp_bds(a1, a2, 4);
    t_res("mmcmp1", r == 0);

    a2[3] = 'e';
    r = memcmp_bds(a1, a2, 4);
    t_res("mmcmp2", r < 0);

    a1[0] = 0xF0;
    a2[0] = 0x10;
    r = memcmp_bds(a1, a2, 1);
    t_res("mmcmp3", r > 0);
    r = memcmp_bds(a1, a2, 0);
    t_res("mmcmp4", r == 0);

    a1[0] = 'A'; a1[1] = 'B'; a1[2] = 'C';
    a2[0] = 'A'; a2[1] = 'B'; a2[2] = 'c';
    r = memcmp_bds(a1, a2, 3);
    t_res("mmcmp5", r < 0);

    a1[0] = '\0'; a1[1] = 'x';
    a2[0] = '\0'; a2[1] = 'y';
    r = memcmp_bds(a1, a2, 2);
    t_res("mmcmp6", r < 0);

    return 0;
}

int t_mchr()
{
    char txt[8];
    char *p;

    txt[0] = 'h'; txt[1] = 'e'; txt[2] = 'l'; txt[3] = 'l';
    txt[4] = 'o'; txt[5] = '\0'; txt[6] = 'x'; txt[7] = 'x';

    p = memchr_bds(txt, 'l', 5);
    t_res("mmchr1", p == &txt[2]);

    p = memchr_bds(txt, 'z', 5);
    t_res("mmchr2", p == 0);
    p = memchr_bds(txt, '\0', 6);
    t_res("mmchr3", p == &txt[5]);

    txt[0] = 'a'; txt[1] = 'a'; txt[2] = 'a'; txt[3] = 'a'; txt[4] = '\0';
    p = memchr_bds(txt, 'a', 1);
    t_res("mmchr4", p == &txt[0]);

    p = memchr_bds(txt, 'x', 0);
    t_res("mmchr5", p == 0);

    return 0;
}

int t_lens()
{
    char txt[12];
    unsigned len;

    txt[0] = 'O'; txt[1] = 'p'; txt[2] = 'e'; txt[3] = 'n';
    txt[4] = 'A'; txt[5] = 'I'; txt[6] = '\0';

    len = strlen_bds(txt);
    t_res("strlen", len == 6);
    txt[0] = '\0';
    len = strlen_bds(txt);
    t_res("strln2", len == 0);

    txt[0] = 'A'; txt[1] = 'B'; txt[2] = 'C'; txt[3] = 'D'; txt[4] = 'E';
    txt[5] = 'F'; txt[6] = 'G'; txt[7] = 'H'; txt[8] = '\0';
    len = strlen_bds(txt);
    t_res("strln3", len == 8);

    return 0;
}

int t_cpy()
{
    char src[12];
    char dst[12];
    char *ret;

    src[0] = 't'; src[1] = 'e'; src[2] = 's'; src[3] = 't'; src[4] = '\0';

    ret = strcpy_bds(dst, src);
    t_res("strcp1", ret == dst);
    t_res("strcp2", eq_str(dst, src));
    src[0] = '\0';
    ret = strcpy_bds(dst, src);
    t_res("strcp3", dst[0] == '\0');

    src[0] = 'L'; src[1] = 'o'; src[2] = 'n'; src[3] = 'g'; src[4] = 'e';
    src[5] = 'r'; src[6] = '\0';
    ret = strcpy_bds(dst, src);
    t_res("strcp4", eq_str(dst, "Longer"));

    return 0;
}

int t_ncpy()
{
    char src[6];
    char dst[10];

    src[0] = 'c'; src[1] = 'o'; src[2] = 'p'; src[3] = 'e'; src[4] = '\0';

    strncpy_bds(dst, src, 6);
    t_res("strnp1", dst[4] == '\0');
    t_res("strnp2", dst[5] == '\0');

    dst[0] = 'x'; dst[1] = 'x'; dst[2] = 'x'; dst[3] = 'x'; dst[4] = '\0';
    strncpy_bds(dst, src, 2);
    t_res("strnp3", dst[2] == 'x');
    dst[0] = 'y'; dst[1] = '\0';
    strncpy_bds(dst, src, 0);
    t_res("strnp4", dst[0] == 'y');

    src[0] = 'X'; src[1] = 'Y'; src[2] = 'Z'; src[3] = '\0';
    strncpy_bds(dst, src, 10);
    t_res("strnp5", dst[3] == '\0' && dst[9] == '\0');

    return 0;
}

int t_cat()
{
    char buf[16];

    buf[0] = 'h'; buf[1] = 'i'; buf[2] = '\0';
    strcat_bds(buf, " there");
    t_res("strct1", eq_str(buf, "hi there"));
    strcat_bds(buf, "");
    t_res("strct2", eq_str(buf, "hi there"));

    buf[0] = '\0';
    strcat_bds(buf, "new");
    t_res("strct3", eq_str(buf, "new"));

    return 0;
}

int t_ncat()
{
    char buf[16];

    buf[0] = 'a'; buf[1] = 0;
    strncat_bds(buf, "bcd", 2);
    t_res("strnc1", eq_str(buf, "abc"));
    strncat_bds(buf, "def", 5);
    t_res("strnc2", eq_str(buf, "abcdef"));

    buf[0] = 'X'; buf[1] = '\0';
    strncat_bds(buf, "YZ", 1);
    t_res("strnc3", eq_str(buf, "XY"));

    return 0;
}

int t_cmp()
{
    int r;

    r = strcmp_bds("abc", "abc");
    t_res("strcmp", r == 0);

    r = strcmp_bds("abc", "abd");
    t_res("strcm2", r < 0);

    r = strcmp_bds("abe", "abd");
    t_res("strcm3", r > 0);
    r = strcmp_bds("", "");
    t_res("strcm4", r == 0);

    r = strcmp_bds("a", "");
    t_res("strcm5", r > 0);

    r = strcmp_bds("", "a");
    t_res("strcm6", r < 0);

    return 0;
}

int t_ncmp()
{
    int r;

    r = strncmp_bds("alpha", "alphabet", 5);
    t_res("strnm1", r == 0);

    r = strncmp_bds("alpha", "alphi", 5);
    t_res("strnm2", r < 0);

    r = strncmp_bds("alpha", "alpg", 4);
    t_res("strnm3", r > 0);
    r = strncmp_bds("abc", "xyz", 0);
    t_res("strnm4", r == 0);

    r = strncmp_bds("abcd", "abc", 3);
    t_res("strnm5", r == 0);

    r = strncmp_bds("", "", 10);
    t_res("strnm6", r == 0);

    return 0;
}

int t_chr()
{
    char *p;

    p = strchr_bds("hello", 'l');
    t_res("strch1", p && p[0] == 'l' && p[1] == 'l');

    p = strchr_bds("hello", '\0');
    t_res("strch2", p && *p == '\0');
    p = strchr_bds("hello", 'z');
    t_res("strch3", p == 0);

    p = strchr_bds("hello", 'h');
    t_res("strch4", p && p[0] == 'h');

    p = strchr_bds("", 'a');
    t_res("strch5", p == 0);

    return 0;
}

int t_rchr()
{
    char text[12];
    char *p;

    text[0] = 'b'; text[1] = 'a'; text[2] = 'n'; text[3] = 'a'; text[4] = 'n'; text[5] = 'a'; text[6] = '\0';

    p = strrchr_bds(text, 'a');
    t_res("strrh1", p == &text[5]);

    p = strrchr_bds(text, 'z');
    t_res("strrh2", p == 0);
    p = strrchr_bds(text, '\0');
    t_res("strrh3", p == &text[6]);

    text[0] = 'x'; text[1] = 'y'; text[2] = 'z'; text[3] = '\0';
    p = strrchr_bds(text, 'y');
    t_res("strrh4", p == &text[1]);

    p = strrchr_bds("", 'a');
    t_res("strrh5", p == 0);

    return 0;
}

int t_strs()
{
    char *p;
    char *hay;

    p = strstr_bds("strawberry", "berry");
    t_res("strst1", p && eq_str(p, "berry"));

    p = strstr_bds("strawberry", "pear");
    t_res("strst2", p == 0);

    hay = "abc";
    p = strstr_bds(hay, "");
    t_res("strst3", p == hay);
    p = strstr_bds("aaaa", "aa");
    t_res("strst4", p && p[0] == 'a' && p[1] == 'a');

    p = strstr_bds("abc", "abcd");
    t_res("strst5", p == 0);

    p = strstr_bds("test string", "str");
    t_res("strst6", p && eq_str(p, "string"));

    p = strstr_bds("", "x");
    t_res("strst7", p == 0);

    return 0;
}

int t_fuzz()
{
    char *buf1;
    char *buf2;
    unsigned seed;
    int i;
    int len;
    int ok;
    int iter;

    buf1 = alloc(128);
    buf2 = alloc(128);
    if (!buf1 || !buf2)
    {
        t_res("alc_fz", 0);
        return 0;
    }

    for (iter = 0; iter < 10; iter++)
    {
        seed = x_rand();
        len = (seed % 64) + 1;

        for (i = 0; i < len; i++)
        {
            seed = seed * 1103515245 + 12345;
            buf1[i] = (seed >> 16) & 0xFF;
        }
        buf1[len] = '\0';

        memcpy_bds(buf2, buf1, len + 1);
        ok = eq_buf(buf1, buf2, len + 1);
        if (!ok)
        {
            t_res("fuz_mc", 0);
            free(buf2);
            free(buf1);
            return 0;
        }
    }
    t_res("fuz_mc", 1);

    for (iter = 0; iter < 10; iter++)
    {
        seed = x_rand();
        len = (seed % 60) + 1;

        for (i = 0; i < len; i++)
        {
            seed = seed * 1103515245 + 12345;
            buf1[i] = ((seed >> 16) & 0x7F) | 0x01;
        }
        buf1[len] = '\0';

        strcpy_bds(buf2, buf1);
        ok = eq_str(buf1, buf2);
        if (!ok)
        {
            t_res("fuz_sc", 0);
            free(buf2);
            free(buf1);
            return 0;
        }
    }
    t_res("fuz_sc", 1);

    for (iter = 0; iter < 10; iter++)
    {
        seed = x_rand();
        len = (seed % 64) + 1;

        for (i = 0; i < len; i++)
        {
            seed = seed * 1103515245 + 12345;
            buf1[i] = (seed >> 16) & 0xFF;
        }

        seed = x_rand();
        memset_bds(buf2, (seed >> 8) & 0xFF, len);

        ok = 1;
        for (i = 0; i < len; i++)
            if (buf2[i] != ((seed >> 8) & 0xFF))
                ok = 0;
        if (!ok)
        {
            t_res("fuz_ms", 0);
            free(buf2);
            free(buf1);
            return 0;
        }
    }
    t_res("fuz_ms", 1);

    free(buf2);
    free(buf1);
    return 0;
}

int t_edge()
{
    char *big;
    char *p;
    int i;
    int ok;

    big = alloc(256);
    if (!big)
    {
        t_res("alc_eg", 0);
        return 0;
    }

    for (i = 0; i < 255; i++)
        big[i] = 'X';
    big[255] = '\0';

    ok = (strlen_bds(big) == 255);
    t_res("edg_ln", ok);

    for (i = 0; i < 255; i++)
        big[i] = 'Y';
    big[128] = '\0';
    p = strchr_bds(big, '\0');
    t_res("edg_ch", p == &big[128]);

    for (i = 0; i < 100; i++)
        big[i] = 'A';
    big[100] = '\0';
    for (i = 101; i < 200; i++)
        big[i] = 'B';
    big[200] = '\0';
    ok = (strcmp_bds(big, big + 101) < 0);
    t_res("edg_cm", ok);

    free(big);
    return 0;
}

int t_bound()
{
    char small[4];
    char tiny[2];
    char *ret;

    small[0] = 'a'; small[1] = 'b'; small[2] = 'c'; small[3] = '\0';
    tiny[0] = 'x'; tiny[1] = '\0';

    ret = memcpy_bds(tiny, small, 1);
    t_res("bnd_m1", tiny[0] == 'a' && tiny[1] == '\0');

    memset_bds(tiny, 'Z', 1);
    t_res("bnd_m2", tiny[0] == 'Z' && tiny[1] == '\0');

    tiny[0] = 'Q'; tiny[1] = '\0';
    ret = strcpy_bds(tiny, "R");
    t_res("bnd_sc", tiny[0] == 'R' && tiny[1] == '\0');

    return 0;
}

int main()
{
    t_tot = 0;
    t_fail = 0;

    t_mc();
    t_mm();
    t_ms();
    t_mcmp();
    t_mchr();
    t_lens();
    t_cpy();
    t_ncpy();
    t_cat();
    t_ncat();
    t_cmp();
    t_ncmp();
    t_chr();
    t_rchr();
    t_strs();
    t_fuzz();
    t_edge();
    t_bound();

    if (t_fail)
    {
        printf("FAIL %d of %d\n", t_fail, t_tot);
        return 1;
    }

    printf("PASS %d tests\n", t_tot);
    return 0;
}
