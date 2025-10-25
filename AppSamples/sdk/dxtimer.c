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
 *
 * ============================================================
 */

/* Altair 8800 timer functions for BDS C
 *
 * Timer 0: Ports 24/25 - Set high byte (24), low byte (25) and start
 * Timer 1: Ports 26/27 - Set high byte (26), low byte (27) and start
 * Timer 2: Ports 28/29 - Set high byte (28), low byte (29) and start
 */

#include "dxtimer.h"

/* Timer port mappings */
#define T0_MSH 24 /* Timer 0 high byte */
#define T0_MSL 25 /* Timer 0 low byte */
#define T1_MSH 26 /* Timer 1 high byte */
#define T1_MSL 27 /* Timer 1 low byte */
#define T2_MSH 28 /* Timer 2 high byte */
#define T2_MSL 29 /* Timer 2 low byte */

/* BDS C I/O entry points */
int inp(); /* int inp(port) */
outp();    /* void outp(port,val) */

/* ------------------------------------------------------- */
/* x_getpt(timer, type) - Get port for timer and type.
 * timer: 0-2, type: 0=high_byte_port, 1=low_byte_port
 * Returns port number or -1 if invalid timer.
 */
x_getpt(timer, type) int timer, type;
{

    if (type == 0) /* high byte port */
    {
        switch (timer)
        {
        case 0:
            return T0_MSH;
        case 1:
            return T1_MSH;
        case 2:
            return T2_MSH;
        }
    }
    else /* low byte port */
    {
        switch (timer)
        {
        case 0:
            return T0_MSL;
        case 1:
            return T1_MSL;
        case 2:
            return T2_MSL;
        }
    }
    return -1; /* Should never reach here */
}

/* ------------------------------------------------------- */
/* x_delay(timer, ms) - Blocking delay in milliseconds (0..65535).
 * timer: 0-2, ms: delay in milliseconds
 * Returns 0 on success, -1 if invalid timer.
 */
int x_delay(timer, ms) int timer;
unsigned ms;
{
    char hi_byte, lo_byte;
    int hi_port, lo_port;

    /* Validate timer range */
    if (timer < 0 || timer > 2)
        return -1;

    /* Get ports */
    hi_port = x_getpt(timer, 0);
    lo_port = x_getpt(timer, 1);

    /* Set high byte (bits 15-8) */
    hi_byte = ms >> 8;
    outp(hi_port, hi_byte);

    /* Set low byte (bits 7-0) and start timer */
    lo_byte = ms & 0xFF;
    outp(lo_port, lo_byte);

    /* Poll until timer expires (inp returns 0 when expired) */
    while (inp(lo_port))
        ;

    return 0;
}

/* ------------------------------------------------------- */
/* x_tmrset(timer, ms) - Start non-blocking timer for given milliseconds.
 * timer: 0-2, ms: delay in milliseconds
 */
int x_tmrset(timer, ms) int timer;
unsigned ms;
{
    char hi_byte, lo_byte;
    int hi_port, lo_port;

    /* Validate timer range */
    if (timer < 0 || timer > 2)
        return -1;

    /* Get ports */
    hi_port = x_getpt(timer, 0);
    lo_port = x_getpt(timer, 1);

    /* Set high byte (bits 15-8) */
    hi_byte = ms >> 8;
    outp(hi_port, hi_byte);

    /* Set low byte (bits 7-0) and start timer */
    lo_byte = ms & 0xFF;
    outp(lo_port, lo_byte);

    return 0;
}

/* ------------------------------------------------------- */
/* x_tmrexp(timer) - Check if non-blocking timer has expired.
 * timer: 0-2
 * Returns 1 (true) if expired, 0 (false) if still running, 1 (true) if invalid timer.
 */
int x_tmrexp(timer) int timer;
{
    int lo_port;

    /* Validate timer range */
    if (timer < 0 || timer > 2)
        return -1;

    /* Get low byte port */
    lo_port = x_getpt(timer, 1);

    return inp(lo_port) == 0; /* 0 if running, non-zero if expired */
}

/* x_tmract(timer) - Check if non-blocking timer is active.
 * timer: 0-2
 * Returns non-zero if active/running, 0 if expired, -1 if invalid timer.
 */
int x_tmract(timer) int timer;
{
    int lo_port;

    /* Validate timer range */
    if (timer < 0 || timer > 2)
        return -1;

    /* Get low byte port */
    lo_port = x_getpt(timer, 1);

    return inp(lo_port); /* non-zero if running, 0 if expired */
}

