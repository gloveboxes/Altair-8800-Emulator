/*
 * Tetris Game for Altair 8800 - VT100/xterm.js compatible
 * BDS C 1.6 on CP/M
 *
 * Rewritten for BDS C constraints:
 *  - All symbols unique within first 7 characters
 *  - No goto labels named 'end'
 *  - Simple deterministic gravity; timer port 29: 1 while running, 0 on expiry
 *  - K&R style definitions only
 */

#include "dxterm.h"
#include "dxtimer.h"
#include "dxsys.h"

/* --- BDS C hooks --- */
int bdos();
int bios();
int inp();
int outp();

/* Timer configuration */
#define TIMER_ID 2  /* Use timer 2 */
#define DELAY_MS 25 /* 25ms game loop delay like snake */

/* --- Board & UI layout --- */
#define BD_W 10
#define BD_H 20
#define BD_SROW 3
#define BD_SCOL 30

#define PRV_ROW 6
#define PRV_COL 51

/* --- Game constants --- */
#define PIECE_NONE 0
#define PIECE_I 1
#define PIECE_O 2
#define PIECE_T 3
#define PIECE_S 4
#define PIECE_Z 5
#define PIECE_J 6
#define PIECE_L 7

#define GAME_PLAYING 0
#define GAME_OVER 1
#define GAME_QUIT 3

/* --- Globals (<=7 chars, unique) --- */
int board[BD_H][BD_W];

int act_pcs;      /* active piece id */
int act_rot;      /* active rotation */
int act_x, act_y; /* board coords */

int nxt_pcs; /* next piece id */
int game_st; /* state */
int score;
int lines_cl; /* lines cleared */
int level;
int fall_tm; /* gravity tick counter */
int fall_sp; /* ticks per row */
int soft_dr; /* soft drop active this tick */

int pcs_shp[8][4]; /* 16-bit 4x4 masks, bit15=TL, bit0=BR */

int prv_ax; /* previous active x */
int prv_ay; /* previous active y */
int prv_rt; /* previous active rotation */
int prv_pc; /* previous active piece id */
int prv_on; /* previous active drawn flag */

/* ========================= Console wrappers using dxterm ========================= */
#define chput(c) putchar(c)
#define cputs(s) puts(s)
#define putnum(n) x_numpr(n)
#define cur_mov(row, col) x_curmv(row, col)
#define clr_scr() x_clrsc()
#define hid_cur() x_hidcr()
#define shw_cur() x_shwcr()
#define ers_cur() x_erseol()
#define set_col(code) x_setcol(code)
#define rst_col() x_rstcol()

/* ========================= Simple RNG ========================= */
int iabs(n)
int n;
{
    return (n < 0) ? -n : n;
}

int rnd_pcs()
{ 
    unsigned rnd;
    rnd = x_rand();
    return (rnd % 7) + 1; /* PIECE_I through PIECE_L */
}

/* ========================= Shapes ========================= */
int init_shp()
{
    /* MSB-first: bit15=TL, bit0=BR; row-major */
    /* I */
    pcs_shp[PIECE_I][0] = 0x0F00;
    pcs_shp[PIECE_I][1] = 0x2222;
    pcs_shp[PIECE_I][2] = 0x00F0;
    pcs_shp[PIECE_I][3] = 0x4444;
    /* O */
    pcs_shp[PIECE_O][0] = 0x0660;
    pcs_shp[PIECE_O][1] = 0x0660;
    pcs_shp[PIECE_O][2] = 0x0660;
    pcs_shp[PIECE_O][3] = 0x0660;
    /* T */
    pcs_shp[PIECE_T][0] = 0x0E40;
    pcs_shp[PIECE_T][1] = 0x4C40;
    pcs_shp[PIECE_T][2] = 0x4E00;
    pcs_shp[PIECE_T][3] = 0x4640;
    /* S */
    pcs_shp[PIECE_S][0] = 0x06C0;
    pcs_shp[PIECE_S][1] = 0x4620;
    pcs_shp[PIECE_S][2] = 0x06C0;
    pcs_shp[PIECE_S][3] = 0x4620;
    /* Z */
    pcs_shp[PIECE_Z][0] = 0x0C60;
    pcs_shp[PIECE_Z][1] = 0x2640;
    pcs_shp[PIECE_Z][2] = 0x0C60;
    pcs_shp[PIECE_Z][3] = 0x2640;
    /* J */
    pcs_shp[PIECE_J][0] = 0x08E0;
    pcs_shp[PIECE_J][1] = 0x64C0;
    pcs_shp[PIECE_J][2] = 0x0E20;
    pcs_shp[PIECE_J][3] = 0x6440;
    /* L */
    pcs_shp[PIECE_L][0] = 0x02E0;
    pcs_shp[PIECE_L][1] = 0x4460;
    pcs_shp[PIECE_L][2] = 0x0E80;
    pcs_shp[PIECE_L][3] = 0xC440;
    return 0;
}

