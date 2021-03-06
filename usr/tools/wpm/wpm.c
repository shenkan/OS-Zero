#define PTHREAD 1
#include <wpm/conf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <zero/cdecl.h>
#include <zero/param.h>
#include <zero/trix.h>
#if (ZASPROF)
#include <zero/prof.h>
#endif
#if (PTHREAD)
#include <pthread.h>
#include <zero/mtx.h>
#endif
#include <zas/zas.h>
#include <wpm/wpm.h>
#include <wpm/mem.h>
#if (WPMVEC)
#include <vec/vec.h>
#include <vec/vm.h>
#endif

#if defined(__i386__) || defined(__i486__) || defined(__i586__)         \
    || defined(__i686__) || defined(__x86_64__) || defined(__amd64__)
#define USEASM 0
#include <wpm/ia32/asm.h>
#else
#define USEASM 0
#endif

#if (ZASBUF)
extern long             readbufcur;
#endif
#if (WPMDB)
extern struct zasline  *linehash[];
#endif
extern struct zastoken *zastokenqueue;
extern struct zastoken *zastokentail;
extern unsigned long    zasinputread;
extern zasmemadr_t      _startadr;
extern wpmpage_t       *mempagetab;

static void hookbzero(struct wpmopcode *op);
static void hookpzero(struct wpmopcode *op);
static void hookpalloc(struct wpmopcode *op);
static void hookpfree(struct wpmopcode *op);

static void    memstoreq(int64_t src, wpmmemadr_t virt);
static int64_t memfetchq(wpmmemadr_t virt);
static void    memstorel(int32_t src, wpmmemadr_t virt);
static int32_t memfetchl(wpmmemadr_t virt);

wpmophandler_t *wpmopfunctab[WPMNUNIT][WPMNASMOP] ALIGNED(PAGESIZE)
= {
    {
        NULL,
        opnot,
        opand,
        opor,
        opxor,
        opshr,
        opshra,
        opshl,
        opror,
        oprol,
        opinc,
        opdec,
        opadd,
        opsub,
        opcmp,
        opmul,
        opdiv,
        opmod,
        opbz,
        opbnz,
        opblt,
        opble,
        opbgt,
        opbge,
        opbo,
        opbno,
        opbc,
        opbnc,
        oppop,
        oppush,
        opmov,
        opmovb,
        opmovw,
        opjmp,
        opcall,
        openter,
        opleave,
        opret,
        oplmsw,
        opsmsw,
        opreset,
        opnop,
        ophlt,
        opbrk,
        optrap,
        opcli,
        opsti,
        opiret,
        opthr,
        opcmpswap,
        opinb,
        opoutb,
        opinw,
        opoutw,
        opinl,
        opoutl,
        ophook
    },
    {
#if (WPMVEC)
        NULL,
        vecopadd,
        vecopsub,
        vecopmul,
        vecopdiv,
        vecopmod,
        vecoplt,
        vecoplte,
        vecopgt,
        vecopgte,
        vecopeq,
        vecopineq,
        vecopshl,
        vecopshr,
        vecopnot,
        vecopand,
        vecopor,
        vecopxor,
        vecopselect,
        vecoprand,
        vecopfloor,
        vecopceil,
        vecoptrunc,
        vecopround,
        vecopitof,
        vecopitob,
        vecopbtoi,
        vecoplog,
        vecopsqrt,
        vecopexp,
        vecopsin,
        vecopcos,
        vecoptan,
        vecopasin,
        vecopacos,
        vecopatan,
        vecopsinh,
        vecopcosh,
        vecoptanh,
#if (WPMVECFULL)
        vecopplscan,
        vecopmulscan,
        vecopmaxscan,
        vecopminscan,
        vecopandscan,
        vecoporscan,
        vecopxorscan,
        vecopplreduce,
        vecopmulreduce,
        vecopmaxreduce,
        vecopminreduce,
        vecopandreduce,
        vecoporreduce,
        vecopxorreduce,
        vecoppermute,
        vecopdpermute,
        vecopfpermute,
        vecopbpermute,
        vecopbfpermute,
        vecopdfpermute,
        vecoppermute,
        vecopextract,
        vecopreplace,
        vecoppack,
        vecoprankup,
        vecoprankdown,
        vecopdist,
        vecopindex,
        vecoplength,
        vecopmkdes,
        vecoplengths,
        vecopcopy,
        vecoppop,
        vecopcpop,
        vecoppair,
        vecopunpair,
#endif /* WPMVECFULL */
#endif /* WPMVEC */
        NULL
    },
    {
        NULL
    },
    {
        NULL
    }
};

