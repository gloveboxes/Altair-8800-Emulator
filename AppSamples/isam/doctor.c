#include "stdio.h"
#include "string.h"
#include "dxisam.h"

#define RECORD_SIZE 81

/* Global index for PATIENTS table */
struct i_idxent g_idx[I_MXIDX];
int g_idxcnt;
int g_idx_ready;
int g_idx_dirty;

#define P_IDLN 5
#define P_NMLN 16
#define P_ADLN 40
#define P_CNT 2000
#define F_CNT 20
#define L_CNT 20
#define S_CNT 10
#define G_CNT 3

struct patient
{
    int id;
    char first[P_NMLN];
    char last[P_NMLN];
    char address[P_ADLN];
    int age;
    char gender;
};

int readback();
int do_updates();
int do_deletes();
int do_lookups();
int lookup_patient();
int delete_patient();
int ensure_index();
int rebuild_index();
void copy_key_from_record();
int idx_get_phys_at();
void idx_set_phys_at();
void idx_copy_entry();
int idx_remove_entry();
void idx_append_sample();
void idx_update_after_delete();
int get_pid();
int make_patient();
int print_patients();

char fname[F_CNT][P_NMLN];
char lname[L_CNT][P_NMLN];
char saddr[S_CNT][P_ADLN];
char gendr[G_CNT];

int namrdy;

int setadr(dst, house, street, max)
char *dst;
int house;
char *street;
int max;
{
    int i;
    int val;
    char digits[6];
    int dcnt;

    memset(dst, 0, max);

    val = house;
    if (val < 0)
        val = -val;

    dcnt = 0;
    if (val == 0)
    {
        digits[dcnt] = '0';
        dcnt++;
    }
    else
    {
        while (val > 0 && dcnt < 5)
        {
            digits[dcnt] = (val % 10) + '0';
            dcnt++;
            val = val / 10;
        }
    }

    i = 0;
    while (dcnt > 0 && i < max - 1)
    {
        dcnt--;
        dst[i] = digits[dcnt];
        i++;
    }

    if (i < max - 1)
    {
        dst[i] = ' ';
        i++;
    }

    strncat(dst, street, max - i - 1);

    return 0;
}

int copy_field(dst, src, max)
char *dst;
char *src;
int max;
{
    int i;

    for (i = 0; i < max; i++)
    {
        if (src && src[i])
            dst[i] = src[i];
        else
            break;
    }

    while (i < max)
    {
        dst[i] = 0;
        i++;
    }

    return 0;
}

int setnum(ptr, value)
char *ptr;
int value;
{
    int num;

    if (value < 0)
        value = 0;
    if (value > 999)
        value = 999;

    num = value / 100;
    ptr[0] = '0' + num;
    value = value % 100;
    num = value / 10;
    ptr[1] = '0' + num;
    ptr[2] = '0' + (value % 10);

    return 0;
}

int setpid(ptr, value)
char *ptr;
int value;
{
    int digit;

    if (value < 0)
        value = 0;

    digit = 0;
    while (value >= 10000)
    {
        value = value - 10000;
        digit++;
    }
    ptr[0] = '0' + digit;

    digit = 0;
    while (value >= 1000)
    {
        value = value - 1000;
        digit++;
    }
    ptr[1] = '0' + digit;

    digit = 0;
    while (value >= 100)
    {
        value = value - 100;
        digit++;
    }
    ptr[2] = '0' + digit;

    digit = 0;
    while (value >= 10)
    {
        value = value - 10;
        digit++;
    }
    ptr[3] = '0' + digit;

    ptr[4] = '0' + value;

    return 0;
}