int pcell(piece, rot, row, col)
int piece, rot, row, col;
{
    int idx, dt;
    if (piece < PIECE_I || piece > PIECE_L)
        return 0;
    if (row < 0 || row >= 4 || col < 0 || col >= 4)
        return 0;
    dt = pcs_shp[piece][rot & 3];
    idx = row * 4 + col; /* 0..15 */
    return (dt >> (15 - idx)) & 1;
}

int pcs_chr(piece)
int piece;
{
    if (piece == PIECE_I)
        return '#';
    if (piece == PIECE_O)
        return 'O';
    if (piece == PIECE_T)
        return 'T';
    if (piece == PIECE_S)
        return 'S';
    if (piece == PIECE_Z)
        return 'Z';
    if (piece == PIECE_J)
        return 'J';
    if (piece == PIECE_L)
        return 'L';
    return ' ';
}

/* Color palette for pieces using dxterm constants */
int pcs_col(piece)
int piece;
{
    if (piece == PIECE_I)
        return XC_CYN; /* Cyan */
    if (piece == PIECE_O)
        return XC_YEL; /* Yellow */
    if (piece == PIECE_T)
        return XC_MAG; /* Magenta */
    if (piece == PIECE_S)
        return XC_GRN; /* Green */
    if (piece == PIECE_Z)
        return XC_RED; /* Red */
    if (piece == PIECE_J)
        return XC_BLU; /* Blue */
    if (piece == PIECE_L)
        return XC_BYEL; /* Bright Yellow */
    return XC_WHT;      /* White */
}

/* ========================= UI ========================= */
int drw_ins()
{
    cur_mov(1, 1);
    cputs("TETRIS for Altair 8800 V1.5 (Enable Character Mode: Ctrl+L)");
    cur_mov(2, 1);
    cputs("LEFT/RIGHT: Move  UP: Rotate  DOWN: Soft Drop  SPACE: Hard Drop  ESC x2: Quit");
    cur_mov(3, 1);
    cputs("====================================================================");
    return 0;
}

int drw_bor()
{
    int i;
    for (i = 0; i < BD_H + 2; i++)
    {
        cur_mov(BD_SROW + i, BD_SCOL - 1);
        chput('|');
    }
    for (i = 0; i < BD_H + 2; i++)
    {
        cur_mov(BD_SROW + i, BD_SCOL + BD_W * 2);
        chput('|');
    }
    cur_mov(BD_SROW + BD_H + 1, BD_SCOL - 1);
    for (i = 0; i < BD_W * 2 + 2; i++)
        chput('-');
    return 0;
}

int brd_cell(r, c)
int r;
int c;
{
    int piece;
    int rel_r;
    int rel_c;

    piece = board[r][c];

    /* Restore normal piece logic */

    if (game_st == GAME_PLAYING && piece == 0)
    {
        rel_r = r - act_y;
        rel_c = c - act_x;
        if (rel_r >= 0 && rel_r < 4 && rel_c >= 0 && rel_c < 4)
        {
            if (pcell(act_pcs, act_rot, rel_r, rel_c))
            {
                piece = act_pcs;
            }
        }
    }

    return piece;
}