struct zasopinfo wpmopinfotab[WPMNUNIT][WPMNASMOP]
= {
    {
        { NULL, 0 },
        { "not", 1 },
        { "and", 2 },
        { "or", 2 },
        { "xor", 2 },
        { "shr", 2 },
        { "shra", 2 },
        { "shl", 2 },
        { "ror", 2 },
        { "rol", 2},
        { "inc", 1 },
        { "dec", 1 },
        { "add", 2},
        { "sub", 2 },
        { "cmp", 2 },
        { "mul", 2 },
        { "div", 2 },
        { "mod", 2 },
        { "bz", 1 },
        { "bnz", 1 },
        { "blt", 1 },
        { "ble", 1 },
        { "bgt", 1 },
        { "bge", 1 },
        { "bo", 1 },
        { "bno", 1 },
        { "bc", 1 },
        { "bnc", 1 },
        { "pop", 1 },
        { "push", 1 },
        { "mov", 2 },
        { "movb", 2 },
        { "movw", 2 },
        { "jmp", 1 },
        { "call", 1 },
        { "enter", 0 },
        { "leave", 0 },
        { "ret", 0 },
        { "lmsw", 1 },
        { "smsw", 1 },
        { "reset", 0 },
        { "nop", 0 },
        { "hlt", 0 },
        { "brk", 1 },
        { "trap", 1 },
        { "cli", 0 },
        { "sti", 0 },
        { "iret", 0 },
        { "thr", 1 },
        { "cmpswap", 2 },
        { "inb", 1 },
        { "outb", 2 },
        { "inw", 1 },
        { "outw", 2 },
        { "inl", 1 },
        { "outl", 2 },
        { "hook", 1 },
        { NULL, 0 }
    },
    {
        { NULL, 0 },
        { "vadd", 2 },
        { "vsub", 2 },
        { "vmul", 2 },
        { "vdiv", 2 },
        { "vmod", 2 },
        { "vlt", 2 },
        { "vlte", 2 },
        { "vgt", 2 },
        { "vgte", 2 },
        { "veq", 2 },
        { "vineq", 2 },
        { "vshl", 2 },
        { "vshr", 2 },
        { "vnot", 1 },
        { "vand", 2 },
        { "vor", 2 },
        { "vxor", 2 },
        { "vselect", 3 },
        { "vrand", 1 },
        { "vfloor", 1 },
        { "vceil", 1 },
        { "vtrunc", 1 },
        { "vround", 1 },
        { "vitof", 1 },
        { "vitob", 1 },
        { "btoi", 1 },
        { "vlog", 1 },
        { "vsqrt", 1 },
        { "vexp", 1 },
        { "vsin", 1 },
        { "vcos", 1 },
        { "vtan", 1 },
        { "vasin", 1 },
        { "vacos", 1 },
        { "vatan", 1 },
        { "vsinh", 1 },
        { "vcosh", 1 },
        { "vtanh", 1 },
#if (WPMVECFULL)
        /* TODO: fix argument counts below */
        { "vplscan", 2 },
        { "vmulscan", 2 },
        { "vmaxscan", 2 },
        { "vminscan", 2 },
        { "vadnscan", 2 },
        { "vorscan", 2 },
        { "vxorscan", 2 },
        { "vplreduce", 2 },
        { "vmulreduce", 2 },
        { "vmaxreduce", 2 },
        { "vminreduce", 2 },
        { "vandreduce", 2 },
        { "vorreduce", 2 },
        { "vxorreduce", 2 },
        { "vpermute", 2 },
        { "vdpermute", 2 },
        { "vfpermute", 2 },
        { "vbpermute", 2 },
        { "vbfpermute", 2 },
        { "vdfpermute", 2 },
        { "vpermute", 2 },
        { "vextract", 2 },
        { "vreplace", 2 },
        { "vpack", 2 },
        { "vrankup", 2 },
        { "vrankdown", 2 },
        { "vdist", 2 },
        { "vindex", 2 },
        { "vlength", 2 },
        { "vmkdes", 2 },
        { "vlengths", 2 },
        { "vcopy", 2 },
        { "vpop", 2 },
        { "vcpop", 2 },
        { "vpair", 2 },
        { "vunpair", 2 },
#endif /* WPMVECFULL */
        { NULL, 0 }
    },
    {
        {
            NULL, 0
        }
    },
    {
        {
            NULL, 0
        }
    }
};
    
#if (WPM)
wpmhookfunc_t *hookfunctab[256]
= {
    hookbzero,
    hookpzero,
    hookpalloc,
    hookpfree
};
#endif

char *argnametab[]
= {
    "ARGNONE",
    "ARGIMMED",
    "ARGADR",
    "ARGREG",
    "ARGSYM"
};

#if (PTHREAD)
__thread struct wpm  *wpm;
#else
struct wpm           *wpm;
#endif
static volatile long  atomlk;
static volatile long  intlk;
static volatile long  thrlk;
static volatile long  thrcnt;
static volatile long  thrid;
uint8_t              *physmem;

static uint32_t
wpmgetthrid(void)
{
    uint32_t id;

    id = ++thrid;

    return id;
}

struct wpm *
wpminit(void)
{
    mtxlk(&thrlk);
    if (!physmem) {
        physmem = calloc(1, MEMSIZE);
    }
    if (!wpm) {
        wpm = calloc(1, sizeof(struct wpm));
        if (wpm) {
            wpm->cpustat.sp = MEMSIZE - thrid * THRSTKSIZE;
            wpm->cpustat.fp = MEMSIZE - thrid * THRSTKSIZE;
            wpm->thrid = wpmgetthrid();
        }
    }
    mtxunlk(&thrlk);

    return wpm;
}