int naminit()
{
    if (namrdy)
        return 0;

    strncpy(fname[0], "Alex", P_NMLN);
    strncpy(fname[1], "Blair", P_NMLN);
    strncpy(fname[2], "Casey", P_NMLN);
    strncpy(fname[3], "Drew", P_NMLN);
    strncpy(fname[4], "Elliot", P_NMLN);
    strncpy(fname[5], "Finley", P_NMLN);
    strncpy(fname[6], "Gale", P_NMLN);
    strncpy(fname[7], "Harper", P_NMLN);
    strncpy(fname[8], "Indigo", P_NMLN);
    strncpy(fname[9], "Jordan", P_NMLN);
    strncpy(fname[10], "Kai", P_NMLN);
    strncpy(fname[11], "Logan", P_NMLN);
    strncpy(fname[12], "Morgan", P_NMLN);
    strncpy(fname[13], "Nico", P_NMLN);
    strncpy(fname[14], "Oakley", P_NMLN);
    strncpy(fname[15], "Peyton", P_NMLN);
    strncpy(fname[16], "Quinn", P_NMLN);
    strncpy(fname[17], "Riley", P_NMLN);
    strncpy(fname[18], "Sawyer", P_NMLN);
    strncpy(fname[19], "Taylor", P_NMLN);

    strncpy(lname[0], "Anderson", P_NMLN);
    strncpy(lname[1], "Bennett", P_NMLN);
    strncpy(lname[2], "Carter", P_NMLN);
    strncpy(lname[3], "Dalton", P_NMLN);
    strncpy(lname[4], "Ellis", P_NMLN);
    strncpy(lname[5], "Fletcher", P_NMLN);
    strncpy(lname[6], "Garcia", P_NMLN);
    strncpy(lname[7], "Hayes", P_NMLN);
    strncpy(lname[8], "Iverson", P_NMLN);
    strncpy(lname[9], "Jackson", P_NMLN);
    strncpy(lname[10], "Knight", P_NMLN);
    strncpy(lname[11], "Lawson", P_NMLN);
    strncpy(lname[12], "Maddox", P_NMLN);
    strncpy(lname[13], "Nolan", P_NMLN);
    strncpy(lname[14], "Owens", P_NMLN);
    strncpy(lname[15], "Prescott", P_NMLN);
    strncpy(lname[16], "Quincy", P_NMLN);
    strncpy(lname[17], "Ramsey", P_NMLN);
    strncpy(lname[18], "Sawyer", P_NMLN);
    strncpy(lname[19], "Thatcher", P_NMLN);

    strncpy(saddr[0], "Maple Ave", P_ADLN);
    strncpy(saddr[1], "Oak Street", P_ADLN);
    strncpy(saddr[2], "Pine Road", P_ADLN);
    strncpy(saddr[3], "Cedar Lane", P_ADLN);
    strncpy(saddr[4], "Elm Drive", P_ADLN);
    strncpy(saddr[5], "Birch Way", P_ADLN);
    strncpy(saddr[6], "Spruce Court", P_ADLN);
    strncpy(saddr[7], "Willow Blvd", P_ADLN);
    strncpy(saddr[8], "Cherry Path", P_ADLN);
    strncpy(saddr[9], "Ash Terrace", P_ADLN);

    gendr[0] = 'M';
    gendr[1] = 'F';
    gendr[2] = 'O';

    namrdy = 1;
    return 0;
}

int make_patient(seq, pat)
int seq;
struct patient *pat;
{
    int idx;
    int findx;
    int lindx;
    int sindx;
    int hous;
    int agev;
    int gndx;

    naminit();

    if (seq <= 0)
        seq = 1;

    idx = seq - 1;
    findx = idx % F_CNT;
    lindx = (idx * 3) % L_CNT;
    sindx = (idx * 7) % S_CNT;
    hous = 100 + (idx * 4);
    agev = 1 + ((idx * 11) % 100);
    gndx = (idx + 1) % G_CNT;

    memset(pat, 0, 81);
    pat->id = seq;
    strncpy(pat->first, fname[findx], P_NMLN);
    strncpy(pat->last, lname[lindx], P_NMLN);
    setadr(pat->address, hous, saddr[sindx], P_ADLN);
    pat->age = agev;
    pat->gender = gendr[gndx];

    return 0;
}