int brd_syn()
{
    int r;
    int c;
    int scr_row;
    int scr_col;
    int color;
    int cur_col;

    rst_col();
    for (r = 0; r < BD_H; r++)
    {
        scr_row = BD_SROW + r + 1;
        scr_col = BD_SCOL;
        cur_col = 0;
        cur_mov(scr_row, scr_col);
        for (c = 0; c < BD_W; c++)
        {
            color = board[r][c];
            if (color != 0)
            {
                color = pcs_col(color);
                if (cur_col != color)
                {
                    set_col(color);
                    cur_col = color;
                }
                chput('#');
                chput('#');
            }
            else
            {
                if (cur_col != 0)
                {
                    rst_col();
                    cur_col = 0;
                }
                chput(' ');
                chput(' ');
            }
        }
        if (cur_col != 0)
        {
            rst_col();
        }
    }

    prv_on = 0;
    prv_pc = PIECE_NONE;
    cur_mov(5, 1);
    return 0;
}

int clr_act(piece, rot, x, y)
int piece;
int rot;
int x;
int y;
{
    int pr;
    int pc;
    int run_len;
    int run_col;
    int br;
    int bc;
    int scr_row;
    int scr_col;
    int i;

    if (piece < PIECE_I || piece > PIECE_L)
        return 0;

    rst_col();
    for (pr = 0; pr < 4; pr++)
    {
        br = y + pr;
        if (br < 0 || br >= BD_H)
            continue;
        run_len = 0;
        for (pc = 0; pc <= 4; pc++)
        {
            if (pc < 4 && pcell(piece, rot, pr, pc))
            {
                bc = x + pc;
                if (bc >= 0 && bc < BD_W)
                {
                    if (run_len == 0)
                        run_col = bc;
                    run_len++;
                    continue;
                }
            }
            if (run_len > 0)
            {
                scr_row = BD_SROW + br + 1;
                scr_col = BD_SCOL + run_col * 2;
                cur_mov(scr_row, scr_col);
                for (i = 0; i < run_len; i++)
                {
                    chput(' ');
                    chput(' ');
                }
                run_len = 0;
            }
        }
    }

    return 0;
}

int drw_act(piece, rot, x, y)
int piece;
int rot;
int x;
int y;
{
    int pr;
    int pc;
    int run_len;
    int run_col;
    int br;
    int bc;
    int scr_row;
    int scr_col;
    int i;
    int color;
    int color_on;

    if (piece < PIECE_I || piece > PIECE_L)
        return 0;

    color = pcs_col(piece);
    color_on = 0;
    for (pr = 0; pr < 4; pr++)
    {
        br = y + pr;
        if (br < 0 || br >= BD_H)
            continue;
        run_len = 0;
        for (pc = 0; pc <= 4; pc++)
        {
            if (pc < 4 && pcell(piece, rot, pr, pc))
            {
                bc = x + pc;
                if (bc >= 0 && bc < BD_W)
                {
                    if (!color_on)
                    {
                        set_col(color);
                        color_on = 1;
                    }
                    if (run_len == 0)
                        run_col = bc;
                    run_len++;
                    continue;
                }
            }
            if (run_len > 0)
            {
                scr_row = BD_SROW + br + 1;
                scr_col = BD_SCOL + run_col * 2;
                cur_mov(scr_row, scr_col);
                for (i = 0; i < run_len; i++)
                {
                    chput('#');
                    chput('#');
                }
                run_len = 0;
            }
        }
    }
    if (color_on)
    {
        rst_col();
    }

    return 0;
}

int brd_drw()
{
    int rot;

    rot = act_rot & 3;

    if (prv_on && game_st == GAME_PLAYING && act_pcs >= PIECE_I && act_pcs <= PIECE_L)
    {
        if (prv_pc == act_pcs && prv_rt == rot && prv_ax == act_x && prv_ay == act_y)
        {
            return 0;
        }
    }

    if (prv_on)
    {
        clr_act(prv_pc, prv_rt, prv_ax, prv_ay);
        prv_on = 0;
    }

    if (game_st == GAME_PLAYING && act_pcs >= PIECE_I && act_pcs <= PIECE_L)
    {
        drw_act(act_pcs, rot, act_x, act_y);
        prv_ax = act_x;
        prv_ay = act_y;
        prv_rt = rot;
        prv_pc = act_pcs;
        prv_on = 1;
    }

    cur_mov(5, 1);
    return 0;
}

