#define ZCCDEBUG 0

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>
#include <zero/param.h>
#include <zcc/zcc.h>

#define ZCC_FILE_ERROR (-1)

#define NVALHASH 4096
#define NFILE  1024
#define NLINEBUF 4096

static int zccreadfile(char *name, int curfile);

#define zccisoper(cp)    (opertab[(int)(*(cp))])
#define zccistypedef(cp) (!strncmp(cp, "typedef", 7))
#define zccisstruct(cp)  (!strncmp(cp, "struct", 6))
#define zccisunion(cp)   (!strncmp(cp, "union", 5))
#define zccisatr(cp)     (!strncmp(cp, "__attribute__", 13))
#define zccispreproc(cp) (*str == '#')
#define zccisdefine(cp)  (!strncmp(cp, "#define", 7))
#define zccqualid(cp)                                                   \
    ((!strncmp(cp, "extern", 6))                                        \
     ? ZCC_EXTERN_QUAL                                                  \
     : (!strncmp(cp, "static", 6)                                       \
        ? ZCC_STATIC_QUAL                                               \
        : (!strncmp(cp, "const", 5)                                     \
           ? ZCC_CONST_QUAL                                             \
           : (!strncmp(cp, "volatile", 8)                               \
              ? ZCC_VOLATILE_QUAL                                       \
              : ZCC_NONE))))
#define zccpreprocid(cp)                                                \
    ((!strncmp(cp, "if", 2))                                            \
     ? ZCC_IF_DIR                                                       \
     : (!strncmp(cp, "elif", 4)                                         \
        ? ZCC_ELIF_DIR                                                  \
        : (!strncmp(cp, "else", 4)                                      \
           ? ZCC_ELSE_DIR                                               \
           : (!strncmp(cp, "endif", 5)                                  \
              ? ZCC_ENDIF_DIR                                           \
              : (!strncmp(cp, "ifdef", 5)                               \
                 ? ZCC_IFDEF_DIR                                        \
                 : (!strncmp(cp, "ifndef", 6)                           \
                    ? ZCC_IFNDEF_DIR                                    \
                    : (!strncmp(cp, "define", 6)                        \
                       ? ZCC_DEFINE_DIR                                 \
                       : ZCC_NONE)))))))
#define zccisagr(t)      ((t) == ZCC_STRUCT || (t) == ZCC_UNION)

#define zccvalsz(t)      (typesztab[(t)])
#define zcctypesz(tok)   (tok->datasz)
#define zccsetival(vp, t, adr)                                          \
    (typesigntab[(t) & 0x1f]                                            \
     ? ((vp)->ival.ll = *((long long *)(adr)))                          \
     : ((vp)->ival.ull = *((unsigned long long *)(adr))))
#define zccsetfval(vp, t, adr)                                          \
    do {                                                                \
        if ((t) == ZCC_FLOAT) {                                         \
            (vp)->fval.f = *(float *)(adr);                             \
        } else if ((t) == ZCC_DOUBLE) {                                 \
            (vp)->fval.d = *(double *)(adr);                            \
        } else if ((t) == ZCC_LONG_DOUBLE) {                            \
            (vp)->fval.d = *(long double *)(adr);                       \
        }                                                               \
    } while (0)

static uint8_t            opertab[256];
static uint8_t            toktab[256];
/* for type indices, see struct zccval in zcc.h (ZCC_INT etc.) */
static long               typesztab[32] = {
    0,                  // ZCC_NONE
    1,                  // ZCC_CHAR
    1,                  // ZCC_UCHAR
    2,                  // ZCC_SHORT
    2,                  // ZCC_USHORT
    4,                  // ZCC_INT
    4,                  // ZCC_UINT
    LONGSIZE,           // ZCC_LONG
    LONGSIZE,           // ZCC_ULONG
    LONGLONGSIZE,       // ZCC_LONGLONG
    LONGLONGSIZE        // ZCC_ULONGLONG
};
static long               typesigntab[32] = {
    0,                  // ZCC_NONE
    1,                  // ZCC_CHAR
    0,                  // ZCC_UCHAR
    1,                  // ZCC_SHORT
    0,                  // ZCC_USHORT
    1,                  // ZCC_INT
    0,                  // ZCC_UINT
    1,                  // ZCC_LONG
    0,                  // ZCC_ULONG
    1,                  // ZCC_LONGLONG
    0                   // ZCC_ULONGLONG
};
static long               parmlentab[16] = {
    0,                  // ZCC_NONE
    6,                  // ZCC_EXTERN_QUAL
    6,                  // ZCC_STATIC_QUAL
    5,                  // ZCC_CONST_QUAL
    8,                  // ZCC_VOLATILE_QUAL
    2,                  // ZCC_IF_DIR
    4,                  // ZCC_ELIF_DIR
    4,                  // ZCC_ELSE_DIR
    5,                  // ZCC_ENDIF_DIR
    5,                  // ZCC_IFDEF_DIR
    6                   // ZCC_IFNDEF_DIR
};
static struct zcctokenq **zccfiletokens;
static int                zcccurfile;
static int                zccnfiles;
static long               zccoptflags;
static char               linebuf[NLINEBUF];