int initcfg()
{
    int i;
    int j;

    for (i = 0; i < I_MXNM; i++)
        g_cfg.dbname[i] = 0;

    strncpy(g_cfg.dbname, "PATIENTS", I_MXNM);
    g_cfg.ntbls = 1;

    for (i = 0; i < I_MXNM; i++)
        g_cfg.tbls[0].name[i] = 0;

    strncpy(g_cfg.tbls[0].name, "PATIENTS", I_MXNM);
    g_cfg.tbls[0].disk = 'C';
    g_cfg.tbls[0].recsz = RECORD_SIZE;
    g_cfg.tbls[0].maxrec = 0;
    g_cfg.tbls[0].nkeys = 1;

    for (i = 0; i < I_MXKEY; i++)
    {
        g_cfg.tbls[0].keyoff[i] = 0;
        g_cfg.tbls[0].keysz[i] = 0;
    }

    g_cfg.tbls[0].keyoff[0] = 0;
    g_cfg.tbls[0].keysz[0] = P_IDLN;
    g_cfg.tbls[0].nrecs = 0;

    for (j = 1; j < I_MXTBL; j++)
    {
        for (i = 0; i < I_MXNM; i++)
            g_cfg.tbls[j].name[i] = 0;
        g_cfg.tbls[j].disk = 0;
        g_cfg.tbls[j].recsz = 0;
        g_cfg.tbls[j].nkeys = 0;
        g_cfg.tbls[j].nrecs = 0;
        g_cfg.tbls[j].maxrec = 0;
        for (i = 0; i < I_MXKEY; i++)
        {
            g_cfg.tbls[j].keyoff[i] = 0;
            g_cfg.tbls[j].keysz[i] = 0;
        }
    }

    return 0;
}

int bldrec(pat, rec)
struct patient *pat;
char *rec;
{
    memset(rec, 0, RECORD_SIZE);

    setpid(rec, pat->id);
    strncpy(&rec[P_IDLN], pat->first, P_NMLN);
    strncpy(&rec[P_IDLN + P_NMLN], pat->last, P_NMLN);
    strncpy(&rec[P_IDLN + (P_NMLN * 2)], pat->address, P_ADLN);
    setnum(&rec[P_IDLN + (P_NMLN * 2) + P_ADLN], pat->age);
    rec[P_IDLN + (P_NMLN * 2) + P_ADLN + 3] = pat->gender;

    return 0;
}

int print_patients()
{
    int i;
    struct patient pat;

    puts("Patient Records:");
    for (i = 0; i < P_CNT; i++)
    {
        make_patient(i + 1, &pat);
        printf("%05d: %-15s %-15s %-30s Age:%3d Gender:%c\r\n",
            pat.id,
            pat.first,
            pat.last,
            pat.address,
            pat.age,
            pat.gender);
    }

    return 0;
}

int readback()
{
    int i;
    int j;
    int rc;
    int age;
    int ageoff;
    int digitsok;
    int count;
    char rbuf[RECORD_SIZE];
    char pid[P_IDLN + 1];
    char fnamebuf[P_NMLN + 1];
    char lnamebuf[P_NMLN + 1];
    char addrbuf[P_ADLN + 1];
    char gender;

    printf("\r\nRecords read back from disk:\r\n");
    count = 0;
    for (i = 0; i < g_cfg.tbls[0].maxrec; i++)
    {
        for (j = 0; j < RECORD_SIZE; j++)
            rbuf[j] = 0;

        rc = i_rdphys("PATIENTS", rbuf, i);
        if (rc == I_ENREC)
            continue;
        if (rc != I_OK)
        {
            printf("Read failed at record %d rc=%d\r\n", i + 1, rc);
            return 1;
        }

        count++;

        digitsok = 1;
        for (j = 0; j < P_IDLN; j++)
        {
            char ch;

            ch = rbuf[j];
            if (ch < '0' || ch > '9')
                digitsok = 0;
            if (ch < 32 || ch > 126)
                pid[j] = '?';
            else
                pid[j] = ch;
        }
        pid[P_IDLN] = 0;
        if (!digitsok)
            printf("Record %d has non-digit id bytes\r\n", i + 1);

        for (j = 0; j < P_NMLN; j++)
            fnamebuf[j] = rbuf[P_IDLN + j];
        fnamebuf[P_NMLN] = 0;

        for (j = 0; j < P_NMLN; j++)
            lnamebuf[j] = rbuf[P_IDLN + P_NMLN + j];
        lnamebuf[P_NMLN] = 0;

        for (j = 0; j < P_ADLN; j++)
            addrbuf[j] = rbuf[P_IDLN + (P_NMLN * 2) + j];
        addrbuf[P_ADLN] = 0;

        ageoff = P_IDLN + (P_NMLN * 2) + P_ADLN;
        age = (rbuf[ageoff] - '0') * 100;
        age = age + ((rbuf[ageoff + 1] - '0') * 10);
        age = age + (rbuf[ageoff + 2] - '0');

        gender = rbuf[ageoff + 3];

        printf("%s: %-15s %-15s %-30s Age:%3d Gender:%c\r\n",
            pid,
            fnamebuf,
            lnamebuf,
            addrbuf,
            age,
            gender);
    }

    printf("Total records displayed: %d\r\n", count);
    return 0;
}