int drw_nxt()
{
    int i, j, ch;
    rst_col();
    cur_mov(PRV_ROW, PRV_COL);
    cputs("Next:");
    cur_mov(PRV_ROW + 1, PRV_COL);
    cputs("    ");
    cur_mov(PRV_ROW + 2, PRV_COL);
    cputs("    ");
    cur_mov(PRV_ROW + 3, PRV_COL);
    cputs("    ");
    cur_mov(PRV_ROW + 4, PRV_COL);
    cputs("    ");
    if (nxt_pcs < PIECE_I || nxt_pcs > PIECE_L)
        return 0;
    ch = pcs_chr(nxt_pcs);
    set_col(pcs_col(nxt_pcs));
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (pcell(nxt_pcs, 0, i, j))
            {
                cur_mov(PRV_ROW + 1 + i, PRV_COL + j);
                chput(ch);
            }
    rst_col();
    return 0;
}

int upd_stat()
{
    cur_mov(5, 1);
    cputs("Score: ");
    putnum(score);
    cputs("          ");
    cur_mov(6, 1);
    cputs("Lines: ");
    putnum(lines_cl);
    cputs("          ");
    cur_mov(7, 1);
    cputs("Level: ");
    putnum(level);
    cputs("          ");
    return 0;
}

/* ========================= Game logic ========================= */
int is_pos(piece, rot, x, y)
int piece, rot, x, y;
{
    int i, j, bx, by;
    if (piece < PIECE_I || piece > PIECE_L)
        return 0;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (pcell(piece, rot, i, j))
            {
                bx = x + j;
                by = y + i;
                if (bx < 0 || bx >= BD_W || by >= BD_H)
                    return 0;
                if (by >= 0 && board[by][bx] != 0)
                    return 0; /* allow above top */
            }
    return 1;
}

int plc_pcs()
{
    int i, j, bx, by;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (pcell(act_pcs, act_rot, i, j))
            {
                bx = act_x + j;
                by = act_y + i;
                if (by >= 0 && by < BD_H && bx >= 0 && bx < BD_W)
                    board[by][bx] = act_pcs;
            }
    prv_on = 0;
    prv_pc = PIECE_NONE;
    act_pcs = PIECE_NONE;
    return 0;
}

int clr_lin()
{
    int r, c, s, full, found;
    found = 0;
    for (r = BD_H - 1; r >= 0; r--)
    {
        full = 1;
        for (c = 0; c < BD_W; c++)
            if (board[r][c] == 0)
            {
                full = 0;
                break;
            }
        if (full)
        {
            found++;
            for (s = r; s > 0; s--)
                for (c = 0; c < BD_W; c++)
                    board[s][c] = board[s - 1][c];
            for (c = 0; c < BD_W; c++)
                board[0][c] = 0;
            r++;
        }
    }
    if (found > 0)
    {
        lines_cl += found;
        if (found == 1)
            score += 40 * (level + 1);
        else if (found == 2)
            score += 100 * (level + 1);
        else if (found == 3)
            score += 300 * (level + 1);
        else if (found == 4)
            score += 1200 * (level + 1);
        level = lines_cl / 10;
        if (level > 20)
            level = 20;
        fall_sp = 20 - (level * 1); /* Start at ~0.5 second, get faster with level */
        if (fall_sp < 6)
            fall_sp = 6; /* Minimum 150ms fall time */
        brd_syn();
        upd_stat();
    }
    return found;
}

int spn_new()
{
    act_pcs = nxt_pcs;
    act_rot = 0;
    act_x = 3;
    act_y = 0;
    fall_tm = 0;
    if (!is_pos(act_pcs, act_rot, act_x, act_y))
    {
        game_st = GAME_OVER;
        return 0;
    }
    prv_on = 0;
    brd_drw();
    nxt_pcs = rnd_pcs(); /* Random next piece */
    drw_nxt();
    upd_stat();
    return 1;
}

