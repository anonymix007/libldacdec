#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include "ldacdec.h"
#include "spectrum.h"
#include "log.h"
#include "fixp_ldac.h"


#define LDAC_MAXNQUS          34
#define LDAC_MAXNSPS          16

/** Spectrum/Residual Data **/
#define LDAC_NIDWL            16
#define LDAC_NIDSF            32
#define LDAC_2DIMSPECBITS      3
#define LDAC_N2DIMSPECENCTBL  16
#define LDAC_N2DIMSPECDECTBL   8
#define LDAC_4DIMSPECBITS      7
#define LDAC_N4DIMSPECENCTBL 256
#define LDAC_N4DIMSPECDECTBL  81

/***************************************************************************************************
    Encoding/Decoding Tables for Spectrum Data
***************************************************************************************************/
static const uint8_t ga_wl_ldac[LDAC_NIDWL] = {
    0,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
};

static const uint8_t ga_nsps_ldac[LDAC_MAXNQUS] = {
      2,  2,  2,  2,  2,  2,  2,  2,
      4,  4,  4,  4,
      4,  4,  4,  4,
      4,  4,  4,  4,
      8,  8,
      8,  8,
     16, 16,
     16, 16,
     16, 16,
     16, 16,
     16, 16,
};

static const uint16_t ga_isp_ldac[LDAC_MAXNQUS+1] = {
      0,  2,  4,  6,  8, 10, 12, 14,
     16, 20, 24, 28,
     32, 36, 40, 44,
     48, 52, 56, 60,
     64, 72,
     80, 88,
     96,112,
    128,144,
    160,176,
    192,208,
    224,240,
    256,
};

static const int decode2DSpectrum[8] = {
      0,   1,   2,   4,   6,   8,   9,  10 
};

static const int decode4DSpectrum[128] = {
      0,   1,   2,   4,   5,   6,   8,   9,  10,  16,  17,  18,  20,  21,  22,  24, 
     25,  26,  32,  33,  34,  36,  37,  38,  40,  41,  42,  64,  65,  66,  68,  69, 
     70,  72,  73,  74,  80,  81,  82,  84,  85,  86,  88,  89,  90,  96,  97,  98, 
    100, 101, 102, 104, 105, 106, 128, 129, 130, 132, 133, 134, 136, 137, 138, 144, 
    145, 146, 148, 149, 150, 152, 153, 154, 160, 161, 162, 164, 165, 166, 168, 169, 
    170,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 
};

/***************************************************************************************************
    Quantization Tables
***************************************************************************************************/
/* Inverse of Quantize Factor for Spectrum/Residual Quantization */

#ifndef FIXEDPOINT
static const scalar QuantizerStepSize[16] = {
    2.0000000000000000e+00, 6.6666666666666663e-01, 2.8571428571428570e-01, 1.3333333333333333e-01,
    6.4516129032258063e-02, 3.1746031746031744e-02, 1.5748031496062992e-02, 7.8431372549019607e-03,
    3.9138943248532287e-03, 1.9550342130987292e-03, 9.7703957010258913e-04, 4.8840048840048840e-04,
    2.4417043096081065e-04, 1.2207776353537203e-04, 6.1037018951994385e-05, 3.0518043793392844e-05,
};

static const scalar QuantizerFineStepSize[16] =
{
	3.0518043793392844e-05, 1.0172681264464281e-05, 4.3597205419132631e-06, 2.0345362528928561e-06,
	9.8445302559331759e-07, 4.8441339354591809e-07, 2.4029955742829012e-07, 1.1967860311134448e-07,
	5.9722199204291275e-08, 2.9831909866464167e-08, 1.4908668194134265e-08, 7.4525137468602791e-09,
	3.7258019525568114e-09, 1.8627872668859698e-09, 9.3136520869755679e-10, 4.6567549848772173e-10
};
#else
static const scalar QuantizerStepSize[16] = {                   /* Q30 */
    0x7fffffff, 0x2aaaaaab, 0x12492492, 0x08888889, 0x04210842, 0x02082082, 0x01020408, 0x00808081, 
    0x00402010, 0x00200802, 0x00100200, 0x00080080, 0x00040020, 0x00020008, 0x00010002, 0x00008001, 
};
static const scalar QuantizerFineStepSize[16] = {                /* Q31 */
    0x00010001, 0x00005556, 0x00002492, 0x00001111, 0x00000842, 0x00000410, 0x00000204, 0x00000101, 
    0x00000080, 0x00000040, 0x00000020, 0x00000010, 0x00000008, 0x00000004, 0x00000002, 0x00000001, 
};
#endif

int decodeSpectrum( channel_t *this, BitReaderCxt *br )
{
    frame_t *frame = this->frame;
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        int startSubband = ga_isp_ldac[i];
        int endSubband   = ga_isp_ldac[i+1];
        int nsps = ga_nsps_ldac[i];
        int wl = ga_wl_ldac[this->precisions[i]];
        
        if( this->precisions[i] == 1 )
        {
            int idxSpectrum = startSubband;
            if( nsps == 2 )
            {
                int value = decode2DSpectrum[ReadInt( br, LDAC_2DIMSPECBITS )];
                this->quantizedSpectra[idxSpectrum]   = ((value>>2)&3) - 1;
                this->quantizedSpectra[idxSpectrum+1] =  (value&3)     - 1;
            } else
            {
                for (int j = 0; j < nsps/4; j++, idxSpectrum+=4)
                {
                    int value = decode4DSpectrum[ReadInt( br, LDAC_4DIMSPECBITS )];
                    this->quantizedSpectra[idxSpectrum]   = ((value>>6)&3) - 1;
                    this->quantizedSpectra[idxSpectrum+1] = ((value>>4)&3) - 1;
                    this->quantizedSpectra[idxSpectrum+2] = ((value>>2)&3) - 1;
                    this->quantizedSpectra[idxSpectrum+3] =  (value&3)     - 1;
                }
            }
        } else
        {
            for( int j = startSubband; j<endSubband; ++j )
                this->quantizedSpectra[j] = ReadSignedInt( br, wl );
        }
    }

    LOG_ARRAY_LEN( this->quantizedSpectra, "%3d, ", frame->quantizationUnitCount );
    return 0;
}

int decodeSpectrumFine( channel_t *this, BitReaderCxt *br )
{
    frame_t *frame = this->frame;
    memset( this->quantizedSpectraFine, 0, sizeof( this->quantizedSpectraFine ) );
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        if( this->precisionsFine[i] > 0 )
        {
            int startSubband = ga_isp_ldac[i];
            int endSubband   = ga_isp_ldac[i+1];
            int wl = ga_wl_ldac[this->precisionsFine[i]];
            for( int j=startSubband; j<endSubband; ++j )
            {
                this->quantizedSpectraFine[j] = ReadSignedInt( br, wl );
            }
        }
    }

    LOG_ARRAY_LEN( this->quantizedSpectraFine, "%3d, ", frame->quantizationUnitCount );
    return 0;
}

#ifndef FIXEDPOINT
static void dequantizeQuantUnit( channel_t* this, int band )
{
    //static double d_min = DBL_MAX, d_max = DBL_MIN;
    const int subBandIndex = ga_isp_ldac[band];
    const int subBandCount = ga_nsps_ldac[band];
    const scalar stepSize = QuantizerStepSize[this->precisions[band]];
    const scalar stepSizeFine = QuantizerFineStepSize[this->precisions[band]];

    for( int sb=0; sb<subBandCount; ++sb )
    {
        const scalar coarse = this->quantizedSpectra[subBandIndex+sb] * stepSize;
        const scalar fine = this->quantizedSpectraFine[subBandIndex+sb] * stepSizeFine;
        this->spectra[subBandIndex + sb] = coarse + fine;
    }
}
#else
static void dequantizeQuantUnit( channel_t* this, int band )
{
    static int imin1 = INT_MAX, imax1 = INT_MIN;
    static int imin2 = INT_MAX, imax2 = INT_MIN;
    static int imin3 = INT_MAX, imax3 = INT_MIN;
    const int subBandIndex = ga_isp_ldac[band];
    const int subBandCount = ga_nsps_ldac[band];
    const scalar stepSize = QuantizerStepSize[this->precisions[band]];
    const scalar stepSizeFine = QuantizerFineStepSize[this->precisions[band]];

    for( int sb=0; sb<subBandCount; ++sb )
    {
        //Q31 <- Q00 * Q30
        //Q31 <- Q00 * Q31
        
        const scalar coarse = mul_lsftrnd_ldac(this->quantizedSpectra[subBandIndex+sb], stepSize, -1);
        const scalar fine = mul_lsftrnd_ldac(this->quantizedSpectraFine[subBandIndex+sb], stepSizeFine, 0);
        imin1 = min(imin1, coarse);
        imax1 = max(imax1, coarse);

        imin2 = min(imin2, fine);
        imax2 = max(imax2, fine);

        this->spectra[subBandIndex + sb] = coarse + fine;

        imin3 = min(imin3, this->spectra[subBandIndex + sb]);
        imax3 = max(imax3, this->spectra[subBandIndex + sb]);
    }
    //TODO: Check if we need to fix the range 
    //printf("ideq1[%d; %d]\n", imin1, imax1);
    //printf("ideq2[%d; %d]\n", imin2, imax2);
    //printf("ideq3[%d; %d]\n", imin3, imax3);
}
#endif