static void
zccusage(void)
{
    fprintf(stderr, "usage <options> [file1] .. [fileN]\n");
    fprintf(stderr, "\t-h OR --help\tprint this help message\n");

    return;
}

static void
zccinitopertab(void)
{
    opertab['!'] = '!';
    opertab['~'] = '~';
    opertab['&'] = '&';
    opertab['|'] = '|';
    opertab['^'] = '^';
    opertab['<'] = '<';
    opertab['>'] = '>';
    opertab['+'] = '+';
    opertab['-'] = '-';
    opertab['*'] = '*';
    opertab['/'] = '/';
    opertab['%'] = '%';
    opertab['='] = '=';
}

static void
zccinittoktab(void)
{
    toktab['.'] = ZCC_DOT_TOKEN;
    toktab[','] = ZCC_COMMA_TOKEN;
    toktab[';'] = ZCC_SEMICOLON_TOKEN;
    toktab['{'] = ZCC_BLOCK_TOKEN;
    toktab['}'] = ZCC_END_BLOCK_TOKEN;
    toktab['?'] = ZCC_EXCLAMATION_TOKEN;
    toktab[':'] = ZCC_COLON_TOKEN;
    toktab['('] = ZCC_LEFT_PAREN_TOKEN;
    toktab[')'] = ZCC_RIGHT_PAREN_TOKEN;
    toktab['['] = ZCC_INDEX_TOKEN;
    toktab[']'] = ZCC_END_INDEX_TOKEN;
    toktab['\''] = ZCC_QUOTE_TOKEN;
    toktab['"'] = ZCC_DOUBLE_QUOTE_TOKEN;
    toktab['\\'] = ZCC_BACKSLASH_TOKEN;
}

static int
zccinit(int argc,
           char *argv[])
{
    int          l;
    char        *str;

    if (argc == 1) {
        fprintf(stderr, "%s: arguments needed\n", argv[0]);

        return 0;
    }
    zccinitopertab();
    zccinittoktab();
    for (l = 1 ; l < argc ; l++) {
        str = argv[l];
        if (*str == '-') {
            if (!strcmp(str, "-h") || !strcmp(str, "--help")) {
                zccusage();
                
                exit(0);
            } else if (!strcmp(str, "-O")) {
                zccoptflags |= ZCC_OPT_ALIGN;
            }
        } else {

            break;
        }
    }
    zccfiletokens = malloc(NFILE * sizeof(struct zcctokenq));
    zccnfiles = NFILE;

    return l;
}

/* TODO: floating-point values */
static struct zccval *
zccgetval(char *str, char **retptr)
{
    long                type = ZCC_NONE;
    unsigned long long  uval = 0;
    long                found = 0;
    long long           val;
    long                neg = 0;
    struct zccval      *newval = NULL;

#if (ZCCDEBUG)
    fprintf(stderr, "getval: %s\n", str);
#endif
    if (*str == '-') {
        neg = 1;
        str++;
    }
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
        while ((*str) && isxdigit(*str)) {
            uval <<= 4;
            uval += (isdigit(*str)
                     ? *str - '0'
                     : (islower(*str)
                        ? *str - 'a' + 10
                        : *str - 'A' + 10));
            str++;
        }
        type = ZCC_INT;
        found = 1;
    } else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B')) {
        str += 2;
        while ((*str) && (*str == '0' || *str == '1')) {
            uval <<= 1;
            uval += *str - '0';
            str++;
        }
        type = ZCC_INT;
        found = 1;
    } else if (*str == '0') {
        str++;
        while ((*str) && isdigit(*str)) {
            if (*str > '7') {
                fprintf(stderr, "invalid number in octal constant: %s\n", str);

                exit(1);
            }
            uval <<= 3;
            uval += *str - '0';
            str++;
        }
        type = ZCC_INT;
        found = 1;
    } else if (isdigit(*str)) {
        while ((*str) && isdigit(*str)) {
            uval *= 10;
            uval += *str - '0';
            str++;
        }
        type = ZCC_INT;
        found = 1;
    }
    if (found) {
        newval = malloc(sizeof(struct zccval));
        if (neg) {
            val = -uval;
            if (*str == 'L' || *str == 'l') {
                str++;
                if (*str == 'L' || *str == 'l') {
                    type = ZCC_LONGLONG;
                } else {
                    type = ZCC_LONG;
                }
            }
        } else {
            if (*str == 'U' || *str == 'u') {
                str++;
                if (*str == 'L' || *str == 'l') {
                    str++;
                    if (*str == 'L' || *str == 'l') {
                        type = ZCC_ULONGLONG;
                    } else {
                        type = ZCC_ULONG;
                    }
                } else {
                    type = ZCC_UINT;
                }
            }
        }
        newval->type = type;
        newval->sz = zccvalsz(type);
        if (neg) {
            zccsetival(newval, type, &val);
        } else {
            zccsetival(newval, type, &uval);
        }
        while (*str == ',' || *str == '\'') {
            str++;
        }
        *retptr = str;
    }

    return newval;
}

