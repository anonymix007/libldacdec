#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "ldacdec.h"
#include "fixp_ldac.h"
#include "utility.h"


int ShuffleTables[9][256];
static void GenerateShuffleTable(int sizeBits)
{
	const int size = 1 << sizeBits;
	int* table = ShuffleTables[sizeBits];

	for (int i = 0; i < size; i++)
	{
		table[i] = BitReverse32(i ^ (i / 2), sizeBits);
	}
}

#ifndef FIXEDPOINT
scalar MdctWindow[2][256];
scalar ImdctWindow[2][256];
scalar SinTables[9][256];
scalar CosTables[9][256];
static void GenerateTrigTables(int sizeBits)
{
	const int size = 1 << sizeBits;
	scalar* sinTab = SinTables[sizeBits];
	scalar* cosTab = CosTables[sizeBits];

	for (int i = 0; i < size; i++)
	{
		const scalar value = M_PI * (4 * i + 1) / (4 * size);
		sinTab[i] = sin(value);
		cosTab[i] = cos(value);
	}
}
static void GenerateMdctWindow(int frameSizePower)
{
	const int frameSize = 1 << frameSizePower;
	scalar* mdct = MdctWindow[frameSizePower - 7];

	for (int i = 0; i < frameSize; i++)
	{
		mdct[i] = (sin(((i + 0.5) / frameSize - 0.5) * M_PI) + 1.0) * 0.5;
	}
}
static void GenerateImdctWindow(int frameSizePower)
{
	const int frameSize = 1 << frameSizePower;
	scalar* imdct = ImdctWindow[frameSizePower - 7];
	scalar* mdct = MdctWindow[frameSizePower - 7];

	for (int i = 0; i < frameSize; i++)
	{
		imdct[i] = mdct[i] / (mdct[frameSize - 1 - i] * mdct[frameSize - 1 - i] + mdct[i] * mdct[i]);
	}
}

void InitMdct()
{
	for (int i = 0; i < 9; i++)
	{
		GenerateTrigTables(i);
		GenerateShuffleTable(i);
	}		
    GenerateMdctWindow(7);
    GenerateImdctWindow(7);

    GenerateMdctWindow(8);
	GenerateImdctWindow(8);
}

#else
#include "tables_fixp.h"
void InitMdct()
{
	for (int i = 0; i < 9; i++)
	{
		GenerateShuffleTable(i);
	}		
}
#endif



static void Dct4(Mdct* mdct, scalar* input, scalar* output);


#ifndef FIXEDPOINT
void RunImdct(Mdct* mdct, scalar* input, scalar* output)
{
	const int size = 1 << mdct->Bits;
	const int half = size / 2;
	scalar dctOut[MAX_FRAME_SAMPLES] = { 0.f };
	const scalar* window = ImdctWindow[mdct->Bits - 7];
	scalar* previous = mdct->ImdctPrevious;

    Dct4(mdct, input, dctOut);
	
    for (int i = 0; i < half; i++)
	{
		output[i] = window[i] * dctOut[i + half] + previous[i];
		output[i + half] = window[i + half] * -dctOut[size - 1 - i] - previous[i + half];
		previous[i] = window[size - 1 - i] * -dctOut[half - i - 1];
		previous[i + half] = window[half - i - 1] * dctOut[i];
    }
}

static void Dct4(Mdct* mdct, scalar* input, scalar* output)
{
	int MdctBits = mdct->Bits;
	int MdctSize = 1 << MdctBits;
	const int* shuffleTable = ShuffleTables[MdctBits];
	const scalar* sinTable = SinTables[MdctBits];
	const scalar* cosTable = CosTables[MdctBits];
	scalar dctTemp[MAX_FRAME_SAMPLES];

	int size = MdctSize;
	int lastIndex = size - 1;
	int halfSize = size / 2;

	for (int i = 0; i < halfSize; i++)
	{
		int i2 = i * 2;
		scalar a = input[i2];
		scalar b = input[lastIndex - i2];
		scalar sin = sinTable[i];
		scalar cos = cosTable[i];
		dctTemp[i2] = a * cos + b * sin;
		dctTemp[i2 + 1] = a * sin - b * cos;
	}
	int stageCount = MdctBits - 1;

	for (int stage = 0; stage < stageCount; stage++)
	{
		int blockCount = 1 << stage;
		int blockSizeBits = stageCount - stage;
		int blockHalfSizeBits = blockSizeBits - 1;
		int blockSize = 1 << blockSizeBits;
		int blockHalfSize = 1 << blockHalfSizeBits;
		sinTable = SinTables[blockHalfSizeBits];
		cosTable = CosTables[blockHalfSizeBits];

		for (int block = 0; block < blockCount; block++)
		{
			for (int i = 0; i < blockHalfSize; i++)
			{
				int frontPos = (block * blockSize + i) * 2;
				int backPos = frontPos + blockSize;
				scalar a = dctTemp[frontPos] - dctTemp[backPos];
				scalar b = dctTemp[frontPos + 1] - dctTemp[backPos + 1];
				scalar sin = sinTable[i];
				scalar cos = cosTable[i];
				dctTemp[frontPos] += dctTemp[backPos];
				dctTemp[frontPos + 1] += dctTemp[backPos + 1];
				dctTemp[backPos] = a * cos + b * sin;
				dctTemp[backPos + 1] = a * sin - b * cos;
			}
		}
	}

	for (int i = 0; i < MdctSize; i++)
	{
		output[i] = dctTemp[shuffleTable[i]];
	}
    //printf("[%.15lf %.15lf]\n", d_min, d_max);
}
#else

