/* ============================================================
 * LLM RULES FOR GENERATING BDS C CODE (Altair 8800 / CP/M)
 * ============================================================
 *
 * 1. Syntax:
 *    - Use K&R (BDS C) style: return_type name(args) on next line
 *    - No ANSI prototypes, no "void", no modern keywords
 *    - All function definitions and calls must follow BDS C rules
 *
 * 2. Symbols (VERY IMPORTANT):
 *    - All symbol names (functions, variables, labels, statics, globals)
 *      must be unique in their first 7 characters
 *    - Prefer short, descriptive names, e.g. "x_delay", "x_tmrset"
 *    - Avoid underscores beyond the leading "x_" unless necessary
 *    - Do not exceed 7 characters for clarity and linker safety
 *
 * 3. Types:
 *    - Use int or unsigned (16-bit) for parameters and locals
 *    - Use long.c for longs
 *    - Explicitly declare return type (no implicit int)
 *
 *
 * 6. Style:
 *    - Add a short comment block before each function
 *    - Keep indentation simple (max 4 spaces)
 *    - No C99/C89 features (stick to 1980-era BDS C)
 *
 * 7. The app runs on CP/M single tasking OS, only one app runs at a time
 * ============================================================
 */


unsigned x_rand(); /* Get random number */
int x_altr();     /* Get Altair emulator version */
int x_uptime();   /* Get system uptime */
int x_cur_utc();  /* Get current UTC time */
int x_local();    /* Get current local time */
int x_temp();     /* Get PI Sense HAT temperature */
int x_press();    /* Get PI Sense HAT pressure */
int x_light();    /* Get PI Sense HAT light sensor */
int x_humid();    /* Get PI Sense HAT humidity */
int x_wkey();     /* Get weather key by index */
int x_wval();     /* Get weather value by index */
int x_lkey();     /* Get location key by index */
int x_lval();     /* Get location value by index */
int x_pkey();     /* Get pollution key by index */
int x_pval();     /* Get pollution value by index */
