/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

/* C application to demonstrate use of Intel 8080 IO Ports */

#include "dxtimer.h"

#define DELAY 10

/* dxterm library function declarations */
int putchar(); /* int putchar(c); */
int puts();  /* int puts(s); */
int x_numpr(); /* int x_numpr(n); */
int x_curmv(); /* int x_curmv(row, col); */
int x_clrsc(); /* int x_clrsc(); */
int x_hidcr(); /* int x_hidcr(); */
int x_shwcr(); /* int x_shwcr(); */
int x_setcol(); /* int x_setcol(code); */
int x_rstcol(); /* int x_rstcol(); */
int x_erseol(); /* int x_erseol(); */
int x_keyrd(); /* int x_keyrd(); */
int x_keyrd(); /* int x_keyrd(); */
int x_isesc(); /* int x_isesc(code); */

/* Long integer library function declarations */
char *itol(); /* char *itol(result,n); */
char *ladd(); /* char *ladd(result,op1,op2); */
char *ltoa(); /* char *ltoa(result,op1); */

/* String library function declarations */
char *strcpy(); /* char *strcpy(dest,src); */
char *strcat(); /* char *strcat(dest,src); */

/* dxterm color constants */
#define XC_BLK 30   /* Black */
#define XC_RED 31   /* Red */
#define XC_GRN 32   /* Green */
#define XC_YEL 33   /* Yellow */
#define XC_BLU 34   /* Blue */
#define XC_MAG 35   /* Magenta */
#define XC_CYN 36   /* Cyan */
#define XC_WHT 37   /* White */
#define XC_BYEL 93  /* Bright Yellow */

/*
w = weather
l = location
p = pollution
*/

int w_key;
int w_value;
int w_items;
int l_key;
int l_value;
int l_items;
int p_key;
int p_value;
int p_items;

main()
{
    char c[4], one[4];  /* long integers for reading counter */
    unsigned l;
    char buffer[50];
    int i;
    char *data;

    w_key   = 34;
    w_value = 35;
    w_items = 6;
    l_key   = 36;
    l_value = 37;
    l_items = 4;
    p_key   = 38;
    p_value = 39;
    p_items = 9;

    /* Initialize screen */
    x_clrsc();
    x_hidcr();
    
    /* Draw static layout once */
    dr_layout();
    dr_air();

    /* Initialize long integers */
    itol(c, 0);      /* reading counter starts at 0 */
    itol(one, 1);    /* increment value */

    for (;;)  /* infinite loop */
    {
        /* Update only dynamic data - no screen clearing */
        up_head(c, buffer);
        up_loc();
        up_wthr();
        up_air();

        /* Wait on Port 32 to go false to signify no pending Azure IoT Publish */
        while (inp(32))
            ;
        /* Call port 32 to publish weather data to Azure IoT */
        outp(32, 0);

        /* Display footer with countdown and check for ESC */
        if (dr_foot()) {
            break; /* Exit if ESC was pressed */
        }

        /* Increment reading counter */
        ladd(c, c, one);
    }
    
    /* Clean exit */
    x_clrsc();
    x_curmv(10, 30);
    x_setcol(XC_BYEL);
    puts("Weather Station Terminated");
    x_rstcol();
    x_curmv(12, 35);
    x_setcol(XC_WHT);
    puts("Thank you!");
    x_rstcol();
    x_shwcr();
}

void pr_kv(key, value, start, items, row) int key;
int value;
int start;
int items;
int row;
{
    int i;
    char buffer[50];
    char *data;
    int col;

    /* Display keys row */
    x_curmv(row, 1);
    x_setcol(XC_BLU);
    col = 1;
    for (i = start; i < items; i++)
    {
        data = getport(key, i, buffer, 50);
        x_curmv(row, col);
        puts(data);
        col += 20; /* Fixed column spacing */
    }
    x_rstcol();

    /* Display values row */
    x_curmv(row + 1, 1);
    x_setcol(XC_WHT);
    col = 1;
    for (i = start; i < items; i++)
    {
        data = getport(value, i, buffer, 50);
        x_curmv(row + 1, col);
        puts(data);
        col += 20; /* Fixed column spacing */
    }
    x_rstcol();
}

/* Get data from Intel 8080 IO port */
char *getport(port_num, port_data, buffer, buffer_len)
int port_num;
int port_data;
char *buffer;
int buffer_len;
{
    char ch;
    int index;

    index = 0;

    /* Select data to be read */
    outp(port_num, port_data);

    while ((ch = inp(200)) && index < buffer_len)
    {
        buffer[index++] = ch;
    }
    buffer[index] = 0x00;

    return buffer;
}

