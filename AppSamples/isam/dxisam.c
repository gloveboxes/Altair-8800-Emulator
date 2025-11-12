/* DXISAM2.C - ISAM Library V2 with Table Support - Low-Level I/O */
#include "stdio.h"
#include "dxisam.h"

struct i_db g_cfg;

/* Low-level I/O buffer size in sectors */
#define I_NSECTS 8
#define I_SECSZ 128
#define I_BUFSZ (I_NSECTS * I_SECSZ)

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
    int i, j, len;
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
    int fd, ch, i, j, t, nsecs;
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
        
        /* Recsz */
    p = i_rdint(p, pend, &g_cfg.tbls[t].recsz);
        
        /* Nkeys */
    p = i_rdint(p, pend, &g_cfg.tbls[t].nkeys);
        
        /* Nrecs */
    p = i_rdint(p, pend, &g_cfg.tbls[t].nrecs);
        
        /* Maxrec */
    p = i_rdint(p, pend, &g_cfg.tbls[t].maxrec);
        
        /* Key offsets */
        for (j = 0; j < g_cfg.tbls[t].nkeys && j < I_MXKEY; j++)
            p = i_rdint(p, pend, &g_cfg.tbls[t].keyoff[j]);
        
        /* Key sizes */
        for (j = 0; j < g_cfg.tbls[t].nkeys && j < I_MXKEY; j++)
            p = i_rdint(p, pend, &g_cfg.tbls[t].keysz[j]);
    }
    
    return I_OK;
}

/* Helper: read integer from buffer */
char *i_rdint(p, pend, val)
char *p;
char *pend;
int *val;
{
    *val = 0;
    while (p < pend && *p >= '0' && *p <= '9')
    {
        *val = (*val) * 10 + (*p - '0');
        p++;
    }
    if (p < pend && *p == '\n')
        p++;
    return p;
}

/* Create table data file - just creates empty file for now */
int i_mktbl(tblnam)
char *tblnam;
{
    int fd;
    int i, j;
    char fname[20];
    
    printf("[i_mktbl] Looking for table: %s\r\n", tblnam);
    printf("[i_mktbl] ntbls=%d\r\n", g_cfg.ntbls);
    
    /* Find table in config */
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        printf("[i_mktbl] Checking table %d: %s\r\n", i, g_cfg.tbls[i].name);
        
        /* Simple string compare */
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
            break;
    }
    
    if (i >= g_cfg.ntbls)
    {
        puts("[i_mktbl] ERROR: Table not found in config");
        return I_ENTBL;
    }
    
    printf("[i_mktbl] Found table at index %d\r\n", i);
    printf("[i_mktbl] disk=%c recsz=%d nkeys=%d\r\n",
        g_cfg.tbls[i].disk, g_cfg.tbls[i].recsz, g_cfg.tbls[i].nkeys);
    
    /* Build filename: disk:name.DAT (e.g., A:CUSTOMER.DAT) */
    fname[0] = g_cfg.tbls[i].disk;
    fname[1] = ':';
    j = 0;
    while (tblnam[j] && j < 8)
    {
        fname[j + 2] = tblnam[j];
        j++;
    }
    fname[j + 2] = '.';
    fname[j + 3] = 'D';
    fname[j + 4] = 'A';
    fname[j + 5] = 'T';
    fname[j + 6] = 0;
    
    printf("[i_mktbl] Creating file: %s\r\n", fname);
    
    /* Create empty file */
    fd = creat(fname);
    if (fd == ERROR)
    {
        printf("[i_mktbl] ERROR: creat failed for %s\r\n", fname);
        return I_EOPEN;
    }
    
    close(fd);
    puts("[i_mktbl] File created successfully");
    return I_OK;
}

