#ifndef __PCALC_OP_H__
#define __PCALC_OP_H__

typedef void zpccfunc_t(void *, void *, void *);

/* cop.c */
void not32(void *arg1, void *dummy, void *ret);
void not64(void *arg1, void *dummy, void *ret);
void and32(void *arg1, void *arg2, void *ret);
void and64(void *arg1, void *arg2, void *ret);
void or32(void *arg1, void *arg2, void *ret);
void or64(void *arg1, void *arg2, void *ret);
void xor32(void *arg1, void *arg2, void *ret);
void xor64(void *arg1, void *arg2, void *ret);
void shl32(void *arg1, void *arg2, void *ret);
void shl64(void *arg1, void *arg2, void *ret);
void shr32(void *arg1, void *arg2, void *ret);
void shr64(void *arg1, void *arg2, void *ret);
void shrl32(void *arg1, void *arg2, void *ret);
void shrl64(void *arg1, void *arg2, void *ret);
void ror32(void *arg1, void *arg2, void *ret);
void ror64(void *arg1, void *arg2, void *ret);
void rol32(void *arg1, void *arg2, void *ret);
void rol64(void *arg1, void *arg2, void *ret);
void inc32(void *arg1, void *arg2, void *ret);
void inc64(void *arg1, void *arg2, void *ret);
void dec32(void *arg1, void *arg2, void *ret);
void dec64(void *arg1, void *arg2, void *ret);
void add32(void *arg1, void *arg2, void *ret);
void add64(void *arg1, void *arg2, void *ret);
void sub32(void *arg1, void *arg2, void *ret);
void sub64(void *arg1, void *arg2, void *ret);
void mul32(void *arg1, void *arg2, void *ret);
void mul64(void *arg1, void *arg2, void *ret);
void div32(void *arg1, void *arg2, void *ret);
void div64(void *arg1, void *arg2, void *ret);
void mod32(void *arg1, void *arg2, void *ret);
void mod64(void *arg1, void *arg2, void *ret);

typedef void zpczerofunc_t(void *, void *, void *, void *);

/* zero.c */
void abs32(void *arg1, void *dummy1, void *dummy2, void *ret);
void abs64(void *arg1, void *dummy1, void *dummy2, void *ret);
void avg32(void *arg1, void *arg2, void *dummy, void *ret);
void avg64(void *arg1, void *arg2, void *dummy, void *ret);
void min32(void *arg1, void *arg2, void *dummy, void *ret);
void min64(void *arg1, void *arg2, void *dummy, void *ret);
void max32(void *arg1, void *arg2, void *dummy, void *ret);
void max64(void *arg1, void *arg2, void *dummy, void *ret);
void bitset32(void *arg1, void *arg2, void *dummy, void *ret);
void bitset64(void *arg1, void *arg2, void *dummy, void *ret);
void setbit32(void *arg1, void *arg2, void *dummy, void *ret);
void setbit64(void *arg1, void *arg2, void *dummy, void *ret);
void clrbit32(void *arg1, void *arg2, void *dummy, void *ret);
void clrbit64(void *arg1, void *arg2, void *dummy, void *ret);
void setbits32(void *arg1, void *arg2, void *arg3, void *ret);
void setbits64(void *arg1, void *arg2, void *arg3, void *ret);
void clrbits32(void *arg1, void *arg2, void *arg3, void *ret);
void clrbits64(void *arg1, void *arg2, void *arg3, void *ret);
void mergebits32(void *arg1, void *arg2, void *arg3, void *ret);
void mergebits64(void *arg1, void *arg2, void *arg3, void *ret);
void copybits32(void *arg1, void *arg2, void *arg3, void *ret);
void copybits64(void *arg1, void *arg2, void *arg3, void *ret);
void is2pow32(void *arg1, void *dummy1, void *dummy2, void *ret);
void is2pow64(void *arg1, void *dummy1, void *dummy2, void *ret);
void rnd2up32(void *arg1, void *arg2, void *dummy, void *ret);
void rnd2up64(void *arg1, void *arg2, void *dummy, void *ret);
void rnd2down32(void *arg1, void *arg2, void *dummy, void *ret);
void rnd2down64(void *arg1, void *arg2, void *dummy, void *ret);
void trailz32(void *arg1, void *dummy1, void *dummy2, void *ret);
void trailz64(void *arg1, void *dummy1, void *dummy2, void *ret);
void leadz32(void *arg1, void *dummy1, void *dummy2, void *ret);
void leadz64(void *arg1, void *dummy1, void *dummy2, void *ret);
void ceil2pow32(void *arg1, void *dummy1, void *dummy2, void *ret);
void ceil2pow64(void *arg1, void *dummy1, void *dummy2, void *ret);
void leapyear32(void *arg1, void *dummy1, void *dummy2, void *ret);
void leapyear64(void *arg1, void *dummy1, void *dummy2, void *ret);

/* TODO: support for big endian architectures */
#define zpcgetval8(ptr)                                                 \
    (*(int8_t *)ptr)
#define zpcgetval16(ptr)                                                \
    (*(int16_t *)ptr)
#define zpcgetval32(ptr)                                                \
    (*(int32_t *)ptr)
#define zpcgetval64(ptr)                                                \
    (*(int64_t *)ptr)
#define zpcgetvalu8(ptr)                                                \
    (*(uint8_t *)ptr)
#define zpcgetvalu16(ptr)                                               \
    (*(uint16_t *)ptr)
#define zpcgetvalu32(ptr)                                               \
    (*(uint32_t *)ptr)
#define zpcgetvalu64(ptr)                                               \
    (*(uint64_t *)ptr)
#if 0
#define zpcsetval8(ptr, val)                                            \
    *(int8_t *)(ptr) = (val)
#define zpcsetval16(ptr, val)                                           \
    *(int16_t *)(ptr) = (val)
#define zpcsetval32(ptr, val)                                           \
    *(int32_t *)(ptr) = (val)
#endif
#define zpcsetval64(ptr, val)                                           \
    *(int64_t *)(ptr) = (val)

#endif /* __PCALC_OP_H__ */

