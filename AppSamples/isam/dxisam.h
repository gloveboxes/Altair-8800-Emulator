#ifndef DXISAM_H
#define DXISAM_H

/* ============================================================
 * DXISAM2.H - ISAM Database Library Header (V2)
 * ============================================================
 * Shared definitions for DXISAM table management and record I/O.
 */

/* Maximum limits (match dxisam.c) */
#define I_MXTBL 3     /* Max tables per database */
#define I_MXKEY 4     /* Max keys per table */
#define I_MXNM 16     /* Max name length */
#define I_RECSZ 128   /* Max fixed record size */

/* Return codes */
#define I_OK 0        /* Success */
#define I_EOPEN -1    /* Cannot open file */
#define I_EWRIT -3    /* Write error */
#define I_ENTBL -4    /* Table not found */
#define I_ESIZE -5    /* Record size mismatch */
#define I_EREAD -6    /* Read error */
#define I_ENREC -7    /* Invalid record number */
#define I_EUPDT -8    /* Update/delete failure */

/* Delete marker used for lazy delete */
#define I_DELFLAG 0xFF

/* Table descriptor structure */
struct i_tbl {
    char name[I_MXNM];    /* Table name */
    char disk;            /* Disk drive (A-D) */
    int recsz;            /* Record size */
    int nkeys;            /* Number of keys */
    int keyoff[I_MXKEY];  /* Key field offsets */
    int keysz[I_MXKEY];   /* Key field sizes */
    int nrecs;            /* Logical record count */
    int maxrec;           /* Physical high-water mark */
};

/* Database config structure shared by callers */
struct i_db {
    char dbname[I_MXNM];              /* Database name */
    int ntbls;                        /* Number of tables */
    struct i_tbl tbls[I_MXTBL];       /* Table descriptors */
    char pad[128];                    /* Padding for BDS C alignment */
};

/* Function declarations (K&R style to match BDS C) */
int i_cfrd();    /* Load config file into g_cfg */
int i_cfwr();    /* Write g_cfg back to disk */
int i_mktbl();   /* Create table data file */
int i_insrt();   /* Insert record into table */
int i_rdrec();   /* Read logical record */
int i_rdphys();  /* Read physical record slot */
int i_uprec();   /* Update logical record */
int i_delrec();  /* Lazy delete logical record */

#endif /* DXISAM_H */
