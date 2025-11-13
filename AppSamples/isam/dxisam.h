#ifndef DXISAM_H
#define DXISAM_H

/* ============================================================
 * DXISAM2.H - ISAM Database Library Header (V2)
 * ============================================================
 * Shared definitions for DXISAM table management and record I/O.
 */

/* Maximum limits (match implementation) */
#define I_MXTBL 3     /* Max tables per database */
#define I_MXKEY 4     /* Max keys per table */
#define I_MXNM 16     /* Max name length */
#define I_RECSZ 128   /* Max fixed record size */

/* Low-level CP/M geometry */
#define I_NSECTS 8
#define I_SECSZ 128
#define I_BUFSZ (I_NSECTS * I_SECSZ)

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

/* Index constants */
#define I_MXKEYLN 12      /* Max key length in index */
#define I_IDXSAMP 20      /* Sample every Nth record for index */
#define I_MXIDX 100       /* Max index entries in memory */

/* Index entry - stores sampled key + physical slot */
struct i_idxent {
    char key[I_MXKEYLN];  /* Key value (zero-padded) */
    int phys;             /* Physical slot number */
};

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
    int idxcnt;           /* Number of index entries */
    int idxsamp;          /* Sample rate (0 = no index) */
};

/* Database config structure shared by callers */
struct i_db {
    char dbname[I_MXNM];              /* Database name */
    int ntbls;                        /* Number of tables */
    struct i_tbl tbls[I_MXTBL];       /* Table descriptors */
    char pad[128];                    /* Padding for BDS C alignment */
};

#ifdef __unix__
#define DX_EXTERN extern
#endif
#ifdef __APPLE__
#define DX_EXTERN extern
#endif
#ifdef _WIN32
#define DX_EXTERN extern
#endif
#ifndef DX_EXTERN
#define DX_EXTERN
#define DX_COMMON_LINKAGE
#endif

DX_EXTERN struct i_db g_cfg;

/* Function declarations (K&R style to match BDS C) */
int i_cfrd();    /* Load config file into g_cfg */
int i_cfwr();    /* Write g_cfg back to disk */
int i_mktbl();   /* Create table data file */
int i_insrt();   /* Insert record into table */
int i_rdrec();   /* Read logical record */
int i_rdphys();  /* Read physical record slot */
int i_wrphys();  /* Write physical record slot */
int i_delphys(); /* Delete physical record slot */
int i_uprec();   /* Update logical record */
int i_delrec();  /* Lazy delete logical record */
int i_idxbld();  /* Build sparse index for table */
int i_idxsrch(); /* Binary search index for key */
int i_idxlookup(); /* High-level indexed lookup */
int i_idxins();  /* Insert/update single index entry */
int i_idxdel();  /* Remove index entry by physical slot */

#endif /* DXISAM_H */
