/* ============================================================
 * Chat JSON Handler for Altair 8800 - BDS C  
 * Simple JSON parsing and generation for OpenAI API
 * ============================================================
 */

#include "chatjson.h"

/* String functions - BDS C style */
int strlen();
int strcpy();
int strcat();
int strcmp();

/* Append plain string to buffer with bounds checking */
int j_add(buf, pos, maxlen, text)
char *buf;
int *pos;
int maxlen;
char *text;
{
    int p;
    p = *pos;

    while (*text) {
        if (p >= maxlen - 1) {
            buf[p] = 0;
            return -1;
        }
        buf[p++] = *text++;
    }

    buf[p] = 0;
    *pos = p;
    return 0;
}

/* Append escaped string to buffer with bounds checking */
int j_addesc(buf, pos, maxlen, text)
char *buf;
int *pos;
int maxlen;
char *text;
{
    int p;
    char ch;

    p = *pos;

    while (*text) {
        ch = *text++;

        if (ch == '"' || ch == '\\') {
            if (p >= maxlen - 2) {
                buf[p] = 0;
                return -1;
            }
            buf[p++] = '\\';
            buf[p++] = ch;
        } else if (ch == '\n') {
            if (p >= maxlen - 2) {
                buf[p] = 0;
                return -1;
            }
            buf[p++] = '\\';
            buf[p++] = 'n';
        } else if (ch == '\r') {
            if (p >= maxlen - 2) {
                buf[p] = 0;
                return -1;
            }
            buf[p++] = '\\';
            buf[p++] = 'r';
        } else {
            if (p >= maxlen - 1) {
                buf[p] = 0;
                return -1;
            }
            buf[p++] = ch;
        }
    }

    buf[p] = 0;
    *pos = p;
    return 0;
}

/* Generate OpenAI chat completions request JSON - BDS C compatible */
int j_genreq(sysmsg, types, texts, msgcnt, outbuf, outsize)
char *sysmsg;
int *types;
char **texts;  /* Pointer to array of message strings */
int msgcnt;
char *outbuf;
int outsize;
{
    char *j_buf;
    int j_pos;
    int i;
    int prev_pos;
    char *msg_text;
    char *cfg_model;
    char *cfg_tokens;
    char *cfg_temp;
    
    j_pos = 0;
    
    /* Use output buffer directly */
    j_buf = outbuf;

    cfg_model = ch_getmdl();
    cfg_tokens = ch_gettok();
    cfg_temp = ch_gettmp();
    
    /* Clear buffer first */
    if (outsize > 0)
        j_buf[0] = 0;
    
    /* Build JSON using bounded append helpers */
    if (j_add(j_buf, &j_pos, outsize, "{\"model\":\"") < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, cfg_model) < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, "\",\"messages\":[") < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, "{\"role\":\"system\",\"content\":\"") < 0)
        return -1;
    
    /* Manually escape and add system message */
    /* printf("j_genreq: About to add sysmsg: '%.100s...'\n", sysmsg); */
    
    /* Simple JSON escaping for system message */
    if (j_addesc(j_buf, &j_pos, outsize, sysmsg) < 0) {
        /* printf("j_genreq: System message truncated due to buffer limit\n"); */
        j_buf[j_pos] = 0;
    }
    if (j_add(j_buf, &j_pos, outsize, "\"}") < 0)
        return -1;
    
    /* Add conversation messages */
    for (i = 0; i < msgcnt; i++) {
        /* Locate message pointer */
        msg_text = *(texts + i);
        if (msg_text == 0)
            msg_text = "";
        
        /* Debug: show message being processed */
        /* printf("j_genreq: Processing message %d, type=%d, text='%s'\n", i, types[i], msg_text); */
        
        prev_pos = j_pos;
        if (j_add(j_buf, &j_pos, outsize, ",{\"role\":\"") < 0) {
            j_pos = prev_pos;
            j_buf[j_pos] = 0;
            /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
            continue;
        }
        
        switch (types[i]) {
        case MSG_USR:
            if (j_add(j_buf, &j_pos, outsize, "user") < 0) {
                j_pos = prev_pos;
                j_buf[j_pos] = 0;
                /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
                continue;
            }
            break;
        case MSG_AST:
            if (j_add(j_buf, &j_pos, outsize, "assistant") < 0) {
                j_pos = prev_pos;
                j_buf[j_pos] = 0;
                /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
                continue;
            }
            break;
        default:
            if (j_add(j_buf, &j_pos, outsize, "user") < 0) {
                j_pos = prev_pos;
                j_buf[j_pos] = 0;
                /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
                continue;
            }
            break;
        }
        
        if (j_add(j_buf, &j_pos, outsize, "\",\"content\":\"") < 0) {
            j_pos = prev_pos;
            j_buf[j_pos] = 0;
            /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
            continue;
        }
        
        /* Simple JSON escaping for message content */
        if (j_addesc(j_buf, &j_pos, outsize, msg_text) < 0) {
            j_pos = prev_pos;
            j_buf[j_pos] = 0;
            /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
            continue;
        }
        if (j_add(j_buf, &j_pos, outsize, "\"}") < 0) {
            j_pos = prev_pos;
            j_buf[j_pos] = 0;
            /* printf("j_genreq: Skipping message %d due to buffer limit\n", i); */
            continue;
        }
    }
    
    /* Close JSON with streaming enabled and configured params */
    if (j_add(j_buf, &j_pos, outsize, "],\"max_tokens\":") < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, cfg_tokens) < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, ",\"temperature\":") < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, cfg_temp) < 0)
        return -1;
    if (j_add(j_buf, &j_pos, outsize, ",\"stream\":true}") < 0)
        return -1;
    
    /* Get final length */
    j_pos = strlen(j_buf);
    
    /* Debug: show buffer usage */
    /* printf("j_genreq: Used %d/%d bytes in internal buffer\n", j_pos, outsize); */
    /* printf("j_genreq: Final JSON: '%.200s...'\n", j_buf); */
    
    return j_pos;
}

