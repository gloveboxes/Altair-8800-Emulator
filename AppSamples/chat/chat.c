/* ============================================================
 * Chat App for Altair 8800 - BDS C
 * Connects to OpenAI Chat Completions API via Intel 8080 ports
 * ============================================================
 */

#include <stdio.h>
#include "dxterm.h"
#include "chatjson.h"

#define CHAT_VERSION "1.5"

/* Message types */
#define MSG_SYS 0
#define MSG_USR 1
#define MSG_AST 2

/* Limits */
#define MAX_MSG 10
#define SYS_LEN 1024
#define REQ_LEN 8192
#define CFG_LINE 80
#define CFG_VAL 16
#define CFG_MLEN 32

/* OpenAI Status codes (like webget status codes) */
#define OPENAI_EOF 0
#define OPENAI_WAITING 1
#define OPENAI_DATA_READY 2

/* Message structure */
struct msg_s
{
    int type;
    char text[MSG_LEN];
};

/* Global chat context - simplified for BDS C */
char g_sysmsg[SYS_LEN];
char g_mtok[CFG_VAL];
char g_tempv[CFG_VAL];
char g_model[CFG_MLEN];
int g_types[MAX_MSG]; /* Renamed to avoid 7-char limit clash */
int g_msgcnt;
int g_cursor; /* Fixed spelling and length */

/* Shared request/response buffers */
char g_req[REQ_LEN];
char g_resp[AST_LEN];

/* Message storage pools */
#define MAX_AST ((MAX_MSG + 1) / 2)
char g_umsg[MAX_MSG][USR_LEN];
char g_amem[MAX_AST][AST_LEN];
int g_uuse[MAX_MSG];
int g_ause[MAX_AST];
char *g_mptr[MAX_MSG];
int g_umap[MAX_MSG];
int g_amap[MAX_MSG];

/* Function declarations */
int ch_init();
int ch_load();
int ch_menu();
int ch_chat();
int ch_addm();
int ch_show();
int ch_clear();
int ch_api();
int ch_recv();
int ch_copy();
int ch_print();
int ch_gus();
int ch_fus();
int ch_gas();
int ch_fas();
int ch_loadcfg();
int ch_cfgln();
int ch_settok();
int ch_settmp();
int ch_setmdl();

/* String functions */
int strlen();
int strcpy();
int strcat();
int strcmp();

/* I/O port functions */
int outp();

main()
{
    int choice;

    x_clrsc();

    /* Initialize chat context */
    if (ch_init() < 0)
    {
        printf("Error: Failed to initialize\n");
        return 1;
    }

    /* Load system message */
    if (ch_load() < 0)
    {
        return 1;
    }

    /* Main loop */
    while (1)
    {
        choice = ch_menu();

        switch (choice)
        {
        case 1:
            ch_chat();
            break;
        case 2:
            ch_show();
            printf("Press any key to continue...");
            x_conin();
            break;
        case 3:
            ch_clear();
            printf("\nPress any key to continue...");
            x_conin();
            break;
        case 0:
            return 0;
            break;
        default:
            printf("Invalid choice\n");
            break;
        }

        /* printf("Press any key to continue...");
        x_conin();
        printf("\n"); */
    }

    return 0;
}

/* Initialize chat system */
int ch_init()
{
    int i;

    /* Clear system message */
    g_sysmsg[0] = 0;
    strcpy(g_mtok, "512");
    strcpy(g_tempv, "0.2");
    strcpy(g_model, "gpt-4o-mini");

    /* Clear message arrays */
    g_msgcnt = 0;
    g_cursor = 0;

    for (i = 0; i < MAX_MSG; i++)
    {
        g_types[i] = 0;
        g_mptr[i] = 0;
        g_umap[i] = -1;
        g_amap[i] = -1;
        g_uuse[i] = 0;
    }

    for (i = 0; i < MAX_AST; i++)
    {
        g_ause[i] = 0;
    }

    return 0;
}

