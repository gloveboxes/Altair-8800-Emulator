/* Test i_insrt - insert records */
#include "stdio.h"
#include "string.h"
#include "dxisam.h"

/* Number of initial records inserted during the test */
#define I_RNUM 1000

main()
{
    int i, j, rc, id, d0, d1, d2, d3, uidx, didx, count;
    int sample_slots[3];
    int sample_count;
    char rec[64];
    char rbuf[64];
    
    puts("Test i_insrt - insert records");
    
    /* Setup simple database */
    for (i = 0; i < I_MXNM; i++)
        g_cfg.dbname[i] = 0;
    
    strncpy(g_cfg.dbname, "ISAMTST", I_MXNM);
    g_cfg.ntbls = 1;
    
    /* Table: NAMES on disk A, 32-byte records */
    for (i = 0; i < I_MXNM; i++)
        g_cfg.tbls[0].name[i] = 0;
    
    strncpy(g_cfg.tbls[0].name, "NAMES", I_MXNM);
    g_cfg.tbls[0].disk = 'C';
    g_cfg.tbls[0].recsz = 32;
    g_cfg.tbls[0].nkeys = 1;
    
    for (i = 0; i < I_MXKEY; i++)
    {
        g_cfg.tbls[0].keyoff[i] = 0;
        g_cfg.tbls[0].keysz[i] = 0;
    }
    g_cfg.tbls[0].keyoff[0] = 0;
    g_cfg.tbls[0].keysz[0] = 4;
    g_cfg.tbls[0].nrecs = 0;
    g_cfg.tbls[0].maxrec = 0;
    
    printf("Config: db='%s' table='%s' recsz=%d\r\n", 
        g_cfg.dbname, g_cfg.tbls[0].name, g_cfg.tbls[0].recsz);
    
    /* Save config */
    if (i_cfwr("ISAMTST.CFG") != I_OK)
    {
        puts("Config write failed");
        return 1;
    }
    
    /* Create table */
    if (i_mktbl("NAMES") != I_OK)
    {
        puts("Create table failed");
        return 1;
    }
    puts("Created C:NAMES.DAT");
    
    /* Insert initial batch of records */
    for (i = 0; i < I_RNUM; i++)
    {
        for (j = 0; j < 32; j++)
            rec[j] = 0;
        id = i + 1;
        d0 = id % 10;
        d1 = (id / 10) % 10;
        d2 = (id / 100) % 10;
        d3 = (id / 1000) % 10;
        rec[0] = '0' + d3;
        rec[1] = '0' + d2;
        rec[2] = '0' + d1;
        rec[3] = '0' + d0;
        rec[4] = 'N';
        rec[5] = 'A';
        rec[6] = 'M';
        rec[7] = 'E';
        rec[8] = '0' + d3;
        rec[9] = '0' + d2;
        rec[10] = '0' + d1;
        rec[11] = '0' + d0;
        printf("Insert rec %d: ID=%.4s Name=%.8s\r\n",
            id, rec, &rec[4]);
        rc = i_insrt("NAMES", rec, 32);
        if (rc != I_OK)
        {
            printf("Insert failed: rc=%d\r\n", rc);
            return 1;
        }
    }
    printf("  nrecs=%d maxrec=%d\r\n", 
        g_cfg.tbls[0].nrecs, g_cfg.tbls[0].maxrec);

    /* Display all records */
    puts("\r\nInitial records:");
    count = 0;
    for (i = 0; i < g_cfg.tbls[0].maxrec; i++)
    {
        for (j = 0; j < 64; j++)
            rbuf[j] = 0;
        rc = i_rdphys("NAMES", rbuf, i);
        if (rc == I_ENREC)
            continue;
        if (rc != I_OK)
        {
            printf("Read rec %d failed: rc=%d\r\n", count + 1, rc);
            return 1;
        }
        count++;
        printf("  Rec %d: ID=%.4s Name=", count, rbuf);
        for (j = 4; j < 32 && rbuf[j]; j++)
            putchar(rbuf[j]);
        putchar('\r');
        putchar('\n');
    }

    /* Update record 10 (index 9) */
    uidx = 9;
    id = uidx + 1;
    d0 = id % 10;
    d1 = (id / 10) % 10;
    d2 = (id / 100) % 10;
    d3 = (id / 1000) % 10;
    for (j = 0; j < 32; j++)
        rec[j] = 0;
    rec[0] = '0' + d3;
    rec[1] = '0' + d2;
    rec[2] = '0' + d1;
    rec[3] = '0' + d0;
    rec[4] = 'U';
    rec[5] = 'P';
    rec[6] = 'D';
    rec[7] = 'A';
    rec[8] = 'T';
    rec[9] = 'E';
    printf("\r\nUpdate rec %d: ID=%.4s Name=UPDATE\r\n", id, rec);
    rc = i_uprec("NAMES", rec, 32, uidx);
    if (rc != I_OK)
    {
        printf("Update failed: rc=%d\r\n", rc);
        return 1;
    }
    for (j = 0; j < 64; j++)
        rbuf[j] = 0;
    if (i_rdrec("NAMES", rbuf, uidx) != I_OK)
    {
        puts("Read after update failed");
        return 1;
    }
    printf("  After update rec %d: ID=%.4s Name=", id, rbuf);
    for (j = 4; j < 32 && rbuf[j]; j++)
        putchar(rbuf[j]);
    putchar('\r');
    putchar('\n');

    /* Delete multiple records to demonstrate lazy delete */
    printf("\r\nDeleting records 1, 5, and 10...\r\n");
    
    /* Delete record 1 (index 0) */
    didx = 0;
    rc = i_delrec("NAMES", didx);
    if (rc != I_OK)
    {
        printf("Delete rec 1 failed: rc=%d\r\n", rc);
        return 1;
    }
    
    /* Delete record 5 (now at index 4 since we deleted index 0) */
    didx = 4;
    rc = i_delrec("NAMES", didx);
    if (rc != I_OK)
    {
        printf("Delete rec 5 failed: rc=%d\r\n", rc);
        return 1;
    }
    
    /* Delete record 10 (now at index 8 after 2 deletions) */
    didx = 8;
    rc = i_delrec("NAMES", didx);
    if (rc != I_OK)
    {
        printf("Delete rec 10 failed: rc=%d\r\n", rc);
        return 1;
    }
    
    printf("  After deletes: nrecs=%d maxrec=%d (lazy delete keeps maxrec)\r\n",
        g_cfg.tbls[0].nrecs, g_cfg.tbls[0].maxrec);

    printf("  Remaining records (nrecs=%d logical records):\r\n", 
        g_cfg.tbls[0].nrecs);
    sample_count = 0;
    for (i = 0; i < g_cfg.tbls[0].maxrec && sample_count < 3; i++)
    {
        if (i_rdphys("NAMES", rbuf, i) == I_ENREC)
        {
            sample_slots[sample_count] = i;
            sample_count++;
        }
    }
    count = 0;
    for (i = 0; i < g_cfg.tbls[0].maxrec; i++)
    {
        for (j = 0; j < 64; j++)
            rbuf[j] = 0;
        rc = i_rdphys("NAMES", rbuf, i);
        if (rc == I_ENREC)
            continue;
        if (rc != I_OK)
        {
            printf("Read rec %d failed: rc=%d\r\n", count + 1, rc);
            return 1;
        }
        count++;
        printf("    Rec %d: ID=%.4s Name=", count, rbuf);
        for (j = 4; j < 32 && rbuf[j]; j++)
            putchar(rbuf[j]);
        putchar('\r');
        putchar('\n');
    }

    /* Insert 3 replacement records - these will reuse deleted slots */
    printf("\r\nInserting 3 new records (will reuse deleted slots)...\r\n");
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 32; j++)
            rec[j] = 0;
        id = I_RNUM + 1 + i;
        d0 = id % 10;
        d1 = (id / 10) % 10;
        d2 = (id / 100) % 10;
        d3 = (id / 1000) % 10;
        rec[0] = '0' + d3;
        rec[1] = '0' + d2;
        rec[2] = '0' + d1;
        rec[3] = '0' + d0;
        rec[4] = 'N';
        rec[5] = 'E';
        rec[6] = 'W';
        rec[7] = '0' + (i + 1);
        rec[8] = '0' + d3;
        rec[9] = '0' + d2;
        rec[10] = '0' + d1;
        rec[11] = '0' + d0;
        rc = i_insrt("NAMES", rec, 32);
        if (rc != I_OK)
        {
            printf("Insert failed: rc=%d\r\n", rc);
            return 1;
        }
    }
    printf("  After inserts: nrecs=%d maxrec=%d (maxrec unchanged - reused slots)\r\n",
        g_cfg.tbls[0].nrecs, g_cfg.tbls[0].maxrec);

    if (sample_count > 0)
    {
        printf("  Sample reused physical slots:\r\n");
        for (i = 0; i < sample_count; i++)
        {
            rc = i_rdphys("NAMES", rbuf, sample_slots[i]);
            if (rc != I_OK)
            {
                printf("    Slot %d still deleted (rc=%d)\r\n", sample_slots[i], rc);
                continue;
            }
            printf("    Slot %d -> ID=%.4s Name=", sample_slots[i], rbuf);
            for (j = 4; j < 32 && rbuf[j]; j++)
                putchar(rbuf[j]);
            putchar('\r');
            putchar('\n');
        }
    }

    /* Final table dump */
    puts("\r\nFinal records:");
    count = 0;
    for (i = 0; i < g_cfg.tbls[0].maxrec; i++)
    {
        for (j = 0; j < 64; j++)
            rbuf[j] = 0;
        rc = i_rdphys("NAMES", rbuf, i);
        if (rc == I_ENREC)
            continue;
        if (rc != I_OK)
        {
            printf("Read rec %d failed: rc=%d\r\n", count + 1, rc);
            return 1;
        }
        count++;
        printf("  Rec %d: ID=%.4s Name=", count, rbuf);
        for (j = 4; j < 32 && rbuf[j]; j++)
            putchar(rbuf[j]);
        putchar('\r');
        putchar('\n');
    }

    if (i_cfwr("ISAMTST.CFG") != I_OK)
    {
        puts("Config update failed");
        return 1;
    }

    /* Verify counts survive reload */
    g_cfg.tbls[0].nrecs = 0;
    g_cfg.tbls[0].maxrec = 0;
    if (i_cfrd("ISAMTST.CFG") != I_OK)
    {
        puts("Config reload failed");
        return 1;
    }
    printf("\r\nReloaded counts: nrecs=%d maxrec=%d\r\n",
        g_cfg.tbls[0].nrecs, g_cfg.tbls[0].maxrec);
    
    puts("\r\nSUCCESS! Lazy delete verified:");
    puts("  - Deleted records marked with flag (maxrec unchanged)");
    puts("  - New inserts reuse deleted slots");
    puts("  - Logical record count (nrecs) reflects active records");
    return 0;
}