#if (PTHREAD)
void
wpminitthr(wpmmemadr_t pc)
{
    uint32_t            id;
    pthread_t           tid;
    struct wpmcpustate *cpustat = malloc(sizeof(struct wpmcpustate));
//    wpmmemadr_t         sp = mempalloc(THRSTKSIZE);

    mtxlk(&thrlk);
    id = wpmgetthrid();
    thrcnt++;
    mtxunlk(&thrlk);
    memcpy(cpustat, &wpm->cpustat, sizeof(struct wpmcpustate));
    cpustat->pc = pc;
//    cpustat->sp = sp;
    cpustat->sp = MEMSIZE - id * THRSTKSIZE;
    pthread_create(&tid, NULL, wpmloop, (void *)cpustat);
    pthread_detach(tid);

    return;
}
#endif /* PTHREAD */

#if (WPM)

void
wpmprintop(struct wpmopcode *op)
{
    fprintf(stderr, "INST: %s, size %d, arg1t == %s, arg2t == %s, reg1 == %x, reg2 == %x, args[0] == %x", wpmopinfotab[op->unit][op->inst].name, op->size << 2, argnametab[op->arg1t], argnametab[op->arg2t], op->reg1, op->reg2, op->args[0]);
    if (op->arg2t) {
        fprintf(stderr, ", args[1] == %x", op->args[1]);
    }
    fprintf(stderr, "\n");
}

#endif

void *
wpmloop(void *cpustat)
{
    wpmophandler_t   *func;
    struct wpmopcode *op;
#if (WPMTRACE)
    int               i;
#endif
#if (WPMDB)
    struct zasline *line;
#endif

    wpminit();
#if (WPMTRACE)
    fprintf(stderr, "memory\n");
    fprintf(stderr, "------\n");
    for (i = WPMTEXTBASE ; i < WPMTEXTBASE + 256 ; i++) {
        fprintf(stderr, "%02x ", physmem[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "registers\n");
    fprintf(stderr, "---------\n");
    fprintf(stderr, "---------\n");
    for (i = 0 ; i < NREG ; i++) {
        fprintf(stderr, "r%d:\t%x\n", i, wpm->cpustat.regs[i]);
    }
#endif
    wpm->shutdown = 0;
    memcpy(&wpm->cpustat, cpustat, sizeof(struct wpmcpustate));
    free(cpustat);
    while (!wpm->shutdown) {
        op = (struct wpmopcode *)&physmem[wpm->cpustat.pc];
        if (op->inst == OPNOP) {
            wpm->cpustat.pc++;
        } else {
            wpm->cpustat.pc = rounduppow2(wpm->cpustat.pc, sizeof(wpmword_t));
            op = (struct wpmopcode *)&physmem[wpm->cpustat.pc];
            func = wpmopfunctab[op->unit][op->inst];
            if (func) {
#if (WPMDB)
                line = zasfindline(wpm->cpustat.pc);
                if (line) {
                    fprintf(stderr, "%s:%ld:\t%s\n", line->file, line->num, line->data);
                }
#endif
                func(op);
            } else {
                fprintf(stderr, "illegal instruction, PC == %lx\n",
                        (long)wpm->cpustat.pc);
#if (WPM)
                wpmprintop(op);
#endif
                
                exit(1);
            }
        }
    }
#if (WPMTRACE)
    fprintf(stderr, "memory\n");
    fprintf(stderr, "------\n");
    for (i = WPMTEXTBASE ; i < WPMTEXTBASE + 256 ; i++) {
        fprintf(stderr, "%02x ", physmem[i]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "registers\n");
    fprintf(stderr, "---------\n");
    for (i = 0 ; i < NREG ; i++) {
        fprintf(stderr, "r%d:\t%x\n", i, wpm->cpustat.regs[i]);
    }
#endif
    mtxlk(&thrlk);
    thrcnt--;
    if (!thrcnt) {
        exit(0);
    }
    mtxunlk(&thrlk);

    return NULL;
}

#if (WPM)

void
opnot(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    dest = (argt == ARGREG
                         ? wpm->cpustat.regs[op->reg1]
                         : op->args[0]);
#if (WPMDEBUG)
    fprintf(stderr, "NOT: %x - ", dest);
#endif
#if (USEASM)
    asmnot(dest);
#else
    dest = ~dest;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWZF;
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt == ARGREG) {
        wpm->cpustat.regs[op->reg1] = dest;
    } else {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opand(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));

#if (WPMDEBUG)
    fprintf(stderr, "AND: %x, %x - ", src, dest);
#endif
#if (USEASM)
    asmand(src, dest);
#else
    dest &= src;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWZF;
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opor(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));

#if (USEASM)
    asmor(src, dest);
#else
    dest |= src;
#endif
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opxor(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));

#if (WPMDEBUG)
    fprintf(stderr, "XOR: %x, %x - ", src, dest);
#endif
#if (USEASM)
    asmxor(src, dest);
#else
    dest ^= src;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWZF;
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opshr(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    cnt = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchl(op->args[0])));
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
#if (!USEASM)
    wpmuword_t sign = ~0L >> cnt;
#endif
    
#if (WPMDEBUG)
    fprintf(stderr, "SHR: %x by %d\n", dest, cnt);
#endif
#if (USEASM)
    asmshr(cnt, dest);
#else
    dest >>= cnt;
    dest &= sign;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWZF;
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opshra(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    cnt = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchl(op->args[0])));
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
#if (!USEASM)
    wpmword_t  sign = (wpmuword_t)dest >> (CHAR_BIT * sizeof(wpmword_t) - 1);
