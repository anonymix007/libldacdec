#pragma once
#include "utility.h"

typedef struct {
	int Bits;
	int Size;
//	scalar Scale;
	scalar ImdctPrevious[MAX_FRAME_SAMPLES];
	scalar* Window;
	scalar* SinTable;
	scalar* CosTable;
} Mdct;

void InitMdct();
void RunImdct(Mdct* mdct, scalar* input, scalar* output);