static struct zcctoken *
zccgettoken(char *str, char **retstr)
{
    char            *cp = str;
    long             len;
    long             parm;
    long             type;
    char            *ptr;
    struct zcctoken *token = malloc(sizeof(struct zcctoken));
    struct zccval   *val;

#if (ZCCDEBUG)
    fprintf(stderr, "gettoken: %s\n", str);
#endif
    token->parm = ZCC_NONE;
    token->str = NULL;
    token->adr = NULL;
    if (*str == '-' && str[1] == '>') {
        token->type = ZCC_INDIR_TOKEN;
        str += 2;
    } else if (zccisoper(str)) {
        token->type = ZCC_OPER_TOKEN;
        token->str = malloc(8);
        ptr = token->str;
        while (zccisoper(str)) {
            *ptr++ = *str++;
        }
        *ptr = '\0';
    } else if ((type = toktab[(int)(*str)])) {
        token->type = type;
        str++;
    } else if (*str == '#') {
        str++;
        while (isspace(*str)) {
            str++;
        }
        fprintf(stderr, "STR: %s\n", str);
        parm = zccpreprocid(str);
        if (parm) {
            len = parmlentab[parm];
            token->type = ZCC_PREPROC_TOKEN;
            token->parm = parm;
            str += len;
        } else if (*str == '#') {
            str++;
            token->type = ZCC_CONCAT_TOKEN;
        } else {
            str++;
            token->type = ZCC_STRINGIFY_TOKEN;
        }
    } else if (zccistypedef(str)) {
        token->type = ZCC_TYPEDEF_TOKEN;
        str += 7;
    } else if (zccisatr(str)) {
        token->type = ZCC_ATR_TOKEN;
        str += 13;
    } else if ((parm = zccqualid(str))) {
        len = parmlentab[parm];
        token->type = ZCC_QUAL_TOKEN;
        token->parm = parm;
        str += len;
    } else if (zccisstruct(str)) {
        token->type = ZCC_STRUCT_TOKEN;
        str += 6;
    } else if (zccisunion(str)) {
        token->type = ZCC_UNION_TOKEN;
        str += 5;
    } else if (isalpha(*str) || *str == '_') {
        ptr = cp;
        while (isalnum(*str) || *str == '_') {
            str++;
        }
        if (*str == ':') {
            token->type = ZCC_LABEL_TOKEN;
            token->str = strndup(ptr, str - ptr);
            token->adr = ZCC_NO_ADR;
            str++;
        } else {
            token->type = ZCC_VAR_TOKEN;
            token->str = strndup(ptr, str - ptr);
            token->adr = ZCC_NO_ADR;
        }
    } else if (isxdigit(*str)) {
        ptr = str;
        val = zccgetval(str, &str);
        if (val) {
            token->type = ZCC_VALUE_TOKEN;
            token->str = strndup(ptr, str - ptr);
            token->adr = val;
        }
    } else {
        free(token);
        token = NULL;
    }
    if (token) {
        while (isspace(*str)) {
            str++;
        }
        *retstr = str;
    }

    return token;
}

static int
zccgetinclude(char *str, char **retstr, int curfile)
{    
    char  path[] = "/include/:/usr/include/";
    char  name[PATH_MAX + 1] = { '\0' };
    int   ret = ZCC_FILE_ERROR;
    char *ptr = name;
    char *cp;

#if (ZCCDEBUG)
    fprintf(stderr, "getinclude: %s\n", str);
#endif
    if (curfile == zccnfiles) {
        zccnfiles <<= 1;
        zccfiletokens = realloc(zccfiletokens,
                                zccnfiles * sizeof(struct zcctokenq));
        if (!zccfiletokens) {
            fprintf(stderr, "cannot include token table\n");

            return ZCC_FILE_ERROR;
        }
    }
    if (!strncmp((char *)str, "#include", 8)) {
        str += 8;
        while ((*str) && isspace(*str)) {
            str++;
        }
        if (*str == '"') {
            str++;
            while ((*str) && *str != '"') {
                *ptr++ = *str++;
            }
            if (*str == '"') {
                *ptr = '\0';
                ret = zccreadfile(name, curfile);
            } else {
                fprintf(stderr, "invalid #include directive %s\n",
                        name);
                
                exit(1);
            }
        } else if (*str == '<') {
            str++;
            ptr = str;
            while ((*str) && *str != '>') {
                str++;
            }
            if (*str == '>') {
                *str = '\0';
                strcat(name, strtok(path, ":"));
                strcat(name, ptr);
                while (ret == ZCC_FILE_ERROR) {
                    ret = zccreadfile(name, curfile);
                    name[0] = '\0';
                    cp = strtok(NULL, ":");
                    if (cp) {
                        strcat(name, cp);
                        strcat(name, ptr);
                    } else {

                        return ZCC_FILE_ERROR;
                    }
                }
            } else {
                fprintf(stderr, "invalid #include directive %s\n",
                        name);
                
                exit(1);
            }
        }
    }
    if (ret != ZCC_FILE_ERROR) {
        *retstr = str;
    }

    return ret;
}

