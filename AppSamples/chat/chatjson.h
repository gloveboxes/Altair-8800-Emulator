/* ============================================================
 * Chat JSON Handler for Altair 8800 - BDS C
 * Simple JSON parsing and generation for OpenAI API
 * ============================================================
 */

/* Message types */
#define MSG_SYS 0
#define MSG_USR 1
#define MSG_AST 2

/* Message length limits */
#define USR_LEN 256
#define AST_LEN 4096
#define MSG_LEN AST_LEN  /* History rows use assistant-length capacity */

/* Forward declaration */
struct msg_s;

/* JSON token types */
#define J_NULL 0
#define J_STR 1
#define J_NUM 2
#define J_OBJ 3
#define J_ARR 4
#define J_BOOL 5

/* JSON parsing functions */
int j_parse();     /* Parse JSON response and extract content */
int j_genreq();    /* Generate request JSON - takes separate arrays */
int j_add();       /* Add string to JSON buffer */
int j_addesc();    /* Add escaped string to JSON buffer */
int j_unesc();     /* Unescape JSON string in place */
int j_match2();    /* Match string at pointer */

/* JSON utility functions */
int j_skip();      /* Skip whitespace */

/* Config access helpers */
char *ch_gettok();
char *ch_gettmp();
char *ch_getmdl();