/* Load system message from chat.sys */
int ch_load()
{
    FILE *fp;
    int ch;
    int idx;

    fp = fopen("chat.sys", "r");
    if (fp == 0)
    {
        printf("Error: Missing chat system instruction file 'chat.sys'\n");
        return -1;
    }

    idx = 0;
    while (idx < SYS_LEN - 1 && (ch = fgetc(fp)) != EOF)
    {
        if (ch == 26)
        {
            break;
        }

        /* Ensure stored system text stays in 7-bit ASCII */
        g_sysmsg[idx++] = ch & 0x7F;
    }
    g_sysmsg[idx] = 0;

    fclose(fp);

    /* Load optional config (uses defaults if missing) */
    ch_loadcfg();

    return 0;
}

/* Parse chat.cfg for optional parameters */
int ch_loadcfg()
{
    FILE *fp;
    int ch;
    int idx;
    char line[CFG_LINE];

    fp = fopen("chat.cfg", "r");
    if (fp == 0)
    {
        return 0;
    }

    idx = 0;
    while ((ch = fgetc(fp)) != EOF)
    {
        if (ch == 26)
        {
            break;
        }

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            line[idx] = 0;
            if (idx > 0)
                ch_cfgln(line);
            idx = 0;
            continue;
        }

        if (idx < CFG_LINE - 1)
        {
            line[idx++] = ch & 0x7F;
        }
    }

    if (idx > 0)
    {
        line[idx] = 0;
        ch_cfgln(line);
    }

    fclose(fp);

    return 0;
}

/* Handle a single config line */
int ch_cfgln(line)
char *line;
{
    char key[CFG_VAL];
    char val[CFG_LINE];
    char *ptr;
    int i;

    ptr = line;

    /* Skip leading whitespace */
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;

    if (*ptr == 0 || *ptr == '#')
        return 0;

    i = 0;
    while (*ptr && *ptr != '=' && *ptr != ' ' && *ptr != '\t')
    {
        if (i < CFG_VAL - 1)
            key[i++] = *ptr;
        ptr++;
    }
    key[i] = 0;

    while (*ptr && *ptr != '=')
        ptr++;
    if (*ptr != '=')
        return 0;
    ptr++;

    while (*ptr == ' ' || *ptr == '\t')
        ptr++;

    i = 0;
    if (strcmp(key, "model") == 0)
    {
        /* For model, allow full line except comment */
        while (*ptr && *ptr != '#' && *ptr != '\n' && *ptr != '\r')
        {
            if (i < CFG_LINE - 1)
                val[i++] = *ptr;
            ptr++;
        }
    }
    else
    {
        while (*ptr && *ptr != '#' && *ptr != '\n' && *ptr != '\r')
        {
            if (*ptr == ' ' || *ptr == '\t')
                break;
            if (i < CFG_LINE - 1)
                val[i++] = *ptr;
            ptr++;
        }
    }
    val[i] = 0;

    if (val[0] == 0)
        return 0;

    if (strcmp(key, "max_tokens") == 0)
        ch_settok(val);
    else if (strcmp(key, "temperature") == 0)
        ch_settmp(val);
    else if (strcmp(key, "model") == 0)
        ch_setmdl(val);

    return 0;
}

/* Validate and store max_tokens */
int ch_settok(val)
char *val;
{
    int i;
    int j;
    char tmp[CFG_VAL];

    j = 0;
    for (i = 0; val[i]; i++)
    {
        if (val[i] >= '0' && val[i] <= '9')
        {
            if (j < CFG_VAL - 1)
                tmp[j++] = val[i];
        }
        else
        {
            break;
        }
    }
    tmp[j] = 0;

    if (j > 0)
        strcpy(g_mtok, tmp);

    return 0;
}