void copy_key_from_record(dest, rec)
char *dest;
char *rec;
{
    int i;
    int ksz;
    int koff;

    ksz = g_cfg.tbls[0].keysz[0];
    if (ksz > I_MXKEYLN)
        ksz = I_MXKEYLN;
    koff = g_cfg.tbls[0].keyoff[0];

    for (i = 0; i < I_MXKEYLN; i++)
    {
        if (i < ksz)
            dest[i] = rec[koff + i];
        else
            dest[i] = 0;
    }
}

/* Helpers that read/write the physical slot without relying on -> syntax */
int idx_get_phys_at(idx)
int idx;
{
    int lo, hi;
    char *p;

    p = &g_idx[idx];
    p = p + I_MXKEYLN;
    lo = p[0] & 0xFF;
    hi = p[1] & 0xFF;
    return (hi << 8) | lo;
}

void idx_set_phys_at(idx, value)
int idx;
int value;
{
    char *p;

    p = &g_idx[idx];
    p = p + I_MXKEYLN;
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
}

void idx_copy_entry(dst, src)
int dst;
int src;
{
    int i;

    idx_set_phys_at(dst, idx_get_phys_at(src));
    for (i = 0; i < I_MXKEYLN; i++)
        g_idx[dst].key[i] = g_idx[src].key[i];
}

int idx_remove_entry(phys)
int phys;
{
    int i;
    int j;

    for (i = 0; i < g_idxcnt; i++)
    {
        if (idx_get_phys_at(i) == phys)
        {
            for (j = i; j < g_idxcnt - 1; j++)
                idx_copy_entry(j, j + 1);
            g_idxcnt--;
            return 1;
        }
    }

    return 0;
}

void idx_append_sample()
{
    int slot;
    int start;
    int rc;
    char rec[RECORD_SIZE];

    if (g_idxcnt >= I_MXIDX)
        return;

    if (g_idxcnt == 0)
        start = 0;
    else
    {
        start = idx_get_phys_at(g_idxcnt - 1) + I_IDXSAMP;
        if (start <= idx_get_phys_at(g_idxcnt - 1))
            start = idx_get_phys_at(g_idxcnt - 1) + 1;
    }

    if (start < 0)
        start = 0;

    for (slot = start; slot < g_cfg.tbls[0].maxrec; slot++)
    {
        rc = i_rdphys("PATIENTS", rec, slot);
        if (rc != I_OK)
            continue;

        if (g_idxcnt > 0 && slot <= idx_get_phys_at(g_idxcnt - 1))
            continue;

        idx_set_phys_at(g_idxcnt, slot);
        copy_key_from_record(g_idx[g_idxcnt].key, rec);
        g_idxcnt++;
        break;
    }
}

void idx_update_after_delete(phys)
int phys;
{
    int i;
    int slot;
    int limit;
    int rc;
    char rec[RECORD_SIZE];

    for (i = 0; i < g_idxcnt; i++)
    {
        if (idx_get_phys_at(i) == phys)
        {
            if (i + 1 < g_idxcnt)
                limit = idx_get_phys_at(i + 1);
            else
                limit = g_cfg.tbls[0].maxrec;

            for (slot = phys + 1; slot < limit; slot++)
            {
                rc = i_rdphys("PATIENTS", rec, slot);
                if (rc != I_OK)
                    continue;

                idx_set_phys_at(i, slot);
                copy_key_from_record(g_idx[i].key, rec);
                return;
            }

            if (idx_remove_entry(phys))
                idx_append_sample();
            return;
        }
    }
}

int rebuild_index()
{
    int rc;

    puts("Refreshing sparse index...");
    rc = i_idxbld("PATIENTS", g_idx, I_MXIDX);
    if (rc < 0)
        return rc;

    g_idxcnt = rc;
    g_idx_ready = 1;
    g_idx_dirty = 0;

    printf("Index built: %d entries (sampling every %d records)\r\n",
        g_idxcnt, I_IDXSAMP);
    return I_OK;
}

int ensure_index()
{
    if (!g_idx_ready || g_idx_dirty)
        return rebuild_index();
    return I_OK;
}

int get_pid(rec)
char *rec;
{
    int i;
    int val;
    char ch;

    val = 0;
    for (i = 0; i < P_IDLN; i++)
    {
        ch = rec[i];
        if (ch < '0' || ch > '9')
            return -1;
        val = val * 10 + (ch - '0');
    }

    return val;
}

int lookup_patient(pid)
int pid;
{
    int rc;
    int rid;
    int age;
    int ageoff;
    int phys;
    char rec[RECORD_SIZE];
    char keybuf[P_IDLN + 1];
    char fnamebuf[P_NMLN + 1];
    char lnamebuf[P_NMLN + 1];
    char addrbuf[P_ADLN + 1];
    char gender;

    rc = ensure_index();
    if (rc != I_OK)
        return rc;

    /* Build key from pid */
    setpid(keybuf, pid);
    keybuf[P_IDLN] = 0;

    /* Use indexed lookup */
    phys = i_idxlookup("PATIENTS", keybuf, g_idx, g_idxcnt, rec);
    if (phys < 0)
    {
        printf("Lookup %d -> not found\r\n", pid);
        return I_ENREC;
    }

    /* Parse and display record */
    rid = get_pid(rec);
    strncpy(fnamebuf, &rec[P_IDLN], P_NMLN);
    fnamebuf[P_NMLN] = 0;
    strncpy(lnamebuf, &rec[P_IDLN + P_NMLN], P_NMLN);
    lnamebuf[P_NMLN] = 0;
    strncpy(addrbuf, &rec[P_IDLN + (P_NMLN * 2)], P_ADLN);
    addrbuf[P_ADLN] = 0;

    ageoff = P_IDLN + (P_NMLN * 2) + P_ADLN;
    age = (rec[ageoff] - '0') * 100;
    age = age + ((rec[ageoff + 1] - '0') * 10);
    age = age + (rec[ageoff + 2] - '0');
    gender = rec[ageoff + 3];

    printf("Lookup %d -> %-15s %-15s %-30s Age:%3d Gender:%c\r\n",
        pid,
        fnamebuf,
        lnamebuf,
        addrbuf,
        age,
        gender);
    return I_OK;
}

int do_lookups()
{
    int rc;
    int i;
    int j;
    int pid;
    int limit;
    int extra;
    int total;

    total = g_cfg.tbls[0].nrecs;
    if (total < 0)
        total = 0;

    rc = ensure_index();
    if (rc != I_OK)
        return rc;

    if (g_idxcnt > 0)
    {
        /* Walk several sample keys plus nearby records to exercise index ranges */
        limit = g_idxcnt;
        if (limit > 10)
            limit = 10;
        for (i = 0; i < limit; i++)
        {
            pid = 0;
            for (j = 0; j < P_IDLN; j++)
                pid = (pid * 10) + (g_idx[i].key[j] - '0');

            rc = lookup_patient(pid);
            if (rc != I_OK && rc != I_ENREC)
                return rc;

            extra = pid - 1;
            if (extra > 0)
            {
                rc = lookup_patient(extra);
                if (rc != I_OK && rc != I_ENREC)
                    return rc;
            }

            if (I_IDXSAMP > 1)
            {
                extra = pid + (I_IDXSAMP / 2);
                if (extra > pid && extra <= total)
                {
                    rc = lookup_patient(extra);
                    if (rc != I_OK && rc != I_ENREC)
                        return rc;
                }
            }
        }

        /* Probe the tail of the index explicitly */
        pid = 0;
        for (j = 0; j < P_IDLN; j++)
            pid = (pid * 10) + (g_idx[g_idxcnt - 1].key[j] - '0');

        rc = lookup_patient(pid);
        if (rc != I_OK && rc != I_ENREC)
            return rc;

        extra = pid + 1;
        if (extra <= total)
        {
            rc = lookup_patient(extra);
            if (rc != I_OK && rc != I_ENREC)
                return rc;
        }

        if (I_IDXSAMP > 1)
        {
            extra = pid + (I_IDXSAMP - 1);
            if (extra <= total)
            {
                rc = lookup_patient(extra);
                if (rc != I_OK && rc != I_ENREC)
                    return rc;
            }
        }
        return I_OK;
    }

    rc = lookup_patient(1);
    if (rc != I_OK && rc != I_ENREC)
        return rc;

    rc = lookup_patient(5);
    if (rc != I_OK && rc != I_ENREC)
        return rc;

    rc = lookup_patient(12);
    if (rc != I_OK && rc != I_ENREC)
        return rc;

    rc = lookup_patient(25);
    if (rc != I_OK && rc != I_ENREC)
        return rc;

    rc = lookup_patient(128);
    if (rc != I_OK && rc != I_ENREC)
        return rc;

    return I_OK;
}

int apply_update(pid, fname, lname, housenum, streetname, age, gender)
int pid;
char *fname;
char *lname;
int housenum;
char *streetname;
int age;
char gender;
{
    char rec[RECORD_SIZE];
    char keybuf[P_IDLN + 1];
    char currec[RECORD_SIZE];
    int ageoff;
    int rc;
    int phys;

    if (pid <= 0)
        return I_ENREC;

    rc = ensure_index();
    if (rc != I_OK)
        return rc;

    memset(rec, 0, RECORD_SIZE);
    setpid(keybuf, pid);
    keybuf[P_IDLN] = 0;
    setpid(rec, pid);
    copy_field(&rec[P_IDLN], fname, P_NMLN);
    copy_field(&rec[P_IDLN + P_NMLN], lname, P_NMLN);
    setadr(&rec[P_IDLN + (P_NMLN * 2)], housenum, streetname, P_ADLN);

    ageoff = P_IDLN + (P_NMLN * 2) + P_ADLN;
    setnum(&rec[ageoff], age);
    rec[ageoff + 3] = gender;

    phys = i_idxlookup("PATIENTS", keybuf, g_idx, g_idxcnt, currec);
    if (phys >= 0)
        rc = i_wrphys("PATIENTS", rec, RECORD_SIZE, phys);
    else if (phys == I_ENREC)
        rc = i_uprec("PATIENTS", rec, RECORD_SIZE, pid - 1);
    else
        rc = phys;

    if (rc != I_OK)
        printf("Update pid %d failed rc=%d\r\n", pid, rc);

    return rc;
}

int delete_patient(pid)
int pid;
{
    int phys;
    int rc;
    char keybuf[P_IDLN + 1];
    char rec[RECORD_SIZE];

    if (pid <= 0)
        return I_ENREC;

    rc = ensure_index();
    if (rc != I_OK)
        return rc;

    setpid(keybuf, pid);
    keybuf[P_IDLN] = 0;

    phys = i_idxlookup("PATIENTS", keybuf, g_idx, g_idxcnt, rec);
    if (phys >= 0)
    {
        rc = i_delphys("PATIENTS", phys);
        if (rc == I_OK)
        {
            /* Remove from index and update */
            i_idxdel("PATIENTS", phys, g_idx, &g_idxcnt);
            idx_update_after_delete(phys);
        }
    }
    else
        rc = phys;

    if (rc != I_OK && rc != I_ENREC)
        printf("Delete pid %d failed rc=%d\r\n", pid, rc);

    return rc;
}

int do_updates()
{
    int rc;
    int i;
    int total;
    int count;
    int pid;

    total = g_cfg.tbls[0].nrecs;
    if (total <= 0)
        return 1;

    count = total / 10;
    if (count <= 0)
        count = 1;

    printf("Updating %d records (10%% of %d)...\r\n", count, total);

    for (i = 0; i < count; i++)
    {
        pid = (i * 7) % total + 1;
        rc = apply_update(pid, "Updated", "Record", 999, "New Address", 50, 'U');
        if (rc != I_OK)
        {
            printf("Update failed at iteration %d (pid=%d) rc=%d\r\n", i, pid, rc);
            return rc;
        }
    }

    printf("Update complete: %d records updated\r\n", count);
    return I_OK;
}

int do_deletes()
{
    int rc;
    int total;
    int count;
    int deleted;
    int cursor;
    int step;
    int attempts;
    int maxattempts;
    int pid;
    int a;
    int b;
    int t;

    total = g_cfg.tbls[0].nrecs;
    if (total <= 0)
        return 1;

    count = total / 10;
    if (count <= 0)
        count = 1;

    rc = ensure_index();
    if (rc != I_OK)
        return rc;

    printf("Deleting %d records (10%% of %d)...\r\n", count, total);

    step = 3;
    if (total > 1)
    {
        while (step < total)
        {
            a = total;
            b = step;
            t = 0;
            while (b != 0)
            {
                t = a % b;
                a = b;
                b = t;
            }
            if (a == 1)
                break;
            step++;
        }
        if (step >= total)
            step = 1;
    }

    cursor = 0;
    deleted = 0;
    attempts = 0;
    maxattempts = total * 2;
    if (maxattempts < count * 2)
        maxattempts = count * 2;

    while (deleted < count && attempts < maxattempts)
    {
        pid = cursor + 1;
        cursor = (cursor + step) % total;
        attempts++;

        rc = delete_patient(pid);
        if (rc == I_OK)
        {
            deleted++;
            continue;
        }
        if (rc == I_ENREC)
            continue;

        printf("Delete failed at pid %d rc=%d\r\n", pid, rc);
        return rc;
    }

    if (deleted < count)
    {
        printf("Delete incomplete: removed %d of %d requested\r\n", deleted, count);
        return I_ENREC;
    }

    printf("Delete complete: %d records deleted\r\n", deleted);
    return I_OK;
}

main()
{
    int i;
    int rc;
    char rec[RECORD_SIZE];
    struct patient pat;

    namrdy = 0;
    g_idx_ready = 0;
    g_idx_dirty = 1;
    g_idxcnt = 0;

    puts("\r\nInitializing database...");
    initcfg();

    printf("Writing config: db=%s table=%s disk=%c recsz=%d\r\n",
        g_cfg.dbname, g_cfg.tbls[0].name, g_cfg.tbls[0].disk, g_cfg.tbls[0].recsz);

    rc = i_cfwr("PATIENTS.CFG");
    printf("i_cfwr returned: %d\r\n", rc);
    if (rc != I_OK)
    {
        puts("Config write failed");
        return 1;
    }
    puts("Config written successfully");

    puts("Creating table...");
    rc = i_mktbl("PATIENTS");
    printf("i_mktbl returned: %d\r\n", rc);
    if (rc != I_OK)
    {
        puts("Create table failed");
        return 1;
    }
    puts("Table created successfully\n");

    /* Initialize index before inserts */
    g_idxcnt = 0;
    g_idx_ready = 1;
    g_idx_dirty = 0;

    puts("Inserting records...\n");
    for (i = 0; i < P_CNT; i++)
    {
        make_patient(i + 1, &pat);
        bldrec(&pat, rec);
        rc = i_insrt("PATIENTS", rec, RECORD_SIZE);
        if (rc != I_OK)
        {
            printf("Insert failed at record %d rc=%d\r\n", i + 1, rc);
            return 1;
        }
        
        /* Update index every N records (sampling boundary) */
        if ((i + 1) % I_IDXSAMP == 0 || i == 0)
        {
            rc = i_idxins("PATIENTS", g_cfg.tbls[0].maxrec - 1, rec, 
                         g_idx, &g_idxcnt, I_MXIDX);
            if (rc != I_OK && rc != I_ESIZE)
            {
                printf("Index insert failed at record %d rc=%d\r\n", i + 1, rc);
            }
        }
        
        if ((i + 1) % 100 == 0)
            printf("  Inserted %d records (index: %d entries)...\r\n", 
                   i + 1, g_idxcnt);
    }
    printf("Insert complete: %d records inserted, %d index entries\r\n", 
           P_CNT, g_idxcnt);

    puts("Updating config with final counts...");
    rc = i_cfwr("PATIENTS.CFG");
    if (rc != I_OK)
    {
        puts("Config update failed");
        return 1;
    }

    puts("Performing record updates...");
    rc = do_updates();
    if (rc != I_OK)
    {
        puts("Record update sequence failed");
        return 1;
    }

    puts("Performing record deletions...");
    rc = do_deletes();
    if (rc != I_OK)
    {
        puts("Record delete sequence failed");
        return 1;
    }

    puts("Writing config after updates and deletions...");
    rc = i_cfwr("PATIENTS.CFG");
    if (rc != I_OK)
    {
        puts("Config write after maintenance failed");
        return 1;
    }

    printf("Index status: %d entries\r\n", g_idxcnt);

    puts("Running sample patient lookups...");
    rc = do_lookups();
    if (rc != I_OK)
    {
        puts("Lookup sequence failed");
        return 1;
    }

    if (readback() != 0)
        return 1;

    printf("\r\nSUCCESS! %d patient records remain in PATIENTS\r\n", g_cfg.tbls[0].nrecs);
    return 0;
}
