/* ============================================================
 * LLM RULES FOR GENERATING BDS C CODE (Altair 8800 / CP/M)
 * ============================================================
 *
 * 1. Syntax:
 *    - Use K&R (BDS C) style: return_type name(args) on next line
 *    - No ANSI prototypes, no "void", no modern keywords
 *    - All function definitions and calls must follow BDS C rules
 *
 * 2. Symbols (VERY IMPORTANT):
 *    - All symbol names (functions, variables, labels, statics, globals)
 *      must be unique in their first 7 characters
 *    - Prefer short, descriptive names, e.g. "x_delay", "x_tmrset"
 *    - Avoid underscores beyond the leading "x_" unless necessary
 *    - Do not exceed 7 characters for clarity and linker safety
 *
 * 3. Types:
 *    - Use int or unsigned (16-bit) for parameters and locals
 *    - Use long.c for longs
 *    - Explicitly declare return type (no implicit int)
 *
 *
 * 6. Style:
 *    - Add a short comment block before each function
 *    - Keep indentation simple (max 4 spaces)
 *    - No C99/C89 features (stick to 1980-era BDS C)
 *
 * 7. The app runs on CP/M single tasking OS, only one app runs at a time
 * ============================================================
 */

#include "dxsys.h"

#define ALTR_PT 70
#define UPTIME_PT 41
#define UTC_PT 42
#define LOCAL_PT 43
#define LOAD_PT 200
#define SENSE_PT 63
#define WKEY_PT 34
#define WVAL_PT 35
#define LKEY_PT 36
#define LVAL_PT 37
#define PKEY_PT 38
#define PVAL_PT 39

/* BDS C I/O entry points */
int inp(); /* int inp(port) */
outp();    /* void outp(port,val) */

int x_loader(buf, size);

unsigned x_rand() /* Get random number */
{
    unsigned r;
    outp(45, 1);          /* generate random number */
    r = inp(200);         /* read low byte */
    r |= (inp(200) << 8); /* read high byte */
    return r;
}

int x_loader(buf, size)
char *buf;
int size;
{
    int i;
    for (i = 0; i < size - 1; i++)
    {
        buf[i] = inp(LOAD_PT);
        if (buf[i] == 0)
            break;
    }
    buf[i] = 0;
    return i;
}

/* get Altair 8800 Emulator Version */
/* Pass in a buffer and its size */
int x_altr(buf, size)
char *buf;
int size;
{
    outp(ALTR_PT, 1);
    return x_loader(buf, size);
}

/* Get system uptime */
/* Pass in a buffer and its size */
int x_uptime(buf, size)
char *buf;
int size;
{
    outp(UPTIME_PT, 1);
    return x_loader(buf, size);
}

/* Get current UTC time */
/* Pass in a buffer and its size */
int x_cur_utc(buf, size)
char *buf;
int size;
{
    outp(UTC_PT, 1);
    return x_loader(buf, size);
}

/* Get current local time */
/* Pass in a buffer and its size */
int x_local(buf, size)
char *buf;
int size;
{
    outp(LOCAL_PT, 1);
    return x_loader(buf, size);
}

/* Get PI Sense HAT temperature sensor */
/* Pass in a buffer and its size */
int x_temp(buf, size)
char *buf;
int size;
{
    outp(SENSE_PT, 0); /* data = 0 for temperature */
    return x_loader(buf, size);
}

/* Get PI Sense HAT pressure sensor */
/* Pass in a buffer and its size */
int x_press(buf, size)
char *buf;
int size;
{
    outp(SENSE_PT, 1); /* data = 1 for pressure */
    return x_loader(buf, size);
}

/* Get PI Sense HAT light sensor */
/* Pass in a buffer and its size */
int x_light(buf, size)
char *buf;
int size;
{
    outp(SENSE_PT, 2); /* data = 2 for light */
    return x_loader(buf, size);
}

/* Get PI Sense HAT humidity sensor */
/* Pass in a buffer and its size */
int x_humid(buf, size)
char *buf;
int size;
{
    outp(SENSE_PT, 3); /* data = 3 for humidity */
    return x_loader(buf, size);
}

/* Get weather key by index */
/* Pass in data index, buffer and its size */
int x_wkey(data, buf, size)
int data;
char *buf;
int size;
{
    outp(WKEY_PT, data);
    return x_loader(buf, size);
}

/* Get weather value by index */
/* Pass in data index, buffer and its size */
int x_wval(data, buf, size)
int data;
char *buf;
int size;
{
    outp(WVAL_PT, data);
    return x_loader(buf, size);
}

/* Get location key by index */
/* Pass in data index, buffer and its size */
int x_lkey(data, buf, size)
int data;
char *buf;
int size;
{
    outp(LKEY_PT, data);
    return x_loader(buf, size);
}

/* Get location value by index */
/* Pass in data index, buffer and its size */
int x_lval(data, buf, size)
int data;
char *buf;
int size;
{
    outp(LVAL_PT, data);
    return x_loader(buf, size);
}

/* Get pollution key by index */
/* Pass in data index, buffer and its size */
int x_pkey(data, buf, size)
int data;
char *buf;
int size;
{
    outp(PKEY_PT, data);
    return x_loader(buf, size);
}

/* Get pollution value by index */
/* Pass in data index, buffer and its size */
int x_pval(data, buf, size)
int data;
char *buf;
int size;
{
    outp(PVAL_PT, data);
    return x_loader(buf, size);
}
