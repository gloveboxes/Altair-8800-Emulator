/*
 * Snake Game for Altair 8800 - VT100/xterm.js compatible
 * BDS C 1.6 on CP/M
 *
 * Classic Snake game using arrow keys for movement.
 * Collect food (*) to grow longer and increase score.
 * Don't hit walls or yourself!
 *
 * Controls:
 * - Arrow keys: Change direction
 * - ESC: Quit game
 *
 * Based on breakout.c pattern for BDS C compatibility
 */

#include "dxterm.h"
#include "dxsys.h"



/* Timer configuration */
#define TIMER_ID 2  /* Use timer 2 */
#define TIMER_MS 50 /* 50ms game loop timer */

/* Screen Boundaries */
#define MIN_ROW 6
#define MAX_ROW 25
#define MIN_COL 5
#define MAX_COL 75

/* Snake Settings */
#define MAX_SNAKE_LENGTH 200
#define INITIAL_LENGTH   3

/* Game States */
#define GAME_PLAYING 0
#define GAME_OVER    1
#define GAME_QUIT    2

/* Directions - using SDK key codes */
#define DIR_NONE  0

/* Snake body coordinates */
int sn_row[MAX_SNAKE_LENGTH];
int sn_col[MAX_SNAKE_LENGTH];
int sn_len;
int sn_dir;
int nxt_dir;

/* Food position */
int food_row, food_col;
int food_exists;

/* Game state */
int gm_st;
int score;
int sp_lvl;

/* Simple counter-based timing */
int move_counter;



/* --- Game Display --- */

int draw_walls()
{
    int i;

    /* Set green color for walls */
    x_setcol(XC_GRN);

    /* Draw top wall */
    x_curmv(MIN_ROW - 1, MIN_COL - 1);
    for (i = MIN_COL - 1; i <= MAX_COL + 1; i++)
    {
        putchar('#');
    }

    /* Draw side walls */
    for (i = MIN_ROW; i <= MAX_ROW; i++)
    {
        x_curmv(i, MIN_COL - 1);
        putchar('#');
        x_curmv(i, MAX_COL + 1);
        putchar('#');
    }

    /* Draw bottom wall */
    x_curmv(MAX_ROW + 1, MIN_COL - 1);
    for (i = MIN_COL - 1; i <= MAX_COL + 1; i++)
    {
        putchar('#');
    }

    /* Reset color */
    x_rstcol();
    return 0;
}

int draw_instructions()
{
    x_curmv(1, 1);
    puts("Snake Game for Altair 8800 (Enable Character Mode: Ctrl+L)");
    x_curmv(2, 1);
    puts("Arrow keys to move, ESC to quit. Don't hit walls or yourself!");
    x_curmv(3, 1);
    puts("Eat food (*) to grow and increase score.");
    x_curmv(4, 1);
    puts("------------------------------------------------------------------");
    return 0;
}

int dr_seg(row, col, is_head)
int row;
int col;
int is_head;
{
    /* Set red color for snake */
    x_setcol(XC_RED);
    x_curmv(row, col);
    if (is_head)
    {
        putchar('O'); /* Head */
    }
    else
    {
        putchar('o'); /* Body */
    }
    /* Reset color */
    x_rstcol();
    return 0;
}

int er_pos(row, col)
int row;
int col;
{
    x_curmv(row, col);
    putchar(' ');
    return 0;
}

int draw_food(row, col)
int row;
int col;
{
    /* Set blue color for food */
    x_setcol(XC_BLU);
    x_curmv(row, col);
    putchar('*');
    /* Reset color */
    x_rstcol();
    return 0;
}

int snake_update_status()
{
    x_curmv(5, 1);
    puts("Score: ");
    x_numpr(score);
    puts("   Length: ");
    x_numpr(sn_len);
    puts("   Speed: ");
    x_numpr(sp_lvl);
    puts("                    ");
    return 0;
}

/* --- Random Number Generation --- */

unsigned get_random()
{
    return x_rand();
}

/* --- Food Management --- */

int is_occ(row, col)
int row;
int col;
{
    int i;

    /* Check if position conflicts with snake body */
    for (i = 0; i < sn_len; i++)
    {
        if (sn_row[i] == row && sn_col[i] == col)
        {
            return 1;
        }
    }
    return 0;
}

int place_food()
{
    int attempts;
    int food_r, food_c;

    attempts = 0;
    while (attempts < 50)
    { /* Try up to 50 times */
        food_r = MIN_ROW + (get_random() % (MAX_ROW - MIN_ROW + 1));
        food_c = MIN_COL + (get_random() % (MAX_COL - MIN_COL + 1));

        if (!is_occ(food_r, food_c))
        {
            food_row    = food_r;
            food_col    = food_c;
            food_exists = 1;
            draw_food(food_row, food_col);
            return 1;
        }
        attempts++;
    }
    return 0; /* Failed to place food */
}

/* --- Input Handling --- */

int snake_handle_input()
{
    int key;

    key = x_keyrd(); /* Read raw key code no waiting */
    if (!key)
        return 0;

    if (x_isesc(key))
    {
        gm_st = GAME_QUIT;
        return 1;
    }

    /* Handle direction changes - prevent reversing into self */
    if (x_isup(key) && sn_dir != XK_DN)
    {
        nxt_dir = XK_UP;
    }
    else if (x_isdn(key) && sn_dir != XK_UP)
    {
        nxt_dir = XK_DN;
    }
    else if (x_islt(key) && sn_dir != XK_RT)
    {
        nxt_dir = XK_LT;
    }
    else if (x_isrt(key) && sn_dir != XK_LT)
    {
        nxt_dir = XK_RT;
    }

    return 0;
}