#endif

#if (WPMDEBUG)
    fprintf(stderr, "SHRA: %x by %d\n", dest, cnt);
#endif

#if (USEASM)
    asmshra(cnt, dest);
#else
    sign = -sign << (CHAR_BIT * sizeof(wpmword_t) - cnt);
    dest >>= cnt;
    dest |= sign;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWZF;
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opshl(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    cnt = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchl(op->args[0])));
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
    wpmword_t    of = dest & (~0L << (CHAR_BIT * sizeof(wpmword_t) - 1));
    wpmword_t    cf = dest & (1L << (CHAR_BIT * sizeof(wpmword_t) - cnt));
    
#if (WPMDEBUG)
    fprintf(stderr, "SHL: %x by %d\n", dest, cnt);
#endif
#if (USEASM)
    asmshl(cnt, dest);
#else
    dest <<= cnt;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~(MSWOF | MSWCF);
    if (of) {
        wpm->cpustat.msw |= MSWOF;
    } else if (cf) {
        wpm->cpustat.msw |= MSWCF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opror(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    cnt = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchl(op->args[0])));
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
#if (!USEASM)
    wpmuword_t   mask = (~0L) >> (CHAR_BIT * sizeof(wpmuword_t) - cnt);
    wpmuword_t   bits = dest & mask;
#endif
    wpmword_t    cf = dest & (1L << (cnt - 1));

#if (WPMDEBUG)
    fprintf(stderr, "ROR: 0x%08x, %d - ", dest, cnt);
#endif
#if (USEASM)
    asmror(cnt, dest);
#else
    bits <<= CHAR_BIT * sizeof(wpmuword_t) - cnt;
    dest >>= cnt;
    dest |= bits;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWCF;
    if (cf) {
        wpm->cpustat.msw |= MSWCF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
oprol(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    cnt = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchl(op->args[0])));
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
#if (!USEASM)
    wpmuword_t   mask = (~0L) << (CHAR_BIT * sizeof(wpmuword_t) - cnt);
    wpmuword_t   bits = dest & mask;
#endif
    wpmword_t  cf = dest & (1L << (CHAR_BIT * sizeof(wpmword_t) - cnt));

#if (WPMDEBUG)
    fprintf(stderr, "ROL: 0x%08x, %d - ", dest, cnt);
#endif
#if (USEASM)
    asmrol(cnt, dest);
#else
    bits >>= CHAR_BIT * sizeof(wpmuword_t) - cnt;
    dest <<= cnt;
    dest |= bits;
#endif
#if (WPMDEBUG)
    fprintf(stderr, "%x\n", dest);
