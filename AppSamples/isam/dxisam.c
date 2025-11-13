#include "stdio.h"
#include "dxisam.h"

#ifndef DX_COMMON_LINKAGE
struct i_db g_cfg;
#endif

int i_cfwr(fname)
char *fname;
{
    int fd;
    int i, j, len;
    char buf[I_BUFSZ];
    char num[10];
    char *p;
    
    printf("[i_cfwr] Writing config to: %s\r\n", fname);
    
    fd = creat(fname);
    if (fd == ERROR)
    {
        puts("[i_cfwr] ERROR: Cannot create config file");
        return I_EOPEN;
    }
    
    p = buf;
    
    /* Write database name */
    i = 0;
    while (g_cfg.dbname[i] && i < I_MXNM)
        *p++ = g_cfg.dbname[i++];
    *p++ = '\n';
    
    /* Write number of tables */
    i = g_cfg.ntbls;
    j = 0;
    if (i == 0)
        num[j++] = '0';
    else
    {
        len = 0;
        while (i > 0)
        {
            num[len++] = (i % 10) + '0';
            i = i / 10;
        }
        for (j = len - 1; j >= 0; j--)
            *p++ = num[j];
    }
    *p++ = '\n';
    
    printf("[i_cfwr] db=%s ntbls=%d\r\n", g_cfg.dbname, g_cfg.ntbls);
    
    /* Write each table */
    for (i = 0; i < g_cfg.ntbls && i < I_MXTBL; i++)
    {
        printf("[i_cfwr] Writing table %d: %s\r\n", i, g_cfg.tbls[i].name);
        
        /* Table name */
        j = 0;
        while (g_cfg.tbls[i].name[j] && j < I_MXNM)
            *p++ = g_cfg.tbls[i].name[j++];
        *p++ = '\n';
        
        /* Disk */
        *p++ = g_cfg.tbls[i].disk;
        *p++ = '\n';
        
        /* Write integers with helper function */
        p = i_wrint(p, g_cfg.tbls[i].recsz);
        p = i_wrint(p, g_cfg.tbls[i].nkeys);
        p = i_wrint(p, g_cfg.tbls[i].nrecs);
        p = i_wrint(p, g_cfg.tbls[i].maxrec);
        
        /* Key offsets */
        for (j = 0; j < g_cfg.tbls[i].nkeys && j < I_MXKEY; j++)
            p = i_wrint(p, g_cfg.tbls[i].keyoff[j]);
        
        /* Key sizes */
        for (j = 0; j < g_cfg.tbls[i].nkeys && j < I_MXKEY; j++)
            p = i_wrint(p, g_cfg.tbls[i].keysz[j]);
    }
    
    /* Write buffer to file */
    len = p - buf;
    i = (len + I_SECSZ - 1) / I_SECSZ;
    if (write(fd, buf, i) != i)
    {
        close(fd);
        puts("[i_cfwr] ERROR: Write failed");
        return I_EWRIT;
    }
    
    close(fd);
    puts("[i_cfwr] Config written successfully");
    return I_OK;
}

/* Helper: write integer to buffer as decimal string with newline */
char *i_wrint(p, val)
char *p;
int val;
{
    int i, len;
    char num[10];
    
    if (val == 0)
    {
        *p++ = '0';
        *p++ = '\n';
        return p;
    }
    
    len = 0;
    while (val > 0)
    {
        num[len++] = (val % 10) + '0';
        val = val / 10;
    }
    
    for (i = len - 1; i >= 0; i--)
        *p++ = num[i];
    *p++ = '\n';
    
    return p;
}

int i_cfrd(fname)
char *fname;
{
    int fd, i, t, nsecs;
    char *p;
    char *pend;
    char buf[I_BUFSZ];
    
    fd = open(fname, 0);
    if (fd == ERROR)
        return I_EOPEN;
    
    /* Read file into buffer */
    nsecs = read(fd, buf, I_NSECTS);
    if (nsecs <= 0)
    {
        close(fd);
        return I_EOPEN;
    }
    
    close(fd);
    
    p = buf;
    pend = buf + (nsecs * I_SECSZ);
    
    /* Read dbname */
    i = 0;
    while (p < pend && *p != '\n' && i < I_MXNM - 1)
    {
        g_cfg.dbname[i] = *p;
        i++;
        p++;
    }
    g_cfg.dbname[i] = 0;
    if (p < pend && *p == '\n')
        p++;
    
    /* Read ntbls */
    g_cfg.ntbls = 0;
    while (p < pend && *p >= '0' && *p <= '9')
    {
        g_cfg.ntbls = g_cfg.ntbls * 10 + (*p - '0');
        p++;
    }
    if (p < pend && *p == '\n')
        p++;
    
    /* Read each table */
    for (t = 0; t < g_cfg.ntbls && t < I_MXTBL; t++)
    {
        /* Table name */
        i = 0;
        while (p < pend && *p != '\n' && i < I_MXNM - 1)
        {
            g_cfg.tbls[t].name[i] = *p;
            i++;
            p++;
        }
        g_cfg.tbls[t].name[i] = 0;
        if (p < pend && *p == '\n')
            p++;
        
        /* Disk */
        if (p >= pend)
            break;
        g_cfg.tbls[t].disk = *p++;
        if (p < pend && *p == '\n')
            p++;
        
        /* Record size, keys, counts */
        p = i_rdint(p, pend, &g_cfg.tbls[t].recsz);
        p = i_rdint(p, pend, &g_cfg.tbls[t].nkeys);
        p = i_rdint(p, pend, &g_cfg.tbls[t].nrecs);
        p = i_rdint(p, pend, &g_cfg.tbls[t].maxrec);
        
        /* Key offsets */
        for (i = 0; i < g_cfg.tbls[t].nkeys && i < I_MXKEY; i++)
            p = i_rdint(p, pend, &g_cfg.tbls[t].keyoff[i]);
        
        /* Key sizes */
        for (i = 0; i < g_cfg.tbls[t].nkeys && i < I_MXKEY; i++)
            p = i_rdint(p, pend, &g_cfg.tbls[t].keysz[i]);
    }
    
    printf("[i_cfrd] Loaded config: db=%s ntbls=%d\r\n", g_cfg.dbname, g_cfg.ntbls);
    return I_OK;
}

char *i_rdint(p, pend, val)
char *p;
char *pend;
int *val;
{
    *val = 0;
    while (p < pend && *p >= '0' && *p <= '9')
    {
        *val = (*val * 10) + (*p - '0');
        p++;
    }
    if (p < pend && *p == '\n')
        p++;
    return p;
}