static void
zccqueuetoken(struct zcctoken *token, int curfile)
{
    struct zcctokenq *queue = zccfiletokens[curfile];
    struct zcctoken  *head = (queue) ? queue->head : NULL;
    struct zcctoken  *tail = (queue) ? queue->tail : NULL;

    if (!head) {
        queue = zccfiletokens[curfile] = malloc(sizeof(struct zcctokenq));
        token->prev = NULL;
        queue->head = token;
        queue->tail = NULL;
    } else if (tail) {
        token->prev = tail;
        tail->next = token;
        queue->tail = token;
    } else {
        head->next = token;
        token->prev = head;
        queue->tail = token;
    }

    return;
}

static int
zccreadfile(char *name, int curfile)
{
    long              buflen = NLINEBUF;
    FILE             *fp = fopen(name, "r");
    long              eof = 0;
    struct zcctoken  *token;
    char             *str = linebuf;
    char             *lim = NULL;
    long              loop = 1;
    int               ch;
    long              comm = 0;
    long              done = 1;
    long              len = 0;
#if (ZCCDB)
    unsigned long     line = 0;
#endif

    if (!fp) {

        return ZCC_FILE_ERROR;
    }
    while (loop) {
        if (done) {
            if (eof) {
                loop = 0;
//                done = 0;

                break;
            }
            str = linebuf;
            done = 0;
            len = 0;
            ch = fgetc(fp);
            if (ch == EOF) {
                loop = 0;

                break;
            } else {
#if (ZCCDB)
                line++;
#endif
                while (ch != EOF && ch != '\n') {
                    *str++ = ch;
                    len++;
                    if (len == buflen) {
                        fprintf(stderr, "overlong line\n");
                        
                        exit(1);
                    }
                    ch = fgetc(fp);
                    eof = (ch == EOF);
                }
                *str = '\0';
                str = linebuf;
                lim = str + len;
                while ((*str) && isspace(*str)) {
                    str++;
                }
                if (str > lim) {
                    done = 1;
                }
//                fprintf(stderr, "BUF: %s\n", str);
            }
        } else {
            curfile = zccgetinclude(str, &str, curfile + 1);
            if (curfile != ZCC_FILE_ERROR) {
                done = 1;
            } else if (str[0] == '/' && str[1] == '/') {
                /* comment start */
                done = 1;
            } else if (str[0] == '/' && str[1] == '*') {
                /* comment */
                comm = 1;
                while (comm) {
                    ch = fgetc(fp);
                    if (ch == EOF) {
                        loop = 0;
                        
                        break;
#if (ZCCDB)
                    } else if (ch == '\n') {
                        line++;
#endif
                    } else if (ch == '*') {
                        ch = fgetc(fp);
                        if (ch == '/') {
                            
                            comm = 0;
                        } else if (ch == EOF) {
                            comm = 0;
                            loop = 0;
                            eof = 1;
                        }
                    }
                }
                done = 1;
            } else {
                if (*str) {
                    while (*str) {
                        token = zccgettoken(str, &str);
                        if (token) {
#if (ZCCDB)
                            token->fname = strdup(name);
                            token->line = line;
#endif
                            zccqueuetoken(token, curfile);
                        }
                        if (str >= lim) {
                            done = 1;
                        }
                    }
                } else {
                    done = 1;
                }
            }
        }
    }
    fclose(fp);
    
    return curfile;
}

int
main(int argc,
     char *argv[])
{
    int  arg;
    long l;
    
    arg = zccinit(argc, argv);
    if (!arg) {

        exit(1);
    }
    for (l = arg; l < argc ; l++) {
        zcccurfile = zccreadfile(argv[l], zcccurfile);
        if (zcccurfile == ZCC_FILE_ERROR) {

            exit(1);
        }
    }

    exit(0);
}