/* Clean numeric data - remove non-digit characters */
char *clean_num(src, dest, dest_len) char *src; char *dest; int dest_len;
{
    int i, j;
    
    j = 0;
    for (i = 0; src[i] && j < (dest_len - 1); i++) {
        if ((src[i] >= '0' && src[i] <= '9') || src[i] == '.' || src[i] == '-') {
            dest[j++] = src[i];
        }
    }
    dest[j] = 0x00;
    return dest;
}

/* Draw static layout elements once */
void dr_layout()
{
    /* Main title */
    x_curmv(1, 28);
    x_setcol(XC_BYEL);
    puts("ALTAIR 8800 WEATHER STATION");
    x_curmv(1, 70);
    x_setcol(XC_CYN);
    puts("ESC=Exit");
    x_rstcol();
    
    /* Top border */
    x_curmv(3, 1);
    x_setcol(XC_BLU);
    puts("================================================================================");
    x_rstcol();
    
    /* Section headers */
    x_curmv(5, 1);
    x_setcol(XC_CYN);
    puts("=== LOCATION ===");
    x_rstcol();
    
    x_curmv(9, 1);
    x_setcol(XC_YEL);
    puts("=== CURRENT WEATHER ===");
    x_rstcol();
    
    x_curmv(13, 1);
    x_setcol(XC_MAG);
    puts("=== AIR QUALITY ===");
    x_rstcol();
    
    /* Bottom border */
    x_curmv(19, 1);
    x_setcol(XC_BLU);
    puts("================================================================================");
    x_rstcol();
    
    /* Static footer text */
    x_curmv(20, 1);
    x_setcol(XC_GRN);
    puts("Data published to MQTT Broker");
    x_curmv(20, 35);
    x_setcol(XC_CYN);
    puts("Press ESC to exit");
    x_curmv(20, 55);
    x_setcol(XC_WHT);
    puts("Next update:");
    x_rstcol();
}

/* Update header data only */
void up_head(reading, buffer) char *reading; char *buffer;
{
    char temp[50];
    char output[100];
    
    /* Build reading string */
    ltoa(temp, reading);
    strcpy(output, "Reading: ");
    strcat(output, temp);
    strcat(output, "    ");
    disp_at(2, 1, XC_GRN, output);
    
    /* Build version string */
    strcpy(output, "Altair Ver: ");
    strcat(output, getport(70, 0, buffer, 50));
    strcat(output, "    ");
    disp_at(2, 25, XC_GRN, output);
    
    /* Build time string */
    strcpy(output, "Time: ");
    strcat(output, getport(43, 0, buffer, 50));
    disp_at(2, 50, XC_GRN, output);
    
    x_rstcol();
}

/* Update location data only */
void up_loc()
{
    char buffer[50];
    char output[100];
    
    /* Build latitude string */
    strcpy(output, "  Latitude: ");
    strcat(output, getport(l_value, 0, buffer, 50));
    strcat(output, "        ");
    disp_at(6, 1, XC_WHT, output);
    
    /* Build longitude string */
    strcpy(output, "Longitude: ");
    strcat(output, getport(l_value, 1, buffer, 50));
    strcat(output, "        ");
    disp_at(6, 40, XC_WHT, output);
    
    /* Build country string */
    strcpy(output, "  Country: ");
    strcat(output, getport(l_value, 2, buffer, 50));
    strcat(output, "        ");
    disp_at(7, 1, XC_WHT, output);
    
    /* Build city string */
    strcpy(output, "City: ");
    strcat(output, getport(l_value, 3, buffer, 50));
    strcat(output, "        ");
    disp_at(7, 40, XC_WHT, output);
    
    x_rstcol();
}

/* Update weather data only */
void up_wthr()
{
    char buffer[50];
    char clean[50];
    char output[100];
    
    /* Build temperature string */
    strcpy(output, "  Temperature: ");
    clean_num(getport(w_value, 0, buffer, 50), clean, 50);
    strcat(output, clean);
    strcat(output, "C     ");
    disp_at(10, 1, XC_WHT, output);
    
    /* Build pressure string */
    strcpy(output, "Pressure: ");
    clean_num(getport(w_value, 1, buffer, 50), clean, 50);
    strcat(output, clean);
    strcat(output, " hPa    ");
    disp_at(10, 30, XC_WHT, output);
    
    /* Build humidity string */
    strcpy(output, "Humidity: ");
    clean_num(getport(w_value, 2, buffer, 50), clean, 50);
    strcat(output, clean);
    strcat(output, "%     ");
    disp_at(10, 55, XC_WHT, output);
    
    /* Build wind speed string */
    strcpy(output, "  Wind Speed: ");
    clean_num(getport(w_value, 3, buffer, 50), clean, 50);
    strcat(output, clean);
    strcat(output, " km/h    ");
    disp_at(11, 1, XC_WHT, output);
    
    /* Build wind direction string */
    strcpy(output, "Dir: ");
    clean_num(getport(w_value, 4, buffer, 50), clean, 50);
    strcat(output, clean);
    strcat(output, " deg   ");
    disp_at(11, 30, XC_WHT, output);
    
    /* Build conditions string */
    strcpy(output, "Conditions: ");
    getport(w_value, 5, clean, 50);
    strcat(output, clean);
    strcat(output, "          ");
    disp_at(11, 55, XC_WHT, output);
    
    x_rstcol();
}