/* --- Game Logic --- */

int init_snake()
{
    int i;

    sn_len  = INITIAL_LENGTH;
    sn_dir  = XK_RT;
    nxt_dir = XK_RT;

    /* Place snake in center, moving right */
    for (i = 0; i < sn_len; i++)
    {
        sn_row[i] = (MIN_ROW + MAX_ROW) / 2;
        sn_col[i] = (MIN_COL + MAX_COL) / 2 - i;
    }

    /* Draw initial snake */
    for (i = 0; i < sn_len; i++)
    {
        dr_seg(sn_row[i], sn_col[i], (i == 0));
    }

    return 0;
}

int move_snake()
{
    int head_r, head_c;
    int i;
    int ate_food;

    /* Update direction */
    sn_dir = nxt_dir;

    /* Calculate new head position */
    head_r = sn_row[0];
    head_c = sn_col[0];

    if (sn_dir == XK_UP)
    {
        head_r--;
    }
    else if (sn_dir == XK_DN)
    {
        head_r++;
    }
    else if (sn_dir == XK_LT)
    {
        head_c--;
    }
    else if (sn_dir == XK_RT)
    {
        head_c++;
    }

    /* Check wall collision */
    if (head_r < MIN_ROW || head_r > MAX_ROW || head_c < MIN_COL || head_c > MAX_COL)
    {
        gm_st = GAME_OVER;
        return 0;
    }

    /* Check self collision */
    for (i = 0; i < sn_len; i++)
    {
        if (sn_row[i] == head_r && sn_col[i] == head_c)
        {
            gm_st = GAME_OVER;
            return 0;
        }
    }

    /* Check food collision */
    ate_food = 0;
    if (food_exists && head_r == food_row && head_c == food_col)
    {
        ate_food    = 1;
        food_exists = 0;
        score += 10;

        /* Increase speed every 50 points */
        if (score % 50 == 0 && sp_lvl < 10)
        {
            sp_lvl++;
        }
    }

    /* Move snake body */
    if (!ate_food)
    {
        /* Erase tail */
        er_pos(sn_row[sn_len - 1], sn_col[sn_len - 1]);

        /* Shift body positions */
        for (i = sn_len - 1; i > 0; i--)
        {
            sn_row[i] = sn_row[i - 1];
            sn_col[i] = sn_col[i - 1];
        }
    }
    else
    {
        /* Snake grows - shift body and increase length */
        if (sn_len < MAX_SNAKE_LENGTH)
        {
            for (i = sn_len; i > 0; i--)
            {
                sn_row[i] = sn_row[i - 1];
                sn_col[i] = sn_col[i - 1];
            }
            sn_len++;
        }
    }

    /* Set new head position */
    sn_row[0] = head_r;
    sn_col[0] = head_c;

    /* Redraw snake head and body */
    dr_seg(sn_row[0], sn_col[0], 1); /* Head */
    if (sn_len > 1)
    {
        dr_seg(sn_row[1], sn_col[1], 0); /* Neck (was head) */
    }

    return 1;
}



/* --- Game Over Display --- */

int show_game_over()
{
    x_curmv(15, 30);
    puts("GAME OVER!");
    x_curmv(16, 25);
    puts("Final Score: ");
    x_numpr(score);
    x_curmv(17, 25);
    puts("Final Length: ");
    x_numpr(sn_len);
    x_curmv(18, 25);
    puts("Press ESC to quit");
    return 0;
}

/* --- Main Program --- */

int main()
{
    int key;
    int move_delay;

    /* Initialize game state */
    gm_st       = GAME_PLAYING;
    score       = 0;
    sp_lvl      = 1;
    food_exists = 0;



    /* Set up display */
    x_clrsc();
    x_hidcr();
    draw_instructions();
    draw_walls();

    /* Initialize snake */
    init_snake();

    /* Place first food */
    place_food();

    /* Initial status display */
    snake_update_status();

    move_counter = 0;
    x_tmrset(TIMER_ID, 20); /* Set timer for main loop - 20ms like game.c */

    /* Main game loop */
    while (gm_st == GAME_PLAYING)
    {
        /* Handle input every cycle */
        snake_handle_input();
        
        if (gm_st != GAME_PLAYING)
            break;

        /* Use counter-based movement timing */
        if (x_tmrexp(TIMER_ID))
        {
            move_counter++;
            
            /* Move snake every N cycles based on speed level */
            {
                move_delay = 10 - sp_lvl; /* Starts at 10, gets faster */
                if (move_delay < 4) move_delay = 4; /* Minimum delay */
                
                if (move_counter >= move_delay)
                {
                    move_snake();
                    move_counter = 0;
                    
                    /* Place new food if needed */
                    if (!food_exists && gm_st == GAME_PLAYING)
                    {
                        place_food();
                    }
                    
                    /* Update status display */
                    snake_update_status();
                }
            }

            x_tmrset(TIMER_ID, 20); /* Reset 20ms timer like game.c */
        }
    }

    /* Game over or quit */
    if (gm_st == GAME_OVER)
    {
        show_game_over();

        /* Wait for quit key */
        while (1)
        {
            key = x_keyrd(); /* Read raw key code no waiting */
            if (key && x_isesc(key))
                break;
        }
    }

    /* Cleanup */
    x_curmv(27, 1);
    x_shwcr();
    puts("Thanks for playing Snake!\r\n");

    return 0;
}
