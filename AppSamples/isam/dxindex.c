#include "stdio.h"
#include "dxisam.h"

#define I_IDXSZ (I_MXKEYLN + 2)

/* Helper to update idxcnt without using unsupported struct pointers */
void tbl_set_idxcnt(tidx, value)
int tidx;
int value;
{
    int *p;

    p = &g_cfg.tbls[tidx].nrecs;
    p++;
    p++;
    *p = value;
}

int i_idxbld(tblnam, idxarr, maxent)
char *tblnam;
struct i_idxent idxarr[];
int maxent;
{
    int i, j, k, tidx, slot, count, ksz, koff;
    int nextsample;
    int rc;
    char rec[I_RECSZ];
    
    tidx = -1;
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tidx = i;
            break;
        }
    }
    
    if (tidx < 0)
        return I_ENTBL;
    
    if (g_cfg.tbls[tidx].nkeys == 0)
        return I_EREAD;
    
    koff = g_cfg.tbls[tidx].keyoff[0];
    ksz = g_cfg.tbls[tidx].keysz[0];
    if (ksz > I_MXKEYLN)
        ksz = I_MXKEYLN;
    
    count = 0;
    nextsample = 0;
    for (slot = 0; slot < g_cfg.tbls[tidx].maxrec; slot++)
    {
        rc = i_rdphys(tblnam, rec, slot);
        if (rc != I_OK)
            continue;
        if (slot < nextsample)
            continue;
        if (count >= maxent)
        {
            printf("[i_idxbld] index capacity reached (%d entries)\r\n", maxent);
            break;
        }
        
        idxarr[count].phys = slot;
        
        for (k = 0; k < I_MXKEYLN; k++)
        {
            if (k < ksz)
                idxarr[count].key[k] = rec[koff + k];
            else
                idxarr[count].key[k] = 0;
        }
        
        count++;
        nextsample = slot + I_IDXSAMP;
    }
    
    printf("[i_idxbld] built %d entries (sampling every %d records)\r\n", count, I_IDXSAMP);
    if (count > 0)
    {
        printf("[i_idxbld] first entry phys=%d key=%.12s\r\n", 
            idxarr[0].phys, idxarr[0].key);
        printf("[i_idxbld] last entry phys=%d key=%.12s\r\n", 
            idxarr[count - 1].phys, idxarr[count - 1].key);
    }
    else
        printf("[i_idxbld] index is empty\r\n");
    
    tbl_set_idxcnt(tidx, count);
    g_cfg.tbls[tidx].idxsamp = I_IDXSAMP;
    
    return count;
}

/* Binary search index for key - return range [start, end) */
int i_idxsrch(tblnam, srchkey, idxarr, idxcnt, startphys, endphys)
char *tblnam;
char *srchkey;
struct i_idxent idxarr[];
int idxcnt;
int *startphys;
int *endphys;
{
    int i, j, tidx, ksz, cmp;
    int lo, hi, mid, best, slot;
    char *ekey;
    
    /* Find table */
    tidx = -1;
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tidx = i;
            break;
        }
    }
    
    if (tidx < 0)
        return I_ENTBL;
    
    if (idxcnt == 0)
    {
        *startphys = 0;
        *endphys = g_cfg.tbls[tidx].maxrec;
        return I_OK;
    }
    
    ksz = g_cfg.tbls[tidx].keysz[0];
    if (ksz > I_MXKEYLN)
        ksz = I_MXKEYLN;
    
    /* Binary search */
    lo = 0;
    hi = idxcnt - 1;
    
    best = -1;
    while (lo <= hi)
    {
        mid = (lo + hi) / 2;
        ekey = idxarr[mid].key;
        
        /* Compare keys */
        cmp = 0;
        for (i = 0; i < ksz; i++)
        {
            char kb, ekb;
            kb = srchkey[i];
            ekb = ekey[i];
            if (kb < ekb)
            {
                cmp = -1;
                break;
            }
            else if (kb > ekb)
            {
                cmp = 1;
                break;
            }
        }
        
        if (cmp < 0)
            hi = mid - 1;
        else if (cmp > 0)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            /* Exact match */
            *startphys = idxarr[mid].phys;
            if (mid + 1 < idxcnt)
            {
                *endphys = idxarr[mid + 1].phys;
            }
            else
                *endphys = g_cfg.tbls[tidx].maxrec;
            return I_OK;
        }
    }
    
    /* Not exact match - return range around insertion point */
    if (cmp > 0)
        slot = best;
    else
        slot = lo - 1;
    
    if (slot < 0)
        slot = 0;
    if (slot >= idxcnt)
        slot = idxcnt - 1;
    
    *startphys = idxarr[slot].phys;
    if (cmp < 0 && slot == 0)
        *startphys = 0;
    if (slot + 1 < idxcnt)
    {
        *endphys = idxarr[slot + 1].phys;
    }
    else
        *endphys = g_cfg.tbls[tidx].maxrec;
    
    return I_OK;
}