/* Parse OpenAI response and extract message content */
int j_parse(jsonstr, outbuf, outsize)
char *jsonstr;
char *outbuf;
int outsize;
{
    char *ptr;
    char *start;
    int i, len;
    
    /* Look for "content":" in the response */
    ptr = jsonstr;
    while (*ptr) {
        if (j_match2(ptr, "\"content\":\"")) {
            ptr += 11; /* Skip past "content":" */
            start = ptr;
            
            /* Find end of string */
            while (*ptr && *ptr != '"') {
                if (*ptr == '\\' && *(ptr + 1)) {
                    ptr += 2; /* Skip escaped character */
                } else {
                    ptr++;
                }
            }
            
            /* Copy content to output buffer */
            len = ptr - start;
            if (len >= outsize)
                len = outsize - 1;
                
            for (i = 0; i < len; i++) {
                outbuf[i] = start[i];
            }
            outbuf[i] = 0;
            
            /* Unescape the content */
            j_unesc(outbuf);
            
            return len;
        }
        ptr++;
    }
    
    /* No content found */
    strcpy(outbuf, "No response found");
    return -1;
}

/* Match string at pointer */
int j_match2(ptr, match)
char *ptr;
char *match;
{
    while (*match) {
        if (*ptr != *match)
            return 0;
        ptr++;
        match++;
    }
    return 1;
}

/* Unescape JSON string in place */
int j_unesc(str)
char *str;
{
    char *src, *dst;
    
    src = dst = str;
    
    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            src++; /* Skip backslash */
            switch (*src) {
            case '"':
                *dst++ = '"';
                break;
            case '\\':
                *dst++ = '\\';
                break;
            case 'n':
                *dst++ = '\n';
                break;
            case 'r':
                *dst++ = '\r';
                break;
            case 't':
                *dst++ = '\t';
                break;
            default:
                *dst++ = *src;
                break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = 0;
    
    return 0;
}

/* Skip whitespace */
int j_skip(ptr)
char **ptr;
{
    while (**ptr == ' ' || **ptr == '\t' || **ptr == '\n' || **ptr == '\r') {
        (*ptr)++;
    }
    return 0;
}
