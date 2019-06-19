#pragma once
#include "nnxint.h"

extern UINT64*** PML4[512];
//extern UINT64** PML4e[512];

UINT64*** GetCR3();
void SetCR3(UINT64***);
void PagingTest();
void PagingInit();