/* High-level indexed lookup - returns physical slot or -1 */
int i_idxlookup(tblnam, srchkey, idxarr, idxcnt, rec)
char *tblnam;
char *srchkey;
struct i_idxent idxarr[];
int idxcnt;
char *rec;
{
    int i, j, tidx, ksz, koff;
    int startphys, endphys, phys;
    int rc, cmp;
    int fd;
    int tsz, nsecs, recno, total;
    char fname[20];
    char tdisk;
    char sbuf[I_BUFSZ];
    
    /* Find table */
    tidx = -1;
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tidx = i;
            break;
        }
    }
    
    if (tidx < 0)
        return I_ENTBL;
    
    if (g_cfg.tbls[tidx].nkeys == 0)
        return I_EREAD;
    
    koff = g_cfg.tbls[tidx].keyoff[0];
    ksz = g_cfg.tbls[tidx].keysz[0];
    
    /* Use index to narrow search */
    rc = i_idxsrch(tblnam, srchkey, idxarr, idxcnt, &startphys, &endphys);
    if (rc != I_OK)
        return rc;
    
    {
        char dbgkey[I_MXKEYLN + 1];
        for (i = 0; i < ksz && i < I_MXKEYLN; i++)
            dbgkey[i] = srchkey[i];
        dbgkey[i] = 0;
        printf("[i_idxlookup] key=%s range=%d-%d\r\n", dbgkey, startphys, endphys);
    }

    tsz = g_cfg.tbls[tidx].recsz;
    tdisk = g_cfg.tbls[tidx].disk;
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
    total = nsecs * I_SECSZ;

    /* Linear scan within range */
    for (phys = startphys; phys < endphys; phys++)
    {
        recno = phys * nsecs;
        if (seek(fd, recno, 0) == ERROR)
            continue;
        if (read(fd, sbuf, nsecs) < nsecs)
            continue;
        
        for (i = 0; i < tsz && i < total; i++)
            rec[i] = sbuf[i];
        if (rec[0] == I_DELFLAG)
            continue;
        
        /* Compare keys */
        cmp = 0;
        for (i = 0; i < ksz; i++)
        {
            if (srchkey[i] != rec[koff + i])
            {
                cmp = 1;
                break;
            }
        }
        
        if (cmp == 0)
        {
            close(fd);
            return phys;  /* Found - return physical slot */
        }
    }
    
    close(fd);
    return I_ENREC;  /* Not found */
}

/* Flattened index entry helpers (avoid -> on BDS C) */
char *idxptr(idxarr, pos)
struct i_idxent idxarr[];
int pos;
{
    char *p;
    int i;

    p = &idxarr[0];
    i = 0;
    while (i < pos)
    {
        p = p + I_IDXSZ;
        i++;
    }
    return p;
}

int idxgphy(idxarr, pos)
struct i_idxent idxarr[];
int pos;
{
    char *p;
    int lo;
    int hi;

    p = idxptr(idxarr, pos);
    p = p + I_MXKEYLN;
    lo = p[0] & 0xFF;
    hi = p[1] & 0xFF;
    return (hi << 8) | lo;
}

void idxsphy(idxarr, pos, value)
struct i_idxent idxarr[];
int pos;
int value;
{
    char *p;

    p = idxptr(idxarr, pos);
    p = p + I_MXKEYLN;
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
}

void idxcopy(idxarr, dst, src)
struct i_idxent idxarr[];
int dst;
int src;
{
    int i;
    char *dp;
    char *sp;

    dp = idxptr(idxarr, dst);
    sp = idxptr(idxarr, src);
    for (i = 0; i < I_IDXSZ; i++)
        dp[i] = sp[i];
}