/* Insert record - append to table data file */
int i_insrt(tblnam, rec, rsiz)
char *tblnam;
char *rec;
int rsiz;
{
    int fd;
    int i, j, k, tsz, nsecs, total;
    int phys;
    int reuse;
    int reuse_phys;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    /* Find table in config */
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        /* Simple string compare */
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            /* Build filename first */
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            /* Verify record size matches table definition */
            tsz = g_cfg.tbls[i].recsz;
            if (rsiz != tsz)
            {
                printf("[i_insrt] ERROR: Size mismatch rsiz=%d tsz=%d\r\n", rsiz, tsz);
                return I_ESIZE;
            }
            
            /* Open for read/write */
            fd = open(fname, 2);
            if (fd == ERROR)
            {
                printf("[i_insrt] ERROR: Cannot open %s for append\r\n", fname);
                return I_EOPEN;
            }
            
            /* Convert record size to sectors */
            nsecs = (rsiz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                printf("[i_insrt] ERROR: Record too large for buffer\r\n");
                return I_ESIZE;
            }
            total = nsecs * I_SECSZ;
            
            /* Look for a deleted slot to reuse */
            reuse = 0;
            reuse_phys = -1;
            if (g_cfg.tbls[i].nrecs < g_cfg.tbls[i].maxrec)
            {
                for (phys = 0; phys < g_cfg.tbls[i].maxrec; phys++)
                {
                    if (seek(fd, phys * nsecs, 0) == ERROR)
                    {
                        reuse = 0;
                        break;
                    }
                    if (read(fd, sbuf, nsecs) < nsecs)
                        break;
                    if (sbuf[0] == I_DELFLAG)
                    {
                        reuse = 1;
                        reuse_phys = phys;
                        break;
                    }
                }
            }
            
            if (reuse)
            {
                if (seek(fd, reuse_phys * nsecs, 0) == ERROR)
                {
                    close(fd);
                    return I_EWRIT;
                }
            }
            else
            {
                if (seek(fd, 0, 2) == ERROR)
                {
                    close(fd);
                    return I_EWRIT;
                }
            }
            
            for (k = 0; k < rsiz && k < total; k++)
                sbuf[k] = rec[k];
            while (k < total)
            {
                sbuf[k] = 0;
                k++;
            }
            if (write(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                printf("[i_insrt] ERROR: Write failed\r\n");
                return I_EWRIT;
            }
            
            close(fd);
            
            /* Update record counts */
            g_cfg.tbls[i].nrecs++;
            if (!reuse && g_cfg.tbls[i].nrecs > g_cfg.tbls[i].maxrec)
                g_cfg.tbls[i].maxrec = g_cfg.tbls[i].nrecs;
            
            return I_OK;
        }
    }
    
    return I_ENTBL;
}

/* Find physical index of Nth logical (non-deleted) record */
int i_findlog(tblnam, logidx, physidx)
char *tblnam;
int logidx;
int *physidx;
{
    int fd;
    int i, j, tsz, nsecs;
    int phys, logical;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tsz = g_cfg.tbls[i].recsz;
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            fd = open(fname, 0);
            if (fd == ERROR)
                return I_EOPEN;
            
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EREAD;
            }
            
            logical = 0;
            for (phys = 0; phys < g_cfg.tbls[i].maxrec; phys++)
            {
                if (seek(fd, phys * nsecs, 0) == ERROR)
                {
                    close(fd);
                    return I_EREAD;
                }
                if (read(fd, sbuf, nsecs) < nsecs)
                {
                    close(fd);
                    return I_EREAD;
                }
                
                if (sbuf[0] != I_DELFLAG)
                {
                    if (logical == logidx)
                    {
                        *physidx = phys;
                        close(fd);
                        return I_OK;
                    }
                    logical++;
                }
            }
            
            close(fd);
            return I_ENREC;
        }
    }
    
    return I_ENTBL;
}

/* Read record by physical index (bypasses delete check for scanning) */
int i_rdphys(tblnam, rec, rnum)
char *tblnam;
char *rec;
int rnum;
{
    int fd;
    int i, j, k, tsz, nsecs, recno, total;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (rnum < 0)
        return I_ENREC;
    
    /* Locate table */
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tsz = g_cfg.tbls[i].recsz;
            if (rnum >= g_cfg.tbls[i].maxrec)
                return I_ENREC;
            
            /* Build filename */
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            fd = open(fname, 0);
            if (fd == ERROR)
                return I_EOPEN;
            
            /* Seek to physical record position */
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EREAD;
            }
            recno = rnum * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                return I_EREAD;
            }
            
            /* Read record */
            if (read(fd, sbuf, nsecs) < nsecs)
            {
                close(fd);
                return I_EREAD;
            }
            
            total = nsecs * I_SECSZ;
            for (k = 0; k < tsz && k < total; k++)
                rec[k] = sbuf[k];
            
            close(fd);
            
            /* Return special code if deleted */
            if (rec[0] == I_DELFLAG)
                return I_ENREC;
                
            return I_OK;
        }
    }
    
    return I_ENTBL;
}