/* Validate and store temperature */
int ch_settmp(val)
char *val;
{
    int i;
    int j;
    int dot;
    char tmp[CFG_VAL];

    j = 0;
    dot = 0;
    for (i = 0; val[i]; i++)
    {
        if (val[i] >= '0' && val[i] <= '9')
        {
            if (j < CFG_VAL - 1)
                tmp[j++] = val[i];
        }
        else if (val[i] == '.' && dot == 0)
        {
            if (j < CFG_VAL - 1)
                tmp[j++] = val[i];
            dot = 1;
        }
        else
        {
            break;
        }
    }

    while (j > 0 && tmp[j - 1] == '.')
        j--;
    tmp[j] = 0;

    if (j > 0)
        strcpy(g_tempv, tmp);

    return 0;
}

char *ch_gettok()
{
    return g_mtok;
}

char *ch_gettmp()
{
    return g_tempv;
}

char *ch_getmdl()
{
    return g_model;
}

/* Validate and store model name */
int ch_setmdl(val)
char *val;
{
    int i;
    int j;
    int ch;
    char tmp[CFG_MLEN];

    j = 0;
    for (i = 0; val[i]; i++)
    {
        ch = val[i] & 0x7F;
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '/')
        {
            if (j < CFG_MLEN - 1)
                tmp[j++] = ch;
        }
        else
        {
            break;
        }
    }
    tmp[j] = 0;

    if (j > 0)
        strcpy(g_model, tmp);

    return 0;
}

/* Display main menu */
int ch_menu()
{
    int choice;

    x_clrsc();
    x_curmv(1, 1);

    printf("Altair 8800 Chat App v%s\n", CHAT_VERSION);
    printf("=========================\n\n");
    printf("1. Start Chat\n");
    printf("2. Show Messages\n");
    printf("3. Clear History\n");
    printf("0. Quit\n\n");
    printf("Choice: ");

    choice = x_conin() - '0';
    /* printf("%d\n\n", choice); */

    return choice;
}

/* Main chat interface */
int ch_chat()
{
    char input[USR_LEN];

    x_clrsc();
    printf("=== Chat Session ===\n");
    printf("Type 'quit' to exit, 'clear' to clear screen\n");
    printf("System message:\n%s\n\n", g_sysmsg);


    while (1)
    {
        /* Show prompt */
        x_setcol(XC_GRN);
        printf("You: ");
        x_rstcol();

        /* Get user input - simple gets() */
        gets(input);

        /* Check for commands */
        if (strcmp(input, "quit") == 0)
        {
            break;
        }
        else if (strcmp(input, "clear") == 0)
        {
            x_clrsc();
            printf("=== Chat Session ===\n\n");
            continue;
        }
        else if (strcmp(input, "") == 0)
        {
            continue;
        }

        /* Add user message */
        ch_addm(MSG_USR, input);

        /* Simulate API call */
        printf("\n");
        x_setcol(XC_CYN);
        printf("Assistant: \n");
        x_rstcol();

        ch_api();

        printf("\n\n");
    }

    return 0;
}

/* Add message to history */
int ch_addm(type, text)
int type;
char *text;
{
    int idx;
    char *type_name;
    int limit;
    int slot;
    int i;

    /* Debug: show what we're adding */
    type_name = (type == MSG_USR) ? "USER" : (type == MSG_AST) ? "ASSISTANT"
                                                               : "UNKNOWN";
    /* printf("DEBUG ch_addm: Adding %s (%d): '%.50s%s'\n", type_name, type, text, strlen(text) > 50 ? "..." : ""); */

    if (g_msgcnt >= MAX_MSG)
    {
        /* Shift messages down */
        /* printf("DEBUG: Shifting messages (full)\n"); */

        if (g_types[0] == MSG_USR)
            ch_fus(g_umap[0]);
        else if (g_types[0] == MSG_AST)
            ch_fas(g_amap[0]);

        for (i = 0; i < MAX_MSG - 1; i++)
        {
            g_types[i] = g_types[i + 1];
            g_mptr[i] = g_mptr[i + 1];
            g_umap[i] = g_umap[i + 1];
            g_amap[i] = g_amap[i + 1];
        }
        g_types[MAX_MSG - 1] = 0;
        g_mptr[MAX_MSG - 1] = 0;
        g_umap[MAX_MSG - 1] = -1;
        g_amap[MAX_MSG - 1] = -1;
        g_msgcnt = MAX_MSG - 1;
    }

    idx = g_msgcnt;
    g_types[idx] = type;
    g_mptr[idx] = 0;
    g_umap[idx] = -1;
    g_amap[idx] = -1;

    if (type == MSG_AST)
    {
        limit = AST_LEN;
        slot = ch_gas();
        if (slot < 0)
        {
            printf("No assistant slots available\n");
            return -1;
        }
        ch_copy(g_amem[slot], text, limit);
        g_mptr[idx] = g_amem[slot];
        g_amap[idx] = slot;
    }
    else
    {
        limit = USR_LEN;
        slot = ch_gus();
        if (slot < 0)
        {
            printf("No user slots available\n");
            return -1;
        }
        ch_copy(g_umsg[slot], text, limit);
        g_mptr[idx] = g_umsg[slot];
        g_umap[idx] = slot;
    }
    g_msgcnt++;

    return 0;
}

