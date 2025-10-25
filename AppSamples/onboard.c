/*
 * onboard.c - Onboard sensor monitoring for Altair 8800
 * Version 1.1
 * 
 * Reads temperature, pressure, humidity and system information
 * from the emulator's onboard sensors using I/O ports.
 * Real-time display using VT100/ANSI escape sequences.
 * 
 * Usage: Press ESC twice to quit
 * 
 * Based on onboard.bas - converted to BDS C 1.6
 * 
 * BDS C Compiler and Libraries with Long Integer support
 * https://msxhub.com/BDSC
 * 
 * To compile with BDS C:
 * cc onboard
 * clink onboard long
 */

#include <stdio.h>

/* VT100/ANSI Control */
#define KEY_ESC 27
#define KEY_CTRL_C 3

/* Timer configuration */
#define TIMER_ID 0      /* Use timer 0 */
#define DELAY_MS 5000   /* 5 second delay */

/* Function prototypes */
int main();
int read_string_from_port();
int cputs();
int chput();
int putnum();
int clear_screen();
int cursor_move();
int hide_cursor();
int show_cursor();
int clear_line();
int check_key_ready();
int get_key();
int display_sensor_data();
int init_display();

/* Timer library functions */
int x_delay();
int x_tmrset();
int x_tmrexp();


/* I/O port functions */
int bdos(), bios(), inp(), outp();

/* Global variables for sensor data */
char sensor_buffer[256];
char clear_padding[21];  /* Pre-allocated padding string */

/* Main program */
int main()
{
    int reading_count, key_pressed, quit_requested;
    
    reading_count = 0;
    quit_requested = 0;
    
    /* Initialize display and padding string */
    init_display();
    
    while (!quit_requested) {
        /* Check for quit key before doing any screen updates */
        if (check_key_ready()) {
            key_pressed = get_key();
            if (key_pressed == 27) {  /* ESC key */
                quit_requested = 1;
                break;  /* Exit main loop immediately */
            }
        }
        
        /* Start accelerometer and increment counter */
        outp(64, 5);
        reading_count++;
        
        /* Update reading count */
        cursor_move(5, 1);
        cputs("Reading: ");
        putnum(reading_count);
        cputs("                ");
        
        /* Display all sensor data */
        display_sensor_data();
        
        /* Update status line */
        cursor_move(14, 1);
        cputs("Status: Monitoring continuously... (5 sec delay)");
        
        /* Sleep using timer library with keyboard checking */
        x_tmrset(TIMER_ID, DELAY_MS);   /* Start timer with configured delay */
        while (x_tmract(TIMER_ID) && !quit_requested) {
            /* Check for keypress during sleep */
            if (check_key_ready()) {
                key_pressed = get_key();
                if (key_pressed == 27) {  /* ESC key */
                    quit_requested = 1;
                    break;  /* Exit timer loop immediately */
                }
            }
        }
    }
    
    /* Final status update */
    cursor_move(14, 1);
    cputs("Status: User requested quit. Stopping...     ");
    
    /* Stop the accelerometer */
    outp(64, 4);
    
    /* Restore cursor and move to bottom */
    cursor_move(16, 1);
    show_cursor();
    cputs("Sensor monitoring stopped by user.\r\n");
    
    return 0;
}

/*
 * Read string data from port 200 until null character
 * This is equivalent to the GOSUB 1600 subroutine in BASIC
 */
int read_string_from_port(buffer, max_len)
char *buffer;
int max_len;
{
    int i, ch;
    
    i = 0;
    ch = inp(200);
    
    while (ch != 0 && i < max_len - 1) {
        buffer[i] = ch;
        i++;
        ch = inp(200);
    }
    
    buffer[i] = 0;      /* Null terminate */
    return i;
}

/*
 * Output a string to console
 */
int cputs(s)
char *s;
{
    while (*s) {
        chput(*s);
        s++;
    }
    return 0;
}

/*
 * Output a character to console using BIOS
 */
int chput(c)
char c;
{
    return bios(4, c);
}

/*
 * Output a number as ASCII string
 */
int putnum(n)
int n;
{
    char buffer[16];
    int i, digit, started;
    
    if (n == 0) {
        chput('0');
        return 0;
    }
    
    if (n < 0) {
        chput('-');
        n = -n;
    }
    
    /* Convert number to string (reverse order) */
    i = 0;
    while (n > 0) {
        buffer[i] = '0' + (n % 10);
        n = n / 10;
        i++;
    }
    
    /* Output digits in correct order */
    while (i > 0) {
        i--;
        chput(buffer[i]);
    }
    
    return 0;
}

