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

/* Board dimensions */
#define BOARD_ROWS 28
#define BOARD_COLS 80

/* Game board storage */
char gboard[BOARD_ROWS][BOARD_COLS + 1]; /* +1 for null terminator */

/* Ball position */
int ballrow, ballcol;

/* Initialize VT100 mode and clear screen */
int g_init()
{
    x_clrsc();
    x_hidcr();
    return 0;
}

/* Clear the game board area */
int g_clrbd()
{
    int row, col;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            /* Create border of * characters */
            if (row == 0 || row == BOARD_ROWS - 1 || col == 0 || col == BOARD_COLS - 1)
            {
                gboard[row][col] = '*';
            }
            else
            {
                gboard[row][col] = ' ';
            }
        }
        gboard[row][BOARD_COLS] = 0; /* Null terminate */
    }

    /* Clear screen area inside border */
    for (row = 2; row < BOARD_ROWS + 2; row++)
    {
        x_curmv(row, 2);
        x_erseol();
    }

    return 0;
}

/* Draw the game board to screen */
int g_draw()
{
    int row;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        x_curmv(row + 1, 1); /* +1 to start at row 1 */
        puts(gboard[row]);
    }

    return 0;
}

/* Set character at board position */
int g_setpt(row, col, ch)
int row;
int col;
char ch;
{
    if (row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS)
    {
        gboard[row][col] = ch;
    }
    return 0;
}

/* Check if direction key changed */
int chkdir(c, direction)
char c;
int *direction;
{
    if (*direction == c || c == 0)
        return 0;

    /* Check if valid direction key */
    if (c == XK_UP || c == XK_DN || c == XK_LT || c == XK_RT)
    {
        *direction = c;
        return 1;
    }
    return 0;
}

/* Update display with current direction and board */
int upd_disp(direction)
int *direction;
{
    int newrow, newcol;

    /* Calculate new position based on direction */
    newrow = ballrow;
    newcol = ballcol;

    if (*direction == XK_UP)
        newrow--;
    else if (*direction == XK_DN)
        newrow++;
    else if (*direction == XK_LT)
        newcol--;
    else if (*direction == XK_RT)
        newcol++;

    /* Check for wall collision (border is made of *) */
    if (newrow <= 0 || newrow >= BOARD_ROWS - 1 ||
        newcol <= 0 || newcol >= BOARD_COLS - 1)
    {
        /* Hit wall - stop movement */
        *direction = 0;
    }
    else
    {
        /* Clear old ball position on screen */
        x_curmv(ballrow + 1, ballcol + 1); /* +1 to account for single border */
        putchar(' ');

        /* Update ball position */
        ballrow = newrow;
        ballcol = newcol;

        /* Draw new ball position on screen */
        x_curmv(ballrow + 1, ballcol + 1); /* +1 to account for single border */
        putchar('O');
    }

    /* Show direction at bottom of screen */
    x_curmv(BOARD_ROWS + 2, 1);
    printf("Direction: %d, Position: (%d,%d)           ", *direction, ballrow, ballcol);

    return 0;
}

main()
{
    char c;
    int direction;
    int row, col;

    direction = 0;

    /* Initialize VT100 mode and game board */
    g_init();
    g_clrbd();

    /* Initialize ball position in the middle of the screen */
    ballrow = BOARD_ROWS / 2;
    ballcol = BOARD_COLS / 2;
    g_setpt(ballrow, ballcol, 'O');

    /* Draw initial board */
    g_draw();

    x_tmrset(0, 50); /* Set timer 0 for 50 ms */

    while (1)
    {
        c = x_keyrd(); /* Read raw key code no waiting */

        if (x_isesc(c) || x_isctrlc(c)) /* ESC or Ctrl-C to exit */
        {
            break;
        }

        if (x_tmrexp(0) || chkdir(c, &direction))
        {
            upd_disp(&direction);
            x_tmrset(0, 50); /* Reset timer 0 for another 50 ms */
        }
    }

    /* Cleanup - restore cursor and clear screen */
    x_shwcr();
    x_clrsc();
    puts("Game exited. Thank you for playing!\r\n");

    return 0;
}