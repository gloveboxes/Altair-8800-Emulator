/* ============================================================
 * DXISAM2.H - ISAM Database Library Header (V2 - Minimal)
 * ============================================================
 * Starting fresh with just config file I/O
 */

/* Maximum limits */
#define I_MXTBL 16    /* Max tables per database */
#define I_MXKEY 4     /* Max keys per table */
#define I_MXNM 16     /* Max name length */
#define I_RECSZ 128   /* Fixed record size */

/* Return codes */
#define I_OK 0        /* Success */
#define I_EOPEN -1    /* Cannot open file */
#define I_EREAD -2    /* Read error */
#define I_EWRIT -3    /* Write error */
#define I_ECFG -9     /* Config error */

/* Table descriptor structure */
struct i_tbl {
    char name[I_MXNM];    /* Table name */
    char disk;            /* Disk drive (A-D) */
    int recsz;            /* Record size */
    int nkeys;            /* Number of keys */
    int keyoff[I_MXKEY];  /* Key field offsets */
    int keysz[I_MXKEY];   /* Key field sizes */
    int nrecs;            /* Current record count */
    int maxrec;           /* Max records */
};

/* Database config structure - MINIMAL VERSION with padding */
struct i_db {
    char dbname[I_MXNM];  /* Database name */
    int ntbls;            /* Number of tables */
    char pad[128];        /* Padding to prevent corruption */
};

/* Function declarations */
int i_cfrd();   /* Read config file: i_cfrd(db, filename) */
int i_cfwr();   /* Write config file: i_cfwr(db, filename) */
int i_cfnew();  /* Create new config: i_cfnew(db, dbname) */