/* Read record by index (0-based) from table data file */
int i_rdrec(tblnam, rec, rnum)
char *tblnam;
char *rec;
int rnum;
{
    int fd;
    int i, j, k, tsz, nsecs, recno, total;
    int phys;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (rnum < 0)
        return I_ENREC;
    
    /* Find physical index of logical record */
    if (i_findlog(tblnam, rnum, &phys) != I_OK)
        return I_ENREC;
    
    /* Locate table */
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tsz = g_cfg.tbls[i].recsz;
            
            /* Build filename */
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            fd = open(fname, 0);
            if (fd == ERROR)
                return I_EOPEN;
            
            /* Seek to physical record position */
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EREAD;
            }
            recno = phys * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                return I_EREAD;
            }
            
            /* Read record */
            if (read(fd, sbuf, nsecs) < nsecs)
            {
                close(fd);
                return I_EREAD;
            }
            
            total = nsecs * I_SECSZ;
            for (k = 0; k < tsz && k < total; k++)
                rec[k] = sbuf[k];
            
            close(fd);
            return I_OK;
        }
    }
    
    return I_ENTBL;
}

/* Update record by index using temp file rewrite */
int i_uprec(tblnam, rec, rsiz, rnum)
char *tblnam;
char *rec;
int rsiz;
int rnum;
{
    int fd;
    int i, j, k, tsz, nsecs, recno, total;
    int phys;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (rnum < 0)
        return I_ENREC;
    
    /* Find physical index of logical record */
    if (i_findlog(tblnam, rnum, &phys) != I_OK)
        return I_ENREC;
    
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tsz = g_cfg.tbls[i].recsz;
            if (rsiz != tsz)
                return I_ESIZE;
            
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            fd = open(fname, 2);
            if (fd == ERROR)
                return I_EOPEN;
            
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EUPDT;
            }
            total = nsecs * I_SECSZ;
            for (k = 0; k < rsiz && k < total; k++)
                sbuf[k] = rec[k];
            while (k < total)
            {
                sbuf[k] = 0;
                k++;
            }
            recno = phys * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                return I_EUPDT;
            }
            if (write(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                return I_EUPDT;
            }
            close(fd);
            return I_OK;
        }
    }
    
    return I_ENTBL;
}

/* Delete record by index via lazy delete (mark as deleted) */
int i_delrec(tblnam, rnum)
char *tblnam;
int rnum;
{
    int fd;
    int i, j, tsz, nsecs;
    int recno;
    int phys;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (rnum < 0)
        return I_ENREC;
    
    /* Find physical index of logical record */
    if (i_findlog(tblnam, rnum, &phys) != I_OK)
        return I_ENREC;
    
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            if (g_cfg.tbls[i].nrecs == 0)
                return I_ENREC;
            
            tsz = g_cfg.tbls[i].recsz;
            tdisk = g_cfg.tbls[i].disk;
            fname[0] = tdisk;
            fname[1] = ':';
            j = 0;
            while (tblnam[j] && j < 8)
            {
                fname[j + 2] = tblnam[j];
                j++;
            }
            fname[j + 2] = '.';
            fname[j + 3] = 'D';
            fname[j + 4] = 'A';
            fname[j + 5] = 'T';
            fname[j + 6] = 0;
            
            /* Open file for read/write */
            fd = open(fname, 2);
            if (fd == ERROR)
            {
                printf("[i_delrec] ERROR: Cannot open %s\r\n", fname);
                return I_EOPEN;
            }
            
            /* Calculate sector count for this record */
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                printf("[i_delrec] ERROR: Record too large\r\n");
                return I_EUPDT;
            }
            
            /* Seek to physical record position */
            recno = phys * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                printf("[i_delrec] ERROR: Seek failed\r\n");
                return I_EUPDT;
            }
            
            /* Read the record */
            if (read(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                printf("[i_delrec] ERROR: Read failed\r\n");
                return I_EREAD;
            }
            
            /* Mark first byte as deleted */
            sbuf[0] = I_DELFLAG;
            
            /* Seek back to record position */
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                printf("[i_delrec] ERROR: Seek back failed\r\n");
                return I_EUPDT;
            }
            
            /* Write marked record back */
            if (write(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                printf("[i_delrec] ERROR: Write failed\r\n");
                return I_EUPDT;
            }
            
            close(fd);
            
            /* Decrement logical record count */
            g_cfg.tbls[i].nrecs--;
            if (g_cfg.tbls[i].nrecs < 0)
                g_cfg.tbls[i].nrecs = 0;
            
            return I_OK;
        }
    }
    
    return I_ENTBL;
}
