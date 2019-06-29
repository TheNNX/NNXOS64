#pragma once
#include "nnxint.h"
void outb(UINT16, UINT8); 
void outw(UINT16, UINT16); 
void outd(UINT16, UINT32);
UINT8 inb(UINT16); 
UINT16 inw(UINT16);
UINT32 ind(UINT16);