#include "utility.h"
/*******************************************************************************
    Subfunction: Check Saturation
*******************************************************************************/
__inline static int32 check_sature_ldac(
int64 val)
{

    return (int32)val;
}

/*******************************************************************************
    Shift and Round
*******************************************************************************/
int32 sftrnd_ldac(
int32 in,
int shift)
{
    int64 out;

    if (shift > 0) {
        out = ((int64)in + ((int64)1 << (shift-1))) >> shift;
    }
    else {
        out = (int64)in << (-shift);
    }

    return check_sature_ldac(out);
}


/*******************************************************************************
    Get Bit Length of Value
*******************************************************************************/
int get_bit_length_ldac(
int32 val)
{
    int len;

    len = 0;
    while (val > 0) {
        val >>= 1;
        len++;
    }

    return len;
}

/*******************************************************************************
    Get Maximum Absolute Value
*******************************************************************************/
inline int32 abs(int32 x){
    return x<0? -x:x;
}

int32 get_absmax_ldac(
int32 *p_x,
int num)
{
    int i;
    int32 abmax, val;

    abmax = abs(p_x[0]);
    for (i = 1; i < num; i++) {
        val = abs(p_x[i]);
        if (abmax < val) {
            abmax = val;
        }
    }

    return abmax;
}