/* ============================================================
 * Incremental Index Maintenance
 * ============================================================
 * These functions maintain the index as records are inserted
 * and deleted, avoiding the need for full rebuilds. The index
 * remains in memory (for now) and will be disk-backed later.
 *
 * i_idxins: Insert/update a single index entry maintaining sort order
 * i_idxdel: Remove an index entry by physical slot number
 */

/* Insert or update a single index entry (maintains sorted order) */
int i_idxins(tblnam, phys, rec, idxarr, idxcnt, maxent)
char *tblnam;
int phys;
char *rec;
struct i_idxent idxarr[];
int *idxcnt;
int maxent;
{
    int i, j, k, tidx, ksz, koff, cmp;
    int insertpos;
    int existing_phys;
    char *ekey;
    char *dstkey;
    char newkey[I_MXKEYLN];
    
    /* Find table */
    tidx = -1;
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tidx = i;
            break;
        }
    }
    
    if (tidx < 0)
        return I_ENTBL;

    if (g_cfg.tbls[tidx].nkeys == 0)
        return I_EREAD;
    
    /* Check if index is full */
    if (*idxcnt >= maxent)
        return I_ESIZE;

    koff = g_cfg.tbls[tidx].keyoff[0];
    ksz = g_cfg.tbls[tidx].keysz[0];
    if (ksz > I_MXKEYLN)
        ksz = I_MXKEYLN;
    
    /* Extract key from record */
    for (k = 0; k < I_MXKEYLN; k++)
    {
        if (k < ksz)
            newkey[k] = rec[koff + k];
        else
            newkey[k] = 0;
    }
    
    /* Find insertion point (binary search for position) */
    insertpos = *idxcnt;
    for (i = 0; i < *idxcnt; i++)
    {
    ekey = idxptr(idxarr, i);

        /* Compare keys */
        cmp = 0;
        for (j = 0; j < ksz; j++)
        {
            if (newkey[j] < ekey[j])
            {
                cmp = -1;
                break;
            }
            else if (newkey[j] > ekey[j])
            {
                cmp = 1;
                break;
            }
        }

        if (cmp == 0)
        {
            existing_phys = idxgphy(idxarr, i);
            if (phys == existing_phys)
            {
                /* Update existing entry in place */
                for (k = 0; k < I_MXKEYLN; k++)
                    ekey[k] = newkey[k];
                idxsphy(idxarr, i, phys);
                return I_OK;
            }
        }

        if (cmp < 0)
        {
            insertpos = i;
            break;
        }
    }
    
    /* Shift entries to make room */
    for (i = *idxcnt; i > insertpos; i--)
    {
    idxcopy(idxarr, i, i - 1);
    }
    
    /* Insert new entry */
    dstkey = idxptr(idxarr, insertpos);
    for (k = 0; k < I_MXKEYLN; k++)
        dstkey[k] = newkey[k];
    idxsphy(idxarr, insertpos, phys);

    (*idxcnt)++;
    tbl_set_idxcnt(tidx, *idxcnt);
    
    return I_OK;
}

/* Remove an index entry by physical slot number */
int i_idxdel(tblnam, phys, idxarr, idxcnt)
char *tblnam;
int phys;
struct i_idxent idxarr[];
int *idxcnt;
{
    int i, j, tidx;
    int found;
    
    /* Find table */
    tidx = -1;
    for (i = 0; i < g_cfg.ntbls; i++)
    {
        for (j = 0; tblnam[j] && g_cfg.tbls[i].name[j]; j++)
            if (tblnam[j] != g_cfg.tbls[i].name[j])
                break;
        if (tblnam[j] == 0 && g_cfg.tbls[i].name[j] == 0)
        {
            tidx = i;
            break;
        }
    }
    
    if (tidx < 0)
        return I_ENTBL;

    /* Find and remove the entry */
    found = -1;
    for (i = 0; i < *idxcnt; i++)
    {
    if (idxgphy(idxarr, i) == phys)
        {
            found = i;
            break;
        }
    }
    
    if (found < 0)
        return I_OK;  /* Not in index (maybe wasn't sampled) */
    
    /* Shift entries down */
    for (i = found; i < *idxcnt - 1; i++)
    {
    idxcopy(idxarr, i, i + 1);
    }

    (*idxcnt)--;
    tbl_set_idxcnt(tidx, *idxcnt);
    
    return I_OK;
}
