#pragma once
#include "nnxint.h"

typedef struct {
	UINT8 year : 7;
	UINT8 month : 4;
	UINT8 day : 5;
}FatDate;

typedef struct {
	UINT8 hour : 5;
	UINT8 minutes : 6;
	UINT8 seconds : 5;
}FatTime;

#define FAT_READONLY 1
#define FAT_HIDDEN 2
#define FAT_SYSTEM 4
#define FAT_VOLUME_ID 8
#define FAT_DIRECTORY 16
#define FAT_ARCHIVE 32
#define FAT_LFN (1|2|4|8);