#endif
    wpm->cpustat.msw &= ~MSWCF;
    if (cf) {
        wpm->cpustat.msw |= MSWCF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    } else {
        memstorel(dest, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opinc(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    dest = (argt == ARGREG
                         ? wpm->cpustat.regs[op->reg1]
                         : op->args[0]);
    wpmword_t    of = (dest == ~0L);
    
    if (!~dest) {
        wpm->cpustat.msw |= MSWOF;
    }
#if (USEASM)
    asminc(dest);
#else
    dest++;
#endif
    wpm->cpustat.msw &= ~MSWOF;
    if (of) {
        wpm->cpustat.msw |= MSWOF;
    }
    if (argt == ARGREG) {
        wpm->cpustat.regs[op->reg1] = dest;
    } else {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opdec(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    dest = (argt == ARGREG
                         ? wpm->cpustat.regs[op->reg1]
                         : op->args[0]);
    wpmword_t    of = !dest;

#if (USEASM)
    asmdec(dest);
#else
    dest--;
#endif
    wpm->cpustat.msw &= ~(MSWZF | MSWOF);
    if (of) {
        wpm->cpustat.msw |= MSWOF;
    } else if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt == ARGREG) {
        wpm->cpustat.regs[op->reg1] = dest;
    } else {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opadd(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
    wpmword_t    tmp = dest;
    wpmword_t    of;

#if (USEASM)
    asmadd(src, dest);
#else
    dest += src;
#endif
    wpm->cpustat.msw &= ~(MSWZF | MSWSF | MSWOF);
    of = ((((tmp ^ src)
            ^ (1L << (CHAR_BIT * sizeof(wpmword_t) - 1)))
           & (dest ^ src))
          & (1L << (CHAR_BIT * sizeof(wpmword_t) - 1)));
    if (of) {
        wpm->cpustat.msw |= MSWOF;
    } else if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    } else if (dest & (1L << (CHAR_BIT * sizeof(wpmword_t) - 1))) {
        wpm->cpustat.msw |= MSWSF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opsub(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));

#if (USEASM)
    asmsub(src, dest);
#else
    dest -= src;
#endif
    wpm->cpustat.msw &= ~(MSWZF | MSWSF);
    if (dest & (1L << (CHAR_BIT * sizeof(wpmword_t) - 1))) {
        wpm->cpustat.msw |= MSWSF;
    } else if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opcmp(struct wpmopcode *op)
{
    wpmword_t    msw = wpm->cpustat.msw;
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));

#if (WPMDEBUG)
    fprintf(stderr, "CMP: %x, %x\n", src, dest);
#endif
    msw &= ~(MSWZF | MSWSF);
    if (src < dest) {
        msw |= MSWSF;
    } else if (src == dest) {
        msw |= MSWZF;
    }
    wpm->cpustat.msw = msw;
    wpm->cpustat.pc += op->size << 2;

    return;
}

/*
 * NOTE: better logic will need to be implemented to detect overflow in beyond
 * 32-bit.
 */
void
opmul(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
#if (WPMWORDSIZE == 32)
    int64_t      res64;
    int64_t      tmp;
#endif

#if (USEASM)
    asmmul(src, dest);
#else
    dest *= src;
#endif
#if (WPMWORDSIZE == 32)
    wpm->cpustat.msw &= ~(MSWOF | MSWSF | MSWZF);
    res64 = (int64_t)dest * (int64_t)src;
    tmp = abs(res64);
    if (tmp > UINT32_MAX || (res64 < 0 && tmp == UINT32_MAX)) {
        wpm->cpustat.msw |= MSWOF;
    }
    if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    } else if (dest < 0) {
        wpm->cpustat.msw |= MSWSF;
    }
#else
#error WPMWORDSIZE != 32
#endif
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opdiv(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
    long         sp = wpm->cpustat.sp;
    long         pc;

    if (!src) {
        sp -= 4;
        sp = wpm->cpustat.sp;
        wpm->cpustat.sp = sp;
        pc = memfetchl(TRAPDIV << 2);
        if (pc) {
            wpm->cpustat.pc = pc;
        } else {
            fprintf(stderr, "division by zero\n");

            exit(1);
        }
    } else {
#if (USEASM)
        asmdiv(src, dest);
#else
        dest /= src;
#endif
    }
    wpm->cpustat.msw &= ~(MSWSF | MSWZF);
    if (dest < 0) {
        wpm->cpustat.msw |= MSWSF;
    } else if (!dest) {
        wpm->cpustat.msw |= MSWZF;
    }
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}


void
opmod(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    src = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    wpmword_t    dest = (argt2 == ARGREG
                         ? wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? op->args[0]
                            : op->args[1]));
    
#if (USEASM)
    asmmod(src, dest);
#else
    dest %= src;
#endif
    if (argt2 == ARGREG) {
        wpm->cpustat.regs[op->reg2] = dest;
    } else if (argt1 == ARGREG) {
        memstorel(dest, op->args[0]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opbz(struct wpmopcode *op)
{
    if (wpm->cpustat.msw & MSWZF) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbnz(struct wpmopcode *op)
{
    if (!(wpm->cpustat.msw & MSWZF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opblt(struct wpmopcode *op)
{
    if (wpm->cpustat.msw & MSWSF) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opble(struct wpmopcode *op)
{
    if ((wpm->cpustat.msw & MSWSF) || (wpm->cpustat.msw & MSWZF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbgt(struct wpmopcode *op)
{
    if (!(wpm->cpustat.msw & MSWSF) && !(wpm->cpustat.msw & MSWZF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbge(struct wpmopcode *op)
{
    if (!(wpm->cpustat.msw & MSWSF) || (wpm->cpustat.msw & MSWZF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbo(struct wpmopcode *op)
{
    if (wpm->cpustat.msw & MSWOF) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbno(struct wpmopcode *op)
{
    if (!(wpm->cpustat.msw & MSWOF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbc(struct wpmopcode *op)
{
    if (wpm->cpustat.msw & MSWCF) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opbnc(struct wpmopcode *op)
{
    if (!(wpm->cpustat.msw & MSWCF)) {
        uint_fast8_t argt = op->arg1t;
        wpmword_t    dest = (argt == ARGREG
                             ? wpm->cpustat.regs[op->reg1]
                             : op->args[0]);
        wpm->cpustat.pc = dest;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
oppop(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    val;

    if (argt == ARGREG) {
        val = memfetchl(wpm->cpustat.sp);
        wpm->cpustat.sp += 4;
        wpm->cpustat.regs[op->reg1] = val;
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
oppush(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    src = (argt == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    
    wpm->cpustat.sp -= 4;
    memstorel(src, wpm->cpustat.sp);
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opmov(struct wpmopcode *op)
{
#if (WPMVEC)
    struct vecopcode *vop = (struct vecopcode *)op;
#endif
    uint_fast8_t      argt1 = op->arg1t;
    uint_fast8_t      argt2 = op->arg2t;
#if (WPMVEC)
    wpmword_t         reg1 = (op->unit == UNIT_VEC
                              ? vop->reg1
                              : op->reg1);
    wpmword_t         reg2 = (op->unit == UNIT_VEC
                              ? vop->reg2
                              : op->reg2);
#else
    wpmword_t         reg1 = op->reg1;
    wpmword_t         reg2 = op->reg2;
#endif
    wpmword_t         src = (argt1 == ARGREG
                             ? ((reg1 & REGINDEX)
                                ? memfetchl(wpm->cpustat.regs[reg1 & 0x0f]
                                            + op->args[0])
                                : (reg1 & REGINDIR
                                   ? memfetchl(wpm->cpustat.regs[reg1 & 0x0f])
                                   : wpm->cpustat.regs[reg1 & 0x0f]))
                             : (argt1 == ARGIMMED
                                ? op->args[0]
                                : memfetchl(op->args[0])));
    wpmword_t         dest;
    
#if (WPMVEC)
    if (argt2 == ARGVAREG) {
        wpm->cpustat.varegs[reg2 & 0x07] = src;
    } else if (argt2 == ARGVLREG) {
        wpm->cpustat.vlregs[reg2 & 0x07] = src;
    } else
#endif
        if (argt2 == ARGREG) {
            if (reg2 & REGINDIR) {
                dest = wpm->cpustat.regs[reg2 & 0x0f];
                memstorel(src, dest);
            } else if (reg2 & REGINDEX) {
                if (argt1 == ARGREG) {
                    dest = wpm->cpustat.regs[reg2 &0x0f] + op->args[0];
                } else {
                    dest = wpm->cpustat.regs[reg2 &0x0f] + op->args[1];
                }
                memstorel(src, dest);
            } else {
                wpm->cpustat.regs[reg2 & 0x0f] = src;
            }
        } else if (argt1 == ARGREG) {
            memstorel(src, op->args[0]);
        } else {
            memstorel(src, op->args[1]);
        }
#if (WPMVEC)
    if (op->unit == UNIT_VEC) {
        wpm->cpustat.pc += 4 + (vop->narg << 2);
    } else {
        wpm->cpustat.pc += op->size << 2;
    }
#else
    wpm->cpustat.pc += op->size << 2;
#endif

    return;
}

void
opmovb(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    reg1  = op->reg1;
    wpmword_t    reg2  = op->reg2;
    int8_t       src = (argt1 == ARGREG
                        ? ((reg1 & REGINDEX)
                           ? memfetchb(wpm->cpustat.regs[reg1 & 0x0f]
                                       + op->args[0])
                           : (reg1 & REGINDIR
                              ? memfetchb(wpm->cpustat.regs[reg1 & 0x0f])
                              : wpm->cpustat.regs[reg1 & 0x0f]))
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                           : memfetchb(op->args[0])));
    wpmword_t    dest;

#if (WPMDEBUG)
    fprintf(stderr, "MOVB: %x - \n", src);
#endif    
    if (argt2 == ARGREG) {
        if (reg2 & REGINDIR) {
            dest = wpm->cpustat.regs[reg2 & 0x0f];
            memstoreb(src, dest);
        } else if (reg2 & REGINDEX) {
            if (argt1 == ARGREG) {
                dest = wpm->cpustat.regs[reg2 &0x0f] + op->args[0];
            } else {
                dest = wpm->cpustat.regs[reg2 &0x0f] + op->args[1];
            }
            memstoreb(src, dest);
        } else {
            wpm->cpustat.regs[reg2 & 0x0f] = src;
        }
    } else if (argt1 == ARGREG) {
        memstoreb(src, op->args[0]);
    } else {
        memstoreb(src, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opmovw(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    reg1  = op->reg1;
    wpmword_t    reg2  = op->reg2;
    int16_t      src = (argt1 == ARGREG
                        ? ((reg1 & REGINDEX)
                           ? memfetchw(wpm->cpustat.regs[reg1 & 0x0f]
                                       + op->args[0])
                           : (reg1 & REGINDIR
                              ? memfetchw(wpm->cpustat.regs[reg1 & 0x0f])
                          : wpm->cpustat.regs[reg1 & 0x0f]))
                        : (argt1 == ARGIMMED
                           ? op->args[0]
                       : memfetchw(op->args[0])));
    wpmword_t    dest;
    
    if (argt2 == ARGREG) {
        if (reg2 & REGINDIR) {
            dest = wpm->cpustat.regs[reg2 & 0x0f];
            memstorew(src, dest);
        } else if (reg2 & REGINDEX) {
            if (argt1 == ARGREG) {
                dest = wpm->cpustat.regs[reg2 & 0x0f] + op->args[0];
            } else {
                dest = wpm->cpustat.regs[reg2 & 0x0f] + op->args[1];
            }
            memstorew(src, dest);
        } else {
            wpm->cpustat.regs[reg2 & 0x0f] = src;
        }
    } else if (argt1 == ARGREG) {
        memstorew(src, op->args[0]);
    } else {
        memstorew(src, op->args[1]);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opjmp(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    dest = (argt == ARGREG
                         ? wpm->cpustat.regs[op->reg1]
                         : op->args[0]);

    wpm->cpustat.pc = dest;

    return;
}

/*
 * call stack
 * ----------
 * r7
 * r6
 * r5
 * r4
 * r3
 * r2
 * r1
 * retadr
 * oldfp
 */
void
opcall(struct wpmopcode *op)
{
    uint_fast8_t  argt = op->arg1t;
    wpmword_t     dest = (argt == ARGREG
                          ? wpm->cpustat.regs[op->reg1]
                          : op->args[0]);
    wpmword_t    *stk = (wpmword_t *)(&physmem[wpm->cpustat.sp]);
    wpmword_t     fp = wpm->cpustat.sp - 36;

    stk[-1] = wpm->cpustat.regs[7];
    stk[-2] = wpm->cpustat.regs[6];
    stk[-3] = wpm->cpustat.regs[5];
    stk[-4] = wpm->cpustat.regs[4];
    stk[-5] = wpm->cpustat.regs[3];
    stk[-6] = wpm->cpustat.regs[2];
    stk[-7] = wpm->cpustat.regs[1];
    stk[-8] = wpm->cpustat.pc + (op->size << 2);
    stk[-9] = wpm->cpustat.fp;
    wpm->cpustat.fp = fp;
    wpm->cpustat.sp = fp;
    wpm->cpustat.pc = dest;

    return;
}

void
openter(struct wpmopcode *op)
{
    long       argt = op->arg1t;
    wpmword_t  ofs = (argt == ARGREG
                      ? wpm->cpustat.regs[op->reg1]
                      : op->args[0]);
    wpmword_t *stk = (wpmword_t *)wpm->cpustat.sp;

    *--stk = wpm->cpustat.fp;
    wpm->cpustat.sp -= ofs + 4;

    return;
}

void
opleave(struct wpmopcode *op)
{
    wpmword_t fp = memfetchl(wpm->cpustat.fp);
    
    wpm->cpustat.fp = memfetchl(fp);
    wpm->cpustat.sp = fp - 4;

    return;
}

/*
 * ret stack
 * ---------
 */
void
opret(struct wpmopcode *op)
{
    wpmword_t   fp = memfetchl(wpm->cpustat.fp);
    wpmword_t   dest = memfetchl(wpm->cpustat.fp + 4);
    wpmword_t  *stk = (wpmword_t *)(&physmem[wpm->cpustat.fp + 8]);

    wpm->cpustat.regs[1] = stk[0];
    wpm->cpustat.regs[2] = stk[1];
    wpm->cpustat.regs[3] = stk[2];
    wpm->cpustat.regs[4] = stk[3];
    wpm->cpustat.regs[5] = stk[4];
    wpm->cpustat.regs[6] = stk[5];
    wpm->cpustat.regs[7] = stk[6];
    wpm->cpustat.fp = fp;
    wpm->cpustat.sp = fp;
    wpm->cpustat.pc = dest;

    return;
}

void
oplmsw(struct wpmopcode *op)
{
    uint_fast8_t argt = op->arg1t;
    wpmword_t    msw = (argt == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);

    wpm->cpustat.msw = msw;
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opsmsw(struct wpmopcode *op)
{
    wpmword_t msw = wpm->cpustat.msw;
    wpmword_t dest = op->args[0];
    
    memstorel(msw, dest);
    
    return;
}

void
opreset(struct wpmopcode *op)
{
    exit(1);

    return;
}

void
ophlt(struct wpmopcode *op)
{
    wpm->shutdown = 1;

    return;
}

void
opnop(struct wpmopcode *op)
{
    wpm->cpustat.pc += op->size << 2;
}

void
opbrk(struct wpmopcode *op)
{
    wpmuword_t pc = memfetchl(TRAPBRK << 2);

    if (pc) {
        wpm->cpustat.pc = pc;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
optrap(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    wpmword_t    trap = (argt1 == ARGREG
                         ? wpm->cpustat.regs[op->reg1]
                         : memfetchl(op->args[0]));
    wpmword_t    pc = memfetchl(trap << 2);

    if (pc) {
        wpmword_t *stk = (wpmword_t *)(&physmem[wpm->cpustat.sp]);
        wpmword_t  fp = wpm->cpustat.sp - 8;
        stk[-1] = wpm->cpustat.pc + (op->size << 2);
        stk[-2] = wpm->cpustat.fp;
        wpm->cpustat.fp = fp;
        wpm->cpustat.sp = fp;
        wpm->cpustat.pc = pc;
    } else {
        wpm->cpustat.pc += op->size << 2;
    }

    return;
}

void
opcli(struct wpmopcode *op)
{
    mtxlk(&intlk);
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opsti(struct wpmopcode *op)
{
    mtxunlk(&intlk);
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
opiret(struct wpmopcode *op)
{
    wpmword_t fp = memfetchl(wpm->cpustat.fp);
    wpmword_t dest = memfetchl(wpm->cpustat.fp + 4);

    wpm->cpustat.fp = fp;
    wpm->cpustat.sp = fp;
    wpm->cpustat.pc = dest;

    return;
}

void
opthr(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    wpmuword_t   pc  = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : op->args[0]);
    
#if (WPMTRACE)
    fprintf(stderr, "thread == 0x%08x\n", pc);
#endif
    wpminitthr(pc);
    wpm->cpustat.pc += op->size << 2;

    return;
}

#if (PTHREAD)
/*
 * - opcmpswap can be used to create mutexes
 */
void
opcmpswap(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    wpmword_t    adr = (argt1 == ARGREG
                        ? wpm->cpustat.regs[op->reg1]
                        : memfetchl(op->args[0]));
    wpmword_t    val = (argt2 == ARGREG
                        ? wpm->cpustat.regs[op->reg2]
                        : (argt1 == ARGREG
                           ? memfetchl(op->args[0])
                           : memfetchl(op->args[1])));

    mtxlk(&atomlk);
    while (memfetchl(adr)) {
        pthread_yield();
    }
    memstorel(val, adr);
    mtxunlk(&atomlk);

    return;
}
#endif /* PTHREAD */

void
opinb(struct wpmopcode *op)
{
}

void
opoutb(struct wpmopcode *op)
{
    uint_fast8_t argt1 = op->arg1t;
    uint_fast8_t argt2 = op->arg2t;
    uint8_t      data = (argt1 == ARGREG
                         ? (uint8_t)wpm->cpustat.regs[op->reg1]
                         : (uint8_t)op->args[0]);
    uint8_t      port = (argt2 == ARGREG
                         ? (uint8_t)wpm->cpustat.regs[op->reg2]
                         : (argt1 == ARGREG
                            ? (uint8_t)op->args[0]
                            : (uint8_t)op->args[1]));

#if (WPMDEBUG)
    fprintf(stderr, "OUTB: %x -> %x\n", data, port);
#endif
    switch (port) {
        case CONOUTPORT:
            fprintf(stdout, "%c", data);
            fflush(stdout);

            break;
        case ERROUTPORT:
            fprintf(stderr, "%c", data);
            fflush(stderr);

            break;
    }

    wpm->cpustat.pc += op->size << 2;
}

void
opinw(struct wpmopcode *op)
{
}

void
opoutw(struct wpmopcode *op)
{
}

void
opinl(struct wpmopcode *op)
{
}

void
opoutl(struct wpmopcode *op)
{
}

void
ophook(struct wpmopcode *op)
{
    uint_fast8_t   argt = op->arg1t;
    wpmword_t      id = (argt == ARGREG
                         ? op->reg1
                         : op->args[0]);
    wpmhookfunc_t *hook = hookfunctab[id];

    if (hook) {
        hook(op);
    }
    wpm->cpustat.pc += op->size << 2;

    return;
}

void
wpmbzero(wpmmemadr_t adr, wpmuword_t size)
{
    wpmuword_t  nb = size;
    uint8_t    *ptr = NULL;

    while (nb--) {
        if (adr < MEMSIZE) {
            ptr = &physmem[adr];
        } else {
            ptr = (void *)mempagetab[pagenum(adr)];
        }
        if (ptr) {
//            bzero(ptr, 4096);
            *ptr = 0;
        } else {
            fprintf(stderr, "BZERO: page not mapped\n");

            exit(1);
        }
        adr++;
    }
}

void
wpmpzero(wpmmemadr_t adr, wpmuword_t size)
{
    wpmuword_t  npg = rounduppow2(size, 1U << PAGEBKT) >> PAGEBKT;
    void       *ptr = NULL;

    while (npg--) {
        if (adr < MEMSIZE) {
            ptr = &physmem[adr];
        } else {
            ptr = (void *)mempagetab[pagenum(adr)];
        }
        if (ptr) {
            bzero(ptr, 4096);
        } else {
            fprintf(stderr, "PZERO: page not mapped\n");

            exit(1);
        }
        adr += 4096;
    }
}

static void
hookbzero(struct wpmopcode *op)
{
    wpmmemadr_t adr = wpm->cpustat.regs[0];
    wpmuword_t  sz = wpm->cpustat.regs[1];

    wpmbzero(adr, sz);

    return;
}

static void
hookpzero(struct wpmopcode *op)
{
    wpmmemadr_t adr = wpm->cpustat.regs[0];
    wpmuword_t  sz = wpm->cpustat.regs[1];

    wpmpzero(adr, sz);

    return;
}

static void
hookpalloc(struct wpmopcode *op)
{
    wpmuword_t size = wpm->cpustat.regs[0];

    wpm->cpustat.regs[0] = mempalloc(size);

    return;
}

static void
hookpfree(struct wpmopcode *op)
{
    wpmmemadr_t adr = wpm->cpustat.regs[0];

    mempfree(adr);

    return;
}

#endif /* WPM */

#if (WPM)
int
wpmmain(int argc, char *argv[])
{
    long        l;
    zasmemadr_t adr = WPMTEXTBASE;
#if (ZASPROF)
    PROFDECLCLK(clk);
#endif

    if (argc < 2) {
        fprintf(stderr, "usage: zas <file1> ...\n");

        exit(1);
    }
    zasinit(&wpmopinfotab[0][0], &wpmopinfotab[1][0]);
    wpminitmem(MEMSIZE);
    wpminit();
    memset(physmem, 0, WPMTEXTBASE);
#if (ZASPROF)
    profstartclk(clk);
#endif
    for (l = 1 ; l < argc ; l++) {
#if (ZASBUF)
        zasreadfile(argv[l], adr, readbufcur);
#else
        zasreadfile(argv[l], adr);
#endif
        if (!zastokenqueue) {
            fprintf(stderr, "WARNING: no input in %s\n", argv[l]);
        } else {
            zasinputread = 1;
            adr = zastranslate(adr);
            zasresolve(WPMTEXTBASE);
            zasremovesyms();
#if (ZASPROF)
            profstopclk(clk);
            fprintf(stderr, "%ld microseconds to process %s\n",
                    profclkdiff(clk), argv[l]);
#endif        
        }
    }
    if (!zasinputread) {
        fprintf(stderr, "empty input\n");

        exit(1);
    }
    fprintf(stderr, "START: %lx\n", (long)_startadr);
    wpminitthr(_startadr);
    pause();

    /* NOTREACHED */
    exit(0);
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: wpm <asmfile>\n");

        exit(1);
    }
    exit(wpmmain(argc, argv));
}

#endif