void RunImdct(Mdct* mdct, scalar* input, scalar* output)
{
	const int size = 1 << mdct->Bits;
	const int half = size / 2;
	scalar dctOut[MAX_FRAME_SAMPLES] = { 0 };
	const scalar* window = ImdctWindow[mdct->Bits - 7];
	scalar* previous = mdct->ImdctPrevious;

    Dct4(mdct, input, dctOut);
	
    for (int i = 0; i < half; i++)
	{
		output[i] = mul_rsftrnd_ldac(window[i], dctOut[i + half], 31) + previous[i];
		output[i + half] = mul_rsftrnd_ldac(window[i + half], -dctOut[size - 1 - i], 31) - previous[i + half];
		previous[i] = mul_rsftrnd_ldac(window[size - 1 - i], -dctOut[half - i - 1], 31);
		previous[i + half] = mul_rsftrnd_ldac(window[half - i - 1], dctOut[i], 31);
    }
}

static void Dct4(Mdct* mdct, scalar* input, scalar* output)
{
	int MdctBits = mdct->Bits;
	int MdctSize = 1 << MdctBits;
	const int* shuffleTable = ShuffleTables[MdctBits];
	const scalar* sinTable = SinTables[MdctBits];
	const scalar* cosTable = CosTables[MdctBits];
	scalar dctTemp[MAX_FRAME_SAMPLES];

	int size = MdctSize;
	int lastIndex = size - 1;
	int halfSize = size / 2;

	for (int i = 0; i < halfSize; i++)
	{
		int i2 = i * 2;
		scalar a = input[i2];
		scalar b = input[lastIndex - i2];
		scalar sin = sinTable[i];
		scalar cos = cosTable[i];
		dctTemp[i2] = mul_rsftrnd_ldac(a, cos, 31) + mul_rsftrnd_ldac(b, sin, 31);
		dctTemp[i2 + 1] = mul_rsftrnd_ldac(a, sin, 31) - mul_rsftrnd_ldac(b, cos, 31);
	}
	int stageCount = MdctBits - 1;

	for (int stage = 0; stage < stageCount; stage++)
	{
		int blockCount = 1 << stage;
		int blockSizeBits = stageCount - stage;
		int blockHalfSizeBits = blockSizeBits - 1;
		int blockSize = 1 << blockSizeBits;
		int blockHalfSize = 1 << blockHalfSizeBits;
		sinTable = SinTables[blockHalfSizeBits];
		cosTable = CosTables[blockHalfSizeBits];

		for (int block = 0; block < blockCount; block++)
		{
			for (int i = 0; i < blockHalfSize; i++)
			{
				int frontPos = (block * blockSize + i) * 2;
				int backPos = frontPos + blockSize;
				scalar a = dctTemp[frontPos] - dctTemp[backPos];
				scalar b = dctTemp[frontPos + 1] - dctTemp[backPos + 1];
				scalar sin = sinTable[i];
				scalar cos = cosTable[i];
				dctTemp[frontPos] += dctTemp[backPos];
				dctTemp[frontPos + 1] += dctTemp[backPos + 1];
				dctTemp[backPos] = mul_rsftrnd_ldac(a, cos, 31) + mul_rsftrnd_ldac(b, sin, 31);
				dctTemp[backPos + 1] = mul_rsftrnd_ldac(a, sin, 31) - mul_rsftrnd_ldac(b, cos, 31);
			}
		}
	}

	for (int i = 0; i < MdctSize; i++)
	{
		output[i] = dctTemp[shuffleTable[i]];
	}    
}
#endif
