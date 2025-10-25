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

/* Console and BDOS entry points */

#include "dxterm.h"

/* x_numpr(n) - Print signed integer in decimal form. */
int x_numpr(n)
int n;
{
    char buf[6];
    int i;

    if (n == 0)
    {
        putchar('0');
        return 0;
    }

    if (n < 0)
    {
        putchar('-');
        n = -n;
    }

    i = 0;
    while (n > 0 && i < 6)
    {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    while (i--)
        putchar(buf[i]);

    return 0;
}

/* x_curmv(row,col) - Move cursor to 1-based row/column. */
int x_curmv(row, col)
int row;
int col;
{
    printf("%c[%d;%dH", XK_ESC, row, col);
    return 0;
}

/* x_clrsc() - Clear screen and reset attributes. */
int x_clrsc()
{
    printf("%c[2J%c[0m", XK_ESC, XK_ESC);
    x_curmv(1, 1);
    return 0;
}

/* x_hidcr() - Hide the terminal cursor. */
int x_hidcr()
{
    printf("%c[?25l", XK_ESC);
    return 0;
}

/* x_shwcr() - Show the terminal cursor. */
int x_shwcr()
{
    printf("%c[?25h", XK_ESC);
    return 0;
}

int x_conin() /* Console input is available wait */
{
    return (bdos(1) & 0xFF);
}

int x_conout(code) /* Console output */
int code;
{
    return bdos(2, code);
}

/* x_keyrd() - Read raw key code without waiting. */
int x_keyrd()
{
    return (bdos(6, 0xFF) & 0xFF);
}

/* x_keyck() - Return non-zero if a key is waiting. */
int x_keyck()
{
    return (bdos(11) & 0xFF);
}

/* x_keygt() - Fetch next key if available, else return 0. */
int x_keygt()
{
    if (!x_keyck())
        return 0;
    return x_keyrd();
}

/* x_isesc(code) - Return non-zero if code is ESC. */
int x_isesc(code)
int code;
{
    return (code == XK_ESC);
}

/* x_isctrlc(code) - Return non-zero if code is Ctrl-C. */
int x_isctrlc(code)
int code;
{
    return (code == XK_CTRL_C);
}

/* x_isup(code) - Return non-zero if code is Up arrow. */
int x_isup(code)
int code;
{
    return (code == XK_UP);
}

/* x_isdn(code) - Return non-zero if code is Down arrow. */
int x_isdn(code)
int code;
{
    return (code == XK_DN);
}

/* x_islt(code) - Return non-zero if code is Left arrow. */
int x_islt(code)
int code;
{
    return (code == XK_LT);
}

/* x_isrt(code) - Return non-zero if code is Right arrow. */
int x_isrt(code)
int code;
{
    return (code == XK_RT);
}

/* x_isspc(code) - Return non-zero if code is Space. */
int x_isspc(code)
int code;
{
    return (code == XK_SPC);
}

/* x_setcol(code) - Set foreground color using ANSI code. */
int x_setcol(code)
int code;
{
    printf("%c[%dm", XK_ESC, code);
    return 0;
}

/* x_rstcol() - Reset all color and text attributes. */
int x_rstcol()
{
    printf("%c[0m", XK_ESC);
    return 0;
}

/* x_erseol() - Erase from cursor to end of line. */
int x_erseol()
{
    printf("%c[K", XK_ESC);
    return 0;
}