void dequantizeSpectra( channel_t *this )
{
    frame_t *frame = this->frame;
    
    memset( this->spectra, 0, sizeof(this->spectra) );
    
    for( int i=0; i<frame->quantizationUnitCount; ++i )
    {
        dequantizeQuantUnit( this, i );
    }
  
    LOG_ARRAY_LEN( this->spectra, "%e, ", ga_isp_ldac[frame->quantizationUnitCount-1] + ga_nsps_ldac[frame->quantizationUnitCount-1] ); 
}


#ifndef FIXEDPOINT
static const scalar spectrumScale[32] =
{
	3.0517578125e-5, 6.1035156250e-5, 1.2207031250e-4, 2.4414062500e-4,
	4.8828125000e-4, 9.7656250000e-4, 1.9531250000e-3, 3.9062500000e-3,
	7.8125000000e-3, 1.5625000000e-2, 3.1250000000e-2, 6.2500000000e-2,
	1.2500000000e-1, 2.5000000000e-1, 5.0000000000e-1, 1.0000000000e+0,
	2.0000000000e+0, 4.0000000000e+0, 8.0000000000e+0, 1.6000000000e+1,
	3.2000000000e+1, 6.4000000000e+1, 1.2800000000e+2, 2.5600000000e+2,
	5.1200000000e+2, 1.0240000000e+3, 2.0480000000e+3, 4.0960000000e+3,
	8.1920000000e+3, 1.6384000000e+4, 3.2768000000e+4, 6.5536000000e+4
};
#else
static const scalar spectrumScale[32] = {                      /* Q15 */
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080, 
    0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000, 
    0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000, 
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x7fffffff, 
};
#endif


#ifndef FIXEDPOINT
void scaleSpectrum(channel_t* this)
{
    const frame_t *frame = this->frame;
	const int quantUnitCount = frame->quantizationUnitCount;
  	scalar *const spectra = this->spectra;

	for (int i = 0; i < quantUnitCount; i++)
	{
        const int startSubBand = ga_isp_ldac[i];
        const int endSubBand   = ga_isp_ldac[i+1];
        if( this->scaleFactors[i] > 0 )
        {
	       for (int sb = startSubBand; sb < endSubBand; sb++)
		   {
                spectra[sb] = spectra[sb] * spectrumScale[this->scaleFactors[i]];
		   }
        } 
	}

    LOG_ARRAY_LEN( this->spectra, "%e, ", ga_isp_ldac[frame->quantizationUnitCount-1] + ga_nsps_ldac[frame->quantizationUnitCount-1] );
}

#else
void scaleSpectrum(channel_t* this)
{    
    const frame_t *frame = this->frame;
	const int quantUnitCount = frame->quantizationUnitCount;
	int *const spectra = this->spectra;

	for (int i = 0; i < quantUnitCount; i++)
	{
        const int startSubBand = ga_isp_ldac[i];
        const int endSubBand   = ga_isp_ldac[i+1];
        if( this->scaleFactors[i] > 0 )
        {
	       for (int sb = startSubBand; sb < endSubBand; sb++)
		   {
		        spectra[sb] = mul_rsftrnd_ldac(spectra[sb], spectrumScale[this->scaleFactors[i]], 31);
		   }
        } 
	}
    //TODO: Check the range here too
    LOG_ARRAY_LEN( this->spectra, "%e, ", ga_isp_ldac[frame->quantizationUnitCount-1] + ga_nsps_ldac[frame->quantizationUnitCount-1] );
}
#endif