/* Show message history */
int ch_show()
{
    int i;

    x_clrsc();
    printf("=== Message History ===\n\n");

    /* Show system message */
    x_setcol(XC_YEL);
    printf("System: %s\n\n", g_sysmsg);
    x_rstcol();

    /* Show messages */
    for (i = 0; i < g_msgcnt; i++)
    {
        switch (g_types[i])
        {
        case MSG_USR:
            x_setcol(XC_GRN);
            printf("You: ");
            break;
        case MSG_AST:
            x_setcol(XC_CYN);
            printf("Assistant: ");
            break;
        default:
            printf("Unknown: ");
            break;
        }

        if (g_mptr[i])
            ch_print(g_mptr[i]);
        printf("\n");
        x_rstcol();
    }

    printf("\n");
    return 0;
}

/* Clear message history */
int ch_clear()
{
    int i;

    for (i = 0; i < g_msgcnt; i++)
    {
        if (g_types[i] == MSG_USR)
            ch_fus(g_umap[i]);
        else if (g_types[i] == MSG_AST)
            ch_fas(g_amap[i]);
        g_types[i] = 0;
        g_mptr[i] = 0;
        g_umap[i] = -1;
        g_amap[i] = -1;
    }
    g_msgcnt = 0;
    printf("\nMessage history cleared\n");
    return 0;
}

/* Simulate OpenAI API call */
int ch_api()
{
    int reqlen, resplen;
    int i;
    char *dbg;

    /* Debug: show system message and queued messages */
    /*
    printf("System message: %s\n", g_sysmsg);
    printf("System message length: %d\n", strlen(g_sysmsg));
        */
    /* printf("DEBUG: g_msgcnt = %d\n", g_msgcnt); */
    /*
    for (i = 0; i < g_msgcnt; i++) {
        dbg = g_mptr[i];
        if (dbg == 0)
            dbg = "";
        printf("DEBUG: Array[%d] type=%d text='%.50s%s'\n", i, g_types[i], dbg, strlen(dbg) > 50 ? "..." : "");
        printf("Message %d (%s): %s\n", i,
               g_types[i] == MSG_USR ? "user" :
               g_types[i] == MSG_AST ? "assistant" : "unknown",
               dbg);
    }
    printf("\n");
    */

    /* Generate JSON request payload */
    reqlen = j_genreq(g_sysmsg, g_types, g_mptr, g_msgcnt, g_req, REQ_LEN);
    if (reqlen < 0)
    {
        printf("Error: JSON too large for buffer\n");
        return -1;
    }

    /* Debug: show JSON prior to sending */
    /* printf("Generated JSON payload (%d bytes):\n%s\n\n", reqlen, g_req); */

    /* Send JSON payload to Altair emulator via I/O ports */
    /* printf("Sending to OpenAI via Altair emulator...\n"); */

    /* Reset OpenAI request buffer */
    outp(120, 1);

    /* Reset OpenAI response buffer */
    outp(122, 1);

    /* Small delay to ensure reset completes */
    /* x_delay(0, 10); */

    /* Send each character of JSON payload */
    for (i = 0; i < reqlen; i++)
    {
        outp(121, g_req[i]);
    }

    /* Send null terminator to trigger OpenAI call */
    outp(121, 0);

    /* Trigger the API call */
    inp(120);

    /* printf("JSON payload sent to emulator.\n\n"); */

    /* Receive streaming response (already contains plain text content) */
    resplen = ch_recv(g_resp, AST_LEN, 1);

    if (resplen > 0)
    {
        /* For streaming, response already contains the content text */
        /* Copy to content buffer with length check */
        if (resplen < AST_LEN)
        {
            /* Add assistant response to history */
            ch_addm(MSG_AST, g_resp);

            /* Ensure trailing newline for stream output */
            if (resplen > 0 && g_resp[resplen - 1] != '\n')
                printf("\n");
        }
        else
        {
            printf("Response too long for buffer\n");
        }
    }
    else
    {
        printf("No response received\n");
    }

    return 0;
}