/*
 * VT100/ANSI Screen Control Functions
 */

/* Clear entire screen and home cursor */
int clear_screen()
{
    chput(KEY_ESC);
    cputs("[2J");
    cursor_move(1, 1);
    return 0;
}

/* Move cursor to row, col (1-based) */
int cursor_move(row, col)
int row;
int col;
{
    chput(KEY_ESC);
    cputs("[");
    putnum(row);
    cputs(";");
    putnum(col);
    cputs("H");
    return 0;
}

/* Hide cursor */
int hide_cursor()
{
    chput(KEY_ESC);
    cputs("[?25l");
    return 0;
}

/* Show cursor */
int show_cursor()
{
    chput(KEY_ESC);
    cputs("[?25h");
    return 0;
}

/* Clear current line */
int clear_line()
{
    chput(KEY_ESC);
    cputs("[2K");
    return 0;
}

/*
 * Keyboard Input Functions (Non-blocking)
 */

/* Check if a key is ready to be read (non-blocking) */
int check_key_ready()
{
    return (bdos(11) & 0xFF);
}

/* Get a key from keyboard (call only if check_key_ready() returns true) */
int get_key()
{
    return (bdos(6, 0xFF) & 0xFF);
}

/*
 * Optimized Functions
 */

/* Initialize display and pre-fill padding string */
int init_display()
{
    int i;
    
    /* Initialize padding string once */
    for (i = 0; i < 20; i++) {
        clear_padding[i] = ' ';
    }
    clear_padding[20] = 0;  /* Null terminate */
    
    /* Set up display */
    clear_screen();
    hide_cursor();
    
    /* Draw static header */
    cursor_move(1, 1);
    cputs("===== Altair 8800 Onboard Sensor Monitor v1.1 =====");
    cursor_move(2, 1);
    cputs("===================================================");
    cursor_move(3, 1);
    cputs("Press ESC twice to quit.");
    
    return 0;
}


/* Optimized sensor data display */
int display_sensor_data()
{
    char luptime[4]; 
    char l3600[4], l60[4];
    char lhours[4], lrem[4];
    char lmins[4];
    char bufHours[16], bufMins[16];

    /* Get and display emulator version */
    cursor_move(7, 1);
    cputs("Emulator version: ");
    outp(70, 0);
    read_string_from_port(sensor_buffer, 255);
    cputs(sensor_buffer);
    cputs(clear_padding);  /* Use pre-allocated padding */
    
    /* Get and display system uptime in seconds */
    cursor_move(8, 1);
    cputs("Uptime in secs:   ");
    outp(41, 1);
    read_string_from_port(sensor_buffer, 255);
    cputs(sensor_buffer);
    cputs(clear_padding);
    
    /* Calculate and display uptime in hours:minutes format using unsigned long */
    cursor_move(9, 1);
    cputs("Uptime hrs:mins:  ");
    
    atol(luptime,sensor_buffer);
    itol(l3600, 3600);
    itol(l60,   60);
    ldiv(lhours, luptime, l3600);   /* lhours = luptime / 3600 */
    lmod(lrem,  luptime, l3600);    /* lrem   = luptime % 3600 */

    ldiv(lmins, lrem, l60);         /* lmins = (luptime % 3600) / 60 */

    ltoa(bufHours, lhours);
    ltoa(bufMins,  lmins);
    cputs(bufHours);
    cputs(":");
    
    /* Pad minutes with leading zero if less than 10 */
    if (atoi(bufMins) < 10) {
        cputs("0");
    }
    cputs(bufMins);
    
    cputs(clear_padding);
    
    /* Get and display temperature */
    cursor_move(10, 1);
    cputs("Temperature:      ");
    outp(63, 0);
    read_string_from_port(sensor_buffer, 255);
    cputs(sensor_buffer);
    cputs(clear_padding);
    
    /* Get and display pressure */
    cursor_move(11, 1);
    cputs("Pressure:         ");
    outp(63, 1);
    read_string_from_port(sensor_buffer, 255);
    cputs(sensor_buffer);
    cputs(clear_padding);
    
    /* Get and display humidity */
    cursor_move(12, 1);
    cputs("Humidity:         ");
    outp(63, 3);
    read_string_from_port(sensor_buffer, 255);
    cputs(sensor_buffer);
    cputs(clear_padding);
    
    return 0;
}