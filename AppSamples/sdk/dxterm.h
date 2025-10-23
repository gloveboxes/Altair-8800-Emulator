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
int bdos();
int bios();

/* Terminal output functions */
int x_numpr(); /* Print signed integer in decimal */

/* Cursor and screen control */
int x_curmv(row, col); /* Move cursor to row,col */
int x_clrsc();         /* Clear screen and reset attributes */
int x_hidcr();         /* Hide cursor */
int x_shwcr();         /* Show cursor */
int x_erseol();        /* Erase from cursor to end of line */

/* Keyboard input functions */
int x_conin(); /* Console input is available wait */
int x_conout(code); /* Console output */
int x_keyrd(); /* Read raw key code no waiting */

/* Key code test functions */
int x_isesc(code);   /* Test if code is ESC */
int x_isctrlc(code); /* Test if code is Ctrl-C */
int x_isup(code);    /* Test if code is Up arrow */
int x_isdn(code);    /* Test if code is Down arrow */
int x_islt(code);    /* Test if code is Left arrow */
int x_isrt(code);    /* Test if code is Right arrow */
int x_isspc(code);   /* Test if code is Space */

/* Color and attribute functions */
int x_setcol(); /* Set foreground color */
int x_rstcol(); /* Reset all attributes */

/* Terminal key codes for Altair environment */
#define XK_ESC 27
#define XK_CTRL_C 3
#define XK_UP 5
#define XK_DN 24
#define XK_LT 19
#define XK_RT 4
#define XK_SPC 32

/* Standard ANSI color codes for xterm.js */
#define XC_BLK 30  /* Black */
#define XC_RED 31  /* Red */
#define XC_GRN 32  /* Green */
#define XC_YEL 33  /* Yellow */
#define XC_BLU 34  /* Blue */
#define XC_MAG 35  /* Magenta */
#define XC_CYN 36  /* Cyan */
#define XC_WHT 37  /* White */
#define XC_BYEL 93 /* Bright Yellow */
#define XC_RST 0   /* Reset all attributes */
