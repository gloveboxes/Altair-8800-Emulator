# DXISAM ISAM Library

## Overview

DXISAM is a small Indexed Sequential Access Method (ISAM) library written for the Altair 8800 emulator environment. It targets BDS C 1.6 and CP/M-style low-level disk I/O, providing fixed-length record storage backed by simple configuration files. The library is used by several sample programs (such as `doctor.c` and `isamtst8.c`) to demonstrate how to model tabular data, persist it on disk, and perform inserts, updates, lookups, and lazy deletes.

Key characteristics:

- Fixed-size records stored in `disk:TABLE.DAT` files (e.g. `C:PATIENTS.DAT`).
- Text configuration (`*.CFG`) that describes databases and tables via the global `g_cfg` structure.
- Single-threaded, no dynamic allocation, designed for ≤200 KB data sets under CP/M.
- Lazy delete support that marks records and allows future inserts to reuse freed slots.

## How ISAM Works

ISAM stores records sequentially on disk while maintaining key ordering. Classic implementations complement sequential storage with an index tree so that records can be found quickly. DXISAM provides the sequential portion: records are appended (or written into deleted slots), and logical record numbers are mapped to physical slots. Logical numbering ignores deleted rows, so fetching record `n` always yields the `n`th active record.

In DXISAM:

- Records are fixed length and addressed by logical index.
- Keys are defined in the metadata (`keyoff`, `keysz`) but no secondary index is built yet; scans are performed with `i_rdphys` or helper logic in the application.
- Deletes simply mark the first byte of the physical record with `0xFF`, preserving file layout and enabling reuse.
- Configuration (`g_cfg`) tracks logical (`nrecs`) versus physical (`maxrec`) counts so that applications can iterate all slots quickly and decide whether a record is active.

The combination of logical-to-physical mapping plus lazy deletes gives predictable O(n) scans and avoids rewriting the data file, which is important on CP/M disks.

## DXISAM Concepts

- **Database configuration (`g_cfg`)**: Holds database name, table list, record sizes, key fields, and counters. Applications populate it before calling `i_cfwr`/`i_cfrd`.
- **Tables**: Each table entry names a disk (A–D), record size, and key metadata. DXISAM currently supports up to three tables in the sample build.
- **Records**: Fixed-length byte arrays (padding with zeros) that the caller constructs before passing to the library.
- **Logical vs. physical slots**: `nrecs` counts active rows; `maxrec` reflects the high-water mark of physical slots (including deleted entries). Traversing logical records uses `i_findlog`, while `i_rdphys` exposes raw slots for scans.
- **Lazy delete flag (`0xFF`)**: The first byte of a deleted record is set to `0xFF`. `i_insrt` scans for these markers and reuses the slot before growing the table.

### Return Codes

| Constant   | Value | Meaning                              |
|------------|-------|--------------------------------------|
| `I_OK`     | 0     | Success                              |
| `I_EOPEN`  | -1    | Failed to open or create a file      |
| `I_EWRIT`  | -3    | Write failed                         |
| `I_ENTBL`  | -4    | Table not found in configuration     |
| `I_ESIZE`  | -5    | Record size mismatch or too large    |
| `I_EREAD`  | -6    | Read failed                          |
| `I_ENREC`  | -7    | Invalid record number / deleted slot |
| `I_EUPDT`  | -8    | Update or delete rewrite failed      |

## API Reference

The functions below operate on the global `g_cfg` instance. Callers must populate `g_cfg` before creating tables or records.

| Function & Signature | Description |
|----------------------|-------------|
| `int i_cfwr(char *cfgname);` | Write the current `g_cfg` structure to a configuration file (e.g. `PATIENTS.CFG`). Creates or overwrites the file using CP/M `creat`/`write` calls. |
| `int i_cfrd(char *cfgname);` | Load `g_cfg` from a configuration file written by `i_cfwr`. Updates table metadata and record counters. |
| `int i_mktbl(char *table);` | Create an empty data file (`disk:TABLE.DAT`) for the named table defined in `g_cfg`. Validates the table entry first. |
| `int i_insrt(char *table, char *record, int size);` | Insert a record. Verifies the size, scans for a deleted slot (`0xFF`) to reuse, and appends only if no free slot exists. Updates `nrecs` and, when necessary, `maxrec`. |
| `int i_rdrec(char *table, char *dest, int logicalIndex);` | Read an active record by logical index (0-based). Uses `i_findlog` internally to skip deleted slots. Returns `I_ENREC` if the index is invalid. |
| `int i_rdphys(char *table, char *dest, int physicalIndex);` | Read a physical slot without logical filtering. Returns `I_ENREC` when the slot is marked deleted; useful for full scans and diagnostics. |
| `int i_uprec(char *table, char *record, int size, int logicalIndex);` | Update a logical record in place. Writes a full record buffer back to the corresponding physical slot. |
| `int i_delrec(char *table, int logicalIndex);` | Lazily delete a record by logical index. Sets the first byte to `0xFF` and decrements `nrecs`, leaving `maxrec` unchanged for reuse. |