/* ========================= Input using dxterm ========================= */
#define key_rdy() x_keyck()
#define rd_key() x_keygt()

int hnd_inp(ch)
int ch;
{
    int k, nx, nr;
    int moved;
    int scch;
    int redraw;
    k = ch;
    if (game_st != GAME_PLAYING)
        return 0;

    moved = 0;
    scch = 0;
    redraw = 0;

    if (x_islt(k))
    {
        nx = act_x - 1;
        if (is_pos(act_pcs, act_rot, nx, act_y))
        {
            act_x = nx;
            moved = 1;
            redraw = 1;
        }
    }
    else if (x_isrt(k))
    {
        nx = act_x + 1;
        if (is_pos(act_pcs, act_rot, nx, act_y))
        {
            act_x = nx;
            moved = 1;
            redraw = 1;
        }
    }
    else if (x_isup(k))
    {
        nr = (act_rot + 1) & 3;
        if (is_pos(act_pcs, nr, act_x, act_y))
        {
            act_rot = nr;
            moved = 1;
            redraw = 1;
        }
    }
    else if (x_isdn(k))
    {
        soft_dr = 1;
    }
    else if (x_isspc(k))
    {
        while (is_pos(act_pcs, act_rot, act_x, act_y + 1))
        {
            act_y++;
            score += 2;
            scch = 1;
        }
        fall_tm = 0;
        moved = 1;
        redraw = 1;
    }

    if (redraw || scch || soft_dr)
    {
        brd_drw();
    }

    if (scch)
    {
        upd_stat();
    }

    return 0;
}

/* ========================= Game over ========================= */
int show_go()
{
    cur_mov(15, 5);
    cputs("GAME OVER!");
    cur_mov(16, 5);
    cputs("Final Score: ");
    putnum(score);
    cur_mov(17, 5);
    cputs("Lines Cleared: ");
    putnum(lines_cl);
    cur_mov(18, 5);
    cputs("Level Reached: ");
    putnum(level);
    cur_mov(19, 5);
    return 0;
}

/* ========================= Main ========================= */
int main()
{
    int r, c, key;
    int drop_sp;
    int ch;

    game_st = GAME_PLAYING;
    score = 0;
    lines_cl = 0;
    level = 0;
    fall_tm = 0;
    fall_sp = 20;
    soft_dr = 0;

    for (r = 0; r < BD_H; r++)
        for (c = 0; c < BD_W; c++)
            board[r][c] = 0;
    init_shp();

    clr_scr();
    hid_cur();
    drw_ins();
    drw_bor();
    brd_syn();
    brd_drw();
    upd_stat();

    nxt_pcs = rnd_pcs();
    if (!spn_new())
    { /* immediate game over */
    }

    while (game_st == GAME_PLAYING)
    {
        /* Start timer for this loop iteration */
        x_tmrset(TIMER_ID, DELAY_MS);

        /* Only increment fall timer once per timer cycle */
        fall_tm++;

        /* Check if piece should fall */
        drop_sp = soft_dr ? 8 : fall_sp;
        if (fall_tm >= drop_sp)
        {
            if (is_pos(act_pcs, act_rot, act_x, act_y + 1))
            {
                act_y++;
                brd_drw();
                if (soft_dr)
                {
                    score += 1;
                }
            }
            else
            {
                plc_pcs();
                brd_drw();
                clr_lin();
                if (!spn_new())
                    break;
            }

            fall_tm = 0;
            soft_dr = 0;
        }

        /* Wait while timer is active and game is playing */
        while (x_tmract(TIMER_ID) && game_st == GAME_PLAYING)
        {
            /* Check for input during wait */
            ch = x_keyrd();

            if (ch != 0)
            {
                if (x_isesc(ch))
                {
                    game_st = GAME_QUIT;
                }
                hnd_inp(ch);
            }
        }
    }

    brd_drw();
    upd_stat();
    show_go();

    cur_mov(24, 1);
    shw_cur();
    cputs("Thanks for playing Tetris!\r\n");
    return 0;
}
