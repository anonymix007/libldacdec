#ifndef _FIXP_LDAC_H
#define _FIXP_LDAC_H

/***************************************************************************************************
    Macro Definitions
***************************************************************************************************/

#define LDAC_MAX_32BIT ((int32)0x7fffffffL)
#define LDAC_MIN_32BIT ((int32)0x80000000L)

#define LDAC_C_BLKFLT   31

#define LDAC_Q_SETPCM   15

#define LDAC_Q_MDCT_WIN 30
#define LDAC_Q_MDCT_COS 31
#define LDAC_Q_MDCT_SIN 31

#define LDAC_Q_NORM1    15
#define LDAC_Q_NORM2    31

#define LDAC_Q_QUANT1   47
#define LDAC_Q_QUANT2    0
#define LDAC_Q_QUANT3   15
#define LDAC_Q_QUANT4   47

#define LDAC_Q_DEQUANT1  0
#define LDAC_Q_DEQUANT2  0
#define LDAC_Q_DEQUANT3 31

#define LDAC_Q_NORM (15+(LDAC_Q_NORM2-LDAC_Q_NORM1))

/***************************************************************************************************
    Function Declarations
***************************************************************************************************/
/* func_fixp_ldac.c */
int32 sftrnd_ldac(int32, int);
#define lsft_ldac(x, n)    ((x) << (n))
#define rsft_ldac(x, n)    ((x) >> (n))
#define rsft_ro_ldac(x, n) (((x) + (1 << (n-1))) >> (n))

#define lsftrnd_ldac(x, n)        (int32)((int64)(x) << (-(n)))
#define rsftrnd_ldac(x, n)        (int32)(((int64)(x) + ((int64)1 << ((n)-1))) >> (n))
#define mul_lsftrnd_ldac(x, y, n) (int32)(((int64)(x) * (int64)(y)) << (-(n)))
#define mul_lsftrnd_ldac2(x, y, n) (int32)(((int64)(x) * (int64)(y)) << ((n)))
#define mul_rsftrnd_ldac(x, y, n) (int32)((((int64)(x) * (int64)(y)) + ((int64)1 << ((n)-1))) >> (n))

int get_bit_length_ldac(int32);
int32 get_absmax_ldac(int32 *, int);

#endif /* _FIXP_LDAC_H */