/* Receive streaming response content via I/O ports */
int ch_recv(buffer, bufsize, echo)
char *buffer;
int bufsize;
int echo;
{
    int status, ch, pos, timeout;

    /* Initialize variables */
    pos = 0;
    timeout = 0;

    /* Clear buffer first */
    buffer[0] = 0;

    /* Read streaming response content chunks */
    while (pos < bufsize - 1)
    {
        /* Check if data is available (port 123 status) */
        status = inp(123);

        if (status == 2)
        { /* OPENAI_DATA_READY */
            /* Read all available characters from current chunk */
            while (status == 2 && pos < bufsize - 1)
            {
                ch = inp(124) & 0x7F;
                buffer[pos++] = ch;

                if (echo)
                {
                    if (ch == '\n')
                    {
                        x_conout('\r');
                        x_conout('\n');
                    }
                    else
                    {
                        x_conout(ch);
                    }
                }

                /* Check if more data available in current chunk */
                status = inp(123);
            }
            timeout = 0;
        }
        else if (status == 0)
        { /* OPENAI_EOF - stream complete */
            break;
        }
        else
        {
            /* OPENAI_WAITING - small delay then check again */
            timeout++;
            if (timeout > 3000)
            { /* Timeout after waiting */
                break;
            }
            x_delay(0, 10); /* Small delay */
        }
    }

    buffer[pos] = 0; /* Null terminate */
    return pos;
}

/* Copy with length guard */
int ch_copy(dst, src, max)
char *dst;
char *src;
int max;
{
    int cnt;
    char *d;
    char *s;

    if (max <= 0)
        return 0;

    d = dst;
    s = src;
    cnt = 0;

    while (*s && cnt < max - 1)
    {
        *d++ = *s++;
        cnt++;
    }

    *d = 0;
    return cnt;
}

/* Allocate user slot */
int ch_gus()
{
    int i;

    for (i = 0; i < MAX_MSG; i++)
    {
        if (g_uuse[i] == 0)
        {
            g_uuse[i] = 1;
            return i;
        }
    }
    return -1;
}

/* Free user slot */
int ch_fus(slot)
int slot;
{
    if (slot >= 0 && slot < MAX_MSG)
        g_uuse[slot] = 0;
    return 0;
}

/* Allocate assistant slot */
int ch_gas()
{
    int i;

    for (i = 0; i < MAX_AST; i++)
    {
        if (g_ause[i] == 0)
        {
            g_ause[i] = 1;
            return i;
        }
    }
    return -1;
}

/* Free assistant slot */
int ch_fas(slot)
int slot;
{
    if (slot >= 0 && slot < MAX_AST)
        g_ause[slot] = 0;
    return 0;
}

/* Print string without truncation */
int ch_print(text)
char *text;
{
    char ch;

    while ((ch = *text++) != 0)
    {
        if (ch == '\n')
            x_conout('\r');
        x_conout(ch);
    }
    return 0;
}
