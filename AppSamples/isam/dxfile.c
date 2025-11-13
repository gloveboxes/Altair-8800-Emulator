#include "stdio.h"
#include "dxisam.h"

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
                printf("[i_insrt] ERROR: Record too large\r\n");
                return I_EWRIT;
            }
            total = nsecs * I_SECSZ;
            
            /* Check for deleted slot to reuse */
            reuse = 0;
            reuse_phys = -1;
            if (g_cfg.tbls[i].nrecs < g_cfg.tbls[i].maxrec)
            {
                for (phys = 0; phys < g_cfg.tbls[i].maxrec; phys++)
                {
                    if (seek(fd, phys * nsecs, 0) == ERROR)
                        break;
                    if (read(fd, sbuf, nsecs) != nsecs)
                        break;
                    if (sbuf[0] == I_DELFLAG)
                    {
                        reuse = 1;
                        reuse_phys = phys;
                        break;
                    }
                }
            }
            
            if (reuse && reuse_phys >= 0)
            {
                phys = reuse_phys;
                if (seek(fd, phys * nsecs, 0) == ERROR)
                {
                    close(fd);
                    printf("[i_insrt] ERROR: Seek failed for reuse\r\n");
                    return I_EWRIT;
                }
            }
            else
            {
                phys = g_cfg.tbls[i].maxrec;
                if (seek(fd, phys * nsecs, 0) == ERROR)
                {
                    close(fd);
                    printf("[i_insrt] ERROR: Seek append failed\r\n");
                    return I_EWRIT;
                }
            }
            
            /* Write record to buffer, pad to sector boundary */
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

/* Write record by physical index (bypasses logical scan) */
int i_wrphys(tblnam, rec, rsiz, phys)
char *tblnam;
char *rec;
int rsiz;
int phys;
{
    int fd;
    int i, j, k, tsz, nsecs, recno, total;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (phys < 0)
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
            if (phys >= g_cfg.tbls[i].maxrec)
                return I_ENREC;
            
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

/* Delete record by physical slot */
int i_delphys(tblnam, phys)
char *tblnam;
int phys;
{
    int fd;
    int i, j, tsz, nsecs;
    int recno;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    if (phys < 0)
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
            if (phys >= g_cfg.tbls[i].maxrec)
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
            
            fd = open(fname, 2);
            if (fd == ERROR)
                return I_EOPEN;
            
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EUPDT;
            }
            
            recno = phys * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                return I_EUPDT;
            }
            
            if (read(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                return I_EREAD;
            }
            if (sbuf[0] == I_DELFLAG)
            {
                close(fd);
                return I_ENREC;
            }
            
            sbuf[0] = I_DELFLAG;
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
            
            g_cfg.tbls[i].nrecs--;
            if (g_cfg.tbls[i].nrecs < 0)
                g_cfg.tbls[i].nrecs = 0;
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
    int phys;
    
    if (rnum < 0)
        return I_ENREC;
    
    /* Find physical index of logical record */
    if (i_findlog(tblnam, rnum, &phys) != I_OK)
        return I_ENREC;
    
    return i_wrphys(tblnam, rec, rsiz, phys);
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
            
            fd = open(fname, 2);
            if (fd == ERROR)
                return I_EOPEN;
            
            nsecs = (tsz + I_SECSZ - 1) / I_SECSZ;
            if (nsecs > I_NSECTS)
            {
                close(fd);
                return I_EUPDT;
            }
            recno = phys * nsecs;
            if (seek(fd, recno, 0) == ERROR)
            {
                close(fd);
                return I_EUPDT;
            }
            
            if (read(fd, sbuf, nsecs) != nsecs)
            {
                close(fd);
                return I_EREAD;
            }
            if (sbuf[0] == I_DELFLAG)
            {
                close(fd);
                return I_ENREC;
            }
            sbuf[0] = I_DELFLAG;
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
            
            g_cfg.tbls[i].nrecs--;
            if (g_cfg.tbls[i].nrecs < 0)
                g_cfg.tbls[i].nrecs = 0;
            return I_OK;
        }
    }
    
    return I_ENTBL;
}
