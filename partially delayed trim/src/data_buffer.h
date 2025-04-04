//////////////////////////////////////////////////////////////////////////////////
// data_buffer.h for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Data Buffer Manager
// File Name: data_buffer.h
//
// Version: v1.0.0
//
// Description:
//   - define parameters, data structure and functions of data buffer manager
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#ifndef DATA_BUFFER_H_
#define DATA_BUFFER_H_

#include "ftl_config.h"

#define BITMAP_SIZE 									(USER_PAGES_PER_SSD/64)
#define AVAILABLE_DATA_BUFFER_ENTRY_COUNT				(128 * USER_DIES)
#define AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT		(USER_DIES)

#define DATA_BUF_NONE	0xffff
#define DATA_BUF_FAIL	0xffff
#define DATA_BUF_DIRTY	1
#define DATA_BUF_CLEAN	0

#define MAX_SAMPLES 1000
#define MAX_PER_UTIL 10
#define MAX_UTIL 101

#define FindDataBufHashTableEntry(logicalSliceAddr) ((logicalSliceAddr) % AVAILABLE_DATA_BUFFER_ENTRY_COUNT)

#define AVAILABLE_DSM_RANGE_ENTRY_COUNT					1024

typedef struct _DATA_BUF_ENTRY {
	unsigned int logicalSliceAddr;
	unsigned int prevEntry : 16;
	unsigned int nextEntry : 16;
	unsigned int blockingReqTail : 16;
	unsigned int hashPrevEntry : 16;
	unsigned int hashNextEntry : 16;
	unsigned int dirty : 1;
	unsigned int blk0 : 8;
	unsigned int blk1 : 8;
	unsigned int blk2 : 8;
	unsigned int blk3 : 8;

	unsigned int reserved0 : 15;
} DATA_BUF_ENTRY, *P_DATA_BUF_ENTRY;

typedef struct _DATA_BUF_MAP{
	DATA_BUF_ENTRY dataBuf[AVAILABLE_DATA_BUFFER_ENTRY_COUNT];
} DATA_BUF_MAP, *P_DATA_BUF_MAP;

typedef struct _DATA_BUF_LRU_LIST {
	unsigned int headEntry : 16;
	unsigned int tailEntry : 16;
} DATA_BUF_LRU_LIST, *P_DATA_BUF_LRU_LIST;

typedef struct _DATA_BUF_HASH_ENTRY{
	unsigned int headEntry : 16;
	unsigned int tailEntry : 16;
} DATA_BUF_HASH_ENTRY, *P_DATA_BUF_HASH_ENTRY;


typedef struct _DATA_BUF_HASH_TABLE{
	DATA_BUF_HASH_ENTRY dataBufHash[AVAILABLE_DATA_BUFFER_ENTRY_COUNT];
} DATA_BUF_HASH_TABLE, *P_DATA_BUF_HASH_TABLE;


typedef struct _TEMPORARY_DATA_BUF_ENTRY {
	unsigned int blockingReqTail : 16;
	unsigned int reserved0 : 16;
} TEMPORARY_DATA_BUF_ENTRY, *P_TEMPORARY_DATA_BUF_ENTRY;

typedef struct _TEMPORARY_DATA_BUF_MAP{
	TEMPORARY_DATA_BUF_ENTRY tempDataBuf[AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT];
} TEMPORARY_DATA_BUF_MAP, *P_TEMPORARY_DATA_BUF_MAP;

typedef struct _ASYNC_TRIM_BIT_MAP {
	unsigned long long writeBitMap[BITMAP_SIZE];
} ASYNC_TRIM_BIT_MAP, *P_ASYNC_TRIM_BIT_MAP;

typedef struct _DATASET_MANAGEMENT_RANGE
{
	DSMRangeUnion ContextAttributes;
	unsigned int lengthInLogicalBlocks;
	unsigned int RealLB;
	unsigned int startingLBA[2];
	unsigned int prevEntry : 16;
	unsigned int nextEntry : 16;
	unsigned int hashPrevEntry : 16;
	unsigned int hashNextEntry : 16;
} DATASET_MANAGEMENT_RANGE;

typedef struct _DSM_RANGE{
	DATASET_MANAGEMENT_RANGE dsmRange[AVAILABLE_DSM_RANGE_ENTRY_COUNT];
} DSM_RANGE, *P_DSM_RANGE;

typedef struct _DSM_RANGE_LRU_LIST {
	unsigned int headEntry : 16;
	unsigned int tailEntry : 16;
} DSM_RANGE_LRU_LIST, *P_DSM_RANGE_LRU_LIST;

typedef struct _DSM_RANGE_HASH_ENTRY{
	unsigned int headEntry : 16;
	unsigned int tailEntry : 16;
} DSM_RAGNGE_HASH_ENTRY, *P_DSM_RANGE_HASH_ENTRY;

typedef struct _DSM_RANGE_HASH_TABLE{
	DSM_RAGNGE_HASH_ENTRY dsmRangeHash[33];
	unsigned int Range_Flag[33];
} DSM_RANGE_HASH_TABLE, *P_DSM_RANGE_HASH_TABLE;

typedef struct _Regression_Entry {
	unsigned int util;
	unsigned int valid_page;
} Regression_Entry, P_Regression_Entry;

typedef struct _REG_BUF_MAP {
	Regression_Entry regressionEnrty[MAX_SAMPLES];
	unsigned int head;
	unsigned int tail;
} REG_BUF_MAP, *P_REG_BUF_MAP;

void InitDataBuf();
unsigned int CheckDataBufHit(unsigned int reqSlotTag);
unsigned int AllocateDataBuf();
void UpdateDataBufEntryInfoBlockingReq(unsigned int bufEntry, unsigned int reqSlotTag);

unsigned int AllocateTempDataBuf(unsigned int dieNo);
void UpdateTempDataBufEntryInfoBlockingReq(unsigned int bufEntry, unsigned int reqSlotTag);

void PutToDataBufHashList(unsigned int bufEntry);
void SelectiveGetFromDataBufHashList(unsigned int bufEntry);

unsigned int CheckDataBufHitbyLSA(unsigned int logicalSliceAddr);
unsigned int AllocateDSMBuf();
void PutDSMBuftoLRUList(unsigned int bufEntry);
void PutToDsmRangeHashList(unsigned int bufEntry);
void SelectiveGetFromDsmRangeHashList(unsigned int bufEntry);
unsigned int FindDsmRangeHashTableEntry(unsigned int length);
unsigned int SmallestDSMBuftoLRUList();
void TRIM (unsigned int lba, unsigned int blk0, unsigned int blk1, unsigned int blk2, unsigned int blk3);

unsigned int cmp(const void *a, const void *b);
void train_model();
void add_sample(unsigned int util, unsigned int valid_page);
unsigned int get_sample_count();
unsigned int predict_valid_page(int util);

extern P_DATA_BUF_MAP dataBufMapPtr;
extern DATA_BUF_LRU_LIST dataBufLruList;
extern P_DATA_BUF_HASH_TABLE dataBufHashTable;
extern P_TEMPORARY_DATA_BUF_MAP tempDataBufMapPtr;

extern P_ASYNC_TRIM_BIT_MAP asyncTrimBitMapPtr;
extern P_DSM_RANGE dsmRangePtr;
extern DSM_RANGE_LRU_LIST dsmRangeLruList;
extern P_DSM_RANGE_HASH_TABLE dsmRangeHashTable;

extern P_REG_BUF_MAP regressionBufMapPtr;

#endif /* DATA_BUFFER_H_ */