/* Update air quality data only */
void up_air()
{
    char buffer[50];
    char clean[50];
    char output[100];
    
    /* AQI needs special color handling */
    x_curmv(14, 1);
    x_setcol(XC_WHT);
    puts("  AQI: ");
    clean_num(getport(p_value, 0, buffer, 50), clean, 50);
    x_setcol(aqi_col(clean));
    puts(clean);
    x_setcol(XC_WHT);
    puts("    ");
    
    /* Build CO string */
    strcpy(output, "  CO: ");
    clean_num(getport(p_value, 1, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(15, 1, XC_WHT, output);
    
    /* Build O3 string */
    strcpy(output, "O3: ");
    clean_num(getport(p_value, 4, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(15, 25, XC_WHT, output);
    
    /* Build NO string */
    strcpy(output, "  NO: ");
    clean_num(getport(p_value, 2, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(16, 1, XC_WHT, output);
    
    /* Build NO2 string */
    strcpy(output, "NO2: ");
    clean_num(getport(p_value, 3, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(16, 25, XC_WHT, output);
    
    /* Build SO2 string */
    strcpy(output, "  SO2: ");
    clean_num(getport(p_value, 5, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(17, 1, XC_WHT, output);
    
    /* Build NH3 string */
    strcpy(output, "NH3: ");
    clean_num(getport(p_value, 6, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(17, 25, XC_WHT, output);
    
    /* Build PM2.5 string */
    strcpy(output, "  PM2.5: ");
    clean_num(getport(p_value, 7, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(18, 1, XC_WHT, output);
    
    /* Build PM10 string */
    strcpy(output, "PM10: ");
    clean_num(getport(p_value, 8, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(18, 25, XC_WHT, output);
    
    x_rstcol();
}

/* Draw the header section with title and status */
void dr_head(reading, buffer) char *reading; char *buffer;
{
    /* Main title */
    x_curmv(1, 28);
    x_setcol(XC_BYEL);
    puts("ALTAIR 8800 WEATHER STATION");
    x_curmv(1, 70);
    x_setcol(XC_CYN);
    puts("ESC=Exit");
    x_rstcol();
    
    /* Status line */
    char temp[50];
    char output[100];
    
    /* Build reading string */
    ltoa(temp, reading);
    strcpy(output, "Reading #");
    strcat(output, temp);
    disp_at(2, 1, XC_GRN, output);
    
    /* Build version string */
    strcpy(output, "Ver: ");
    strcat(output, getport(70, 0, buffer, 50));
    disp_at(2, 25, XC_WHT, output);
    
    /* Build time string */
    strcpy(output, "Time: ");
    strcat(output, getport(43, 0, buffer, 50));
    disp_at(2, 50, XC_GRN, output);
    x_rstcol();
    
    /* Top border */
    x_curmv(3, 1);
    x_setcol(XC_BLU);
    puts("================================================================================");
    x_rstcol();
}

/* Draw location information section */
void dr_loc()
{
    char buffer[50];
    
    x_curmv(5, 1);
    x_setcol(XC_CYN);
    puts("=== LOCATION ===");
    x_rstcol();
    
    /* Location data in a clean format */
    x_curmv(6, 1);
    x_setcol(XC_WHT);
    puts("  Latitude: ");
    puts(getport(l_value, 0, buffer, 50));
    
    x_curmv(6, 40);
    puts("Longitude: ");
    puts(getport(l_value, 1, buffer, 50));
    
    x_curmv(7, 1);
    puts("  Country: ");
    puts(getport(l_value, 2, buffer, 50));
    
    x_curmv(7, 40);
    puts("City: ");
    puts(getport(l_value, 3, buffer, 50));
    x_rstcol();
}

/* Draw weather information section */
void dr_wthr()
{
    char buffer[50];
    char clean[50];
    
    x_curmv(9, 1);
    x_setcol(XC_YEL);
    puts("=== CURRENT WEATHER ===");
    x_rstcol();
    
    /* Weather data in clean format */
    x_curmv(10, 1);
    x_setcol(XC_WHT);
    puts("  Temperature: ");
    clean_num(getport(w_value, 0, buffer, 50), clean, 50);
    puts(clean);
    puts("C");
    
    x_curmv(10, 30);
    puts("Pressure: ");
    clean_num(getport(w_value, 1, buffer, 50), clean, 50);
    puts(clean);
    puts(" hPa");
    
    x_curmv(10, 55);
    puts("Humidity: ");
    clean_num(getport(w_value, 2, buffer, 50), clean, 50);
    puts(clean);
    puts("%");
    
    x_curmv(11, 1);
    puts("  Wind Speed: ");
    clean_num(getport(w_value, 3, buffer, 50), clean, 50);
    puts(clean);
    puts(" km/h");
    
    x_curmv(11, 30);
    puts("Direction: ");
    clean_num(getport(w_value, 4, buffer, 50), clean, 50);
    puts(clean);
    puts(" deg");
    
    x_curmv(11, 55);
    puts("Conditions: ");
    clean_num(getport(w_value, 5, buffer, 50), clean, 50);
    puts(clean);
    x_rstcol();
}

/* Draw air quality section */
void dr_air()
{
    char buffer[50];
    char clean[50];
    
    x_curmv(13, 1);
    x_setcol(XC_MAG);
    puts("=== AIR QUALITY ===");
    x_rstcol();
    
    /* AQI and main pollutants */
    x_curmv(14, 1);
    x_setcol(XC_WHT);
    puts("  AQI: ");
    /* Color-code AQI value based on air quality level */
    x_setcol(aqi_col(getport(p_value, 0, buffer, 50)));
    puts(getport(p_value, 0, buffer, 50));
    x_setcol(XC_WHT);
    
    char output[100];
    
    /* Build CO string */
    strcpy(output, "  CO: ");
    clean_num(getport(p_value, 1, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(15, 1, XC_WHT, output);
    
    /* Build O3 string */
    strcpy(output, "O3: ");
    clean_num(getport(p_value, 4, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(15, 25, XC_WHT, output);
    
    /* Build NO string */
    strcpy(output, "  NO: ");
    clean_num(getport(p_value, 2, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(16, 1, XC_WHT, output);
    
    /* Build NO2 string */
    strcpy(output, "NO2: ");
    clean_num(getport(p_value, 3, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(16, 25, XC_WHT, output);
    
    /* Build SO2 string */
    strcpy(output, "  SO2: ");
    clean_num(getport(p_value, 5, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(17, 1, XC_WHT, output);
    
    /* Build NH3 string */
    strcpy(output, "NH3: ");
    clean_num(getport(p_value, 6, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(17, 25, XC_WHT, output);
    
    /* Build PM2.5 string */
    strcpy(output, "  PM2.5: ");
    clean_num(getport(p_value, 7, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(18, 1, XC_WHT, output);
    
    /* Build PM10 string */
    strcpy(output, "PM10: ");
    clean_num(getport(p_value, 8, buffer, 50), clean, 50);
    strcat(output, clean);
    disp_at(18, 25, XC_WHT, output);
    x_rstcol();
}

/* Update countdown only - returns 1 if ESC pressed */
int dr_foot()
{
    int i;
    int key;
    
    for (i = DELAY; i > 0; i--)
    {
        /* Check for key press */
        if (x_keyrd()) {
            key = x_keyrd();
            if (x_isesc(key)) {
                x_curmv(21, 1);
                x_setcol(XC_YEL);
                puts("Exiting Weather Station...");
                x_rstcol();
                return 1; /* Signal to exit */
            }
        }
        
        /* Update countdown display */
        x_curmv(20, 68);
        x_setcol(XC_WHT);
        x_numpr(i);
        puts("s  ");
        x_rstcol();
        x_delay(0, 1000); /* 1 second delay using timer 0 */
    }
    return 0; /* Continue running */
}

/* Display string at position with color */
void disp_at(row, col, color, text) 
int row; int col; int color; char *text;
{
    x_curmv(row, col);
    x_setcol(color);
    puts(text);
}

/* Get AQI color based on air quality value */
int aqi_col(aqi_str) char *aqi_str;
{
    int aqi;
    int i;
    
    /* Simple string to int conversion */
    aqi = 0;
    for (i = 0; aqi_str[i] >= '0' && aqi_str[i] <= '9'; i++) {
        aqi = aqi * 10 + (aqi_str[i] - '0');
    }
    
    if (aqi <= 50) return XC_GRN;      /* Good - Green */
    if (aqi <= 100) return XC_YEL;     /* Moderate - Yellow */
    if (aqi <= 150) return 33;         /* Unhealthy for sensitive - Orange (Yellow) */
    if (aqi <= 200) return XC_RED;     /* Unhealthy - Red */
    if (aqi <= 300) return XC_MAG;     /* Very Unhealthy - Magenta */
    return 91;                         /* Hazardous - Bright Red */
}