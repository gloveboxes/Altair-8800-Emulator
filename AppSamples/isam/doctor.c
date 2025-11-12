#include "stdio.h"
#include "string.h"
#include "dxisam.h"

#define RECORD_SIZE 81

struct i_db g_cfg;

#define P_IDLN 5
#define P_NMLN 16
#define P_ADLN 40
#define P_CNT 600
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
    int i;
    int rc;
    int rid;
    int age;
    int ageoff;
    int max_phys;
    char rec[RECORD_SIZE];
    char fnamebuf[P_NMLN + 1];
    char lnamebuf[P_NMLN + 1];
    char addrbuf[P_ADLN + 1];
    char gender;

    /* Scan physical records directly using i_rdphys */
    max_phys = g_cfg.tbls[0].maxrec;
    for (i = 0; i < max_phys; i++)
    {
        rc = i_rdphys("PATIENTS", rec, i);
        if (rc != I_OK)
            continue;  /* Skip deleted records */

        rid = get_pid(rec);
        if (rid == pid)
        {
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
    }

    printf("Lookup %d -> not found\r\n", pid);
    return I_ENREC;
}

int do_lookups()
{
    int rc;

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
    int rc;
    int i;

    if (pid <= 0 || pid > g_cfg.tbls[0].nrecs)
        return I_ENREC;

    for (i = 0; i < RECORD_SIZE; i++)
        rec[i] = 0;

    setpid(rec, pid);
    copy_field(&rec[P_IDLN], fname, P_NMLN);
    copy_field(&rec[P_IDLN + P_NMLN], lname, P_NMLN);
    setadr(&rec[P_IDLN + (P_NMLN * 2)], housenum, streetname, P_ADLN);
    setnum(&rec[P_IDLN + (P_NMLN * 2) + P_ADLN], age);
    rec[P_IDLN + (P_NMLN * 2) + P_ADLN + 3] = gender;

    rc = i_uprec("PATIENTS", rec, RECORD_SIZE, pid - 1);
    if (rc != I_OK)
        return rc;

    return I_OK;
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
    int i;
    int total;
    int count;
    int idx;

    total = g_cfg.tbls[0].nrecs;
    if (total <= 0)
        return 1;

    count = total / 10;
    if (count <= 0)
        count = 1;

    printf("Deleting %d records (10%% of %d)...\r\n", count, total);

    for (i = 0; i < count; i++)
    {
        if (g_cfg.tbls[0].nrecs <= 0)
            break;

        idx = (i * 3) % g_cfg.tbls[0].nrecs;
        rc = i_delrec("PATIENTS", idx);
        if (rc != I_OK)
        {
            printf("Delete failed at iteration %d (idx=%d) rc=%d\r\n", i, idx, rc);
            return rc;
        }
    }

    printf("Delete complete: %d records deleted\r\n", count);
    return I_OK;
}

main()
{
    int i;
    int rc;
    char rec[RECORD_SIZE];
    struct patient pat;

    namrdy = 0;

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
    puts("Table created successfully");

    puts("Inserting records...");
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
        if ((i + 1) % 100 == 0)
            printf("  Inserted %d records...\r\n", i + 1);
    }
    printf("Insert complete: %d records inserted\r\n", P_CNT);

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