### Helpers and Globals

- `struct i_db g_cfg;` must remain in sync between calls. After inserts/deletes, applications typically call `i_cfwr` again to persist updated counters.
- `i_findlog` is an internal helper exposed in `dxisam2.c` that maps logical indices to physical slots. Most applications rely on `i_rdrec` instead of calling it directly.

## Usage Examples

### Creating a table and inserting records (`doctor.c`)

```c
/* Prepare configuration */
initcfg();
strncpy(g_cfg.tbls[0].name, "PATIENTS", I_MXNM);
g_cfg.tbls[0].disk = 'C';
g_cfg.tbls[0].recsz = 81;
g_cfg.tbls[0].nkeys = 1;
g_cfg.tbls[0].keyoff[0] = 0;
g_cfg.tbls[0].keysz[0] = 5;
g_cfg.tbls[0].nrecs = 0;
g_cfg.tbls[0].maxrec = 0;

i_cfwr("PATIENTS.CFG");      /* Persist metadata */
i_mktbl("PATIENTS");          /* Create C:PATIENTS.DAT */

/* Insert patient records */
for (i = 0; i < P_CNT; i++) {
	make_patient(i + 1, &pat);
	bldrec(&pat, rec);        /* Build fixed-length buffer */
	i_insrt("PATIENTS", rec, RECORD_SIZE);
}

i_cfwr("PATIENTS.CFG");      /* Save updated counts */
```

This pattern configures the table, writes the `.CFG`, creates the `.DAT`, and loads it with fixed-size patient records. `i_insrt` handles slot reuse automatically, so the caller only needs to build the correct record image.

### Updating and deleting records (`doctor.c`)

```c
/* Update roughly 10% of rows */
pid = (i * 7) % total + 1;             /* Logical ID */
apply_update(pid, "Updated", "Record", 999, "New Address", 50, 'U');

/* Lazy delete roughly 10% of rows */
idx = (i * 3) % g_cfg.tbls[0].nrecs;   /* Logical index */
i_delrec("PATIENTS", idx);

i_cfwr("PATIENTS.CFG");               /* Persist new counts */
```

`apply_update` rebuilds the full record buffer and calls `i_uprec`, which overwrites the physical slot in place. `i_delrec` marks each selected record as deleted and decrements `nrecs`, enabling later inserts to recycle those slots without growing the file.

### Inspecting physical slots and slot reuse (`isamtst8.c`)

```c
/* Scan every physical slot and sample deleted ones */
for (i = 0; i < g_cfg.tbls[0].maxrec; i++) {
	if (i_rdphys("NAMES", rbuf, i) == I_ENREC) {
		sample_slots[sample_count++] = i;
		continue;
	}
	printf("Rec %d: ID=%.4s\r\n", count + 1, rbuf);
}

/* Insert new records and confirm reuse */
for (i = 0; i < 3; i++) {
	/* build rec[...] */
	i_insrt("NAMES", rec, 32);
}

printf("After inserts: nrecs=%d maxrec=%d\r\n",
	g_cfg.tbls[0].nrecs, g_cfg.tbls[0].maxrec);

/* Show that deleted slots now contain the new rows */
for (i = 0; i < sample_count; i++) {
	i_rdphys("NAMES", rbuf, sample_slots[i]);
	printf("Slot %d -> ID=%.4s\r\n", sample_slots[i], rbuf);
}
```

The `isamtst8` utility demonstrates the lazy-delete strategy end to end: records are deleted, new inserts reuse the freed physical slots, and `maxrec` remains unchanged. Reloading the configuration via `i_cfrd` proves that the persisted counters survive restarts.

## Next Steps

- Extend the metadata to support more tables or optional secondary indices.
- Add higher-level lookup helpers on top of `i_rdphys` for key-based searches.
- Integrate automated rebuild utilities if a clean physical rewrite is ever required.

Refer to `doctor.c` and `isamtst8.c` for complete, runnable examples within the emulator environment.
