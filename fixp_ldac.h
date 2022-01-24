#ifndef _FIXP_LDAC_H
#define _FIXP_LDAC_H

#define lsft_ldac(x, n)    ((x) << (n))
#define rsft_ldac(x, n)    ((x) >> (n))
#define rsft_ro_ldac(x, n) (((x) + (1 << (n-1))) >> (n))

#define lsftrnd_ldac(x, n)        (int32)((int64)(x) << (-(n)))
#define rsftrnd_ldac(x, n)        (int32)(((int64)(x) + ((int64)1 << ((n)-1))) >> (n))
#define mul_lsftrnd_ldac(x, y, n) (int32)(((int64)(x) * (int64)(y)) << (-(n)))
#define mul_lsftrnd_ldac2(x, y, n) (int32)(((int64)(x) * (int64)(y)) << ((n)))
#define mul_rsftrnd_ldac(x, y, n) (int32)((((int64)(x) * (int64)(y)) + ((int64)1 << ((n)-1))) >> (n))


#endif /* _FIXP_LDAC_H */
