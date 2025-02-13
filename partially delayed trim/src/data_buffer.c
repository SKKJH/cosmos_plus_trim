//////////////////////////////////////////////////////////////////////////////////
// data_buffer.c for Cosmos+ OpenSSD
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
// File Name: data_buffer.c
//
// Version: v1.0.0
//
// Description:
//   - manage data buffer used to transfer data between host system and NAND device
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include <assert.h>
#include "memory_map.h"


P_DATA_BUF_MAP dataBufMapPtr;
DATA_BUF_LRU_LIST dataBufLruList;
P_DATA_BUF_HASH_TABLE dataBufHashTablePtr;
P_TEMPORARY_DATA_BUF_MAP tempDataBufMapPtr;
P_ASYNC_TRIM_BIT_MAP asyncTrimBitMapPtr;

P_DSM_RANGE dsmRangePtr;
DSM_RANGE_LRU_LIST dsmRangeLruList;
P_DSM_RANGE_HASH_TABLE dsmRangeHashTable;

void InitDataBuf()
{
	int bufEntry;

	dataBufMapPtr = (P_DATA_BUF_MAP) DATA_BUFFER_MAP_ADDR;
	dataBufHashTablePtr = (P_DATA_BUF_HASH_TABLE)DATA_BUFFFER_HASH_TABLE_ADDR;
	tempDataBufMapPtr = (P_TEMPORARY_DATA_BUF_MAP)TEMPORARY_DATA_BUFFER_MAP_ADDR;

	asyncTrimBitMapPtr = (P_ASYNC_TRIM_BIT_MAP)ASYNC_TRIM_BIT_MAP_ADDR;
	dsmRangePtr = (P_DSM_RANGE)DSM_RANGE_ADDR;
	dsmRangeHashTable = (P_DSM_RANGE_HASH_TABLE)DSM_RANGE_HASH_TABLE_ADDR;

	for (int i = 0; i < AVAILABLE_DSM_RANGE_ENTRY_COUNT; i++)
	{
		dsmRangePtr->dsmRange[i].lengthInLogicalBlocks = 0;
		dsmRangePtr->dsmRange[i].startingLBA[0] = 0xffffffff;
		dsmRangePtr->dsmRange[i].startingLBA[1] = 0xffffffff;
		dsmRangePtr->dsmRange[i].ContextAttributes.value = 0;

		dsmRangePtr->dsmRange[i].prevEntry = i-1;
		dsmRangePtr->dsmRange[i].nextEntry = i+1;

		dsmRangePtr->dsmRange[i].hashPrevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[i].hashNextEntry = DATA_BUF_NONE;
	}

	for (int i = 0; i < 21; i++)
	{
		dsmRangeHashTable->dsmRangeHash[i].headEntry = DATA_BUF_NONE;
		dsmRangeHashTable->dsmRangeHash[i].tailEntry = DATA_BUF_NONE;
		dsmRangeHashTable->Range_Flag[i] = 0;
	}

	dsmRangePtr->dsmRange[0].prevEntry = DATA_BUF_NONE;
	dsmRangePtr->dsmRange[AVAILABLE_DSM_RANGE_ENTRY_COUNT - 1].nextEntry = DATA_BUF_NONE;
	dsmRangeLruList.headEntry = 0 ;
	dsmRangeLruList.tailEntry = AVAILABLE_DSM_RANGE_ENTRY_COUNT - 1;

	for (int i = 0; i < BITMAP_SIZE; i++)
	{
		asyncTrimBitMapPtr->writeBitMap[i] = 0ULL;
	}

	for(bufEntry = 0; bufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT; bufEntry++)
	{
		dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr = LSA_NONE;
		dataBufMapPtr->dataBuf[bufEntry].prevEntry = bufEntry-1;
		dataBufMapPtr->dataBuf[bufEntry].nextEntry = bufEntry+1;
		dataBufMapPtr->dataBuf[bufEntry].dirty = DATA_BUF_CLEAN;
		dataBufMapPtr->dataBuf[bufEntry].blockingReqTail =  REQ_SLOT_TAG_NONE;
		dataBufMapPtr->dataBuf[bufEntry].blk0 =  0;
		dataBufMapPtr->dataBuf[bufEntry].blk1 =  0;
		dataBufMapPtr->dataBuf[bufEntry].blk2 =  0;
		dataBufMapPtr->dataBuf[bufEntry].blk3 =  0;

		dataBufHashTablePtr->dataBufHash[bufEntry].headEntry = DATA_BUF_NONE;
		dataBufHashTablePtr->dataBufHash[bufEntry].tailEntry = DATA_BUF_NONE;
		dataBufMapPtr->dataBuf[bufEntry].hashPrevEntry = DATA_BUF_NONE;
		dataBufMapPtr->dataBuf[bufEntry].hashNextEntry = DATA_BUF_NONE;
	}

	dataBufMapPtr->dataBuf[0].prevEntry = DATA_BUF_NONE;
	dataBufMapPtr->dataBuf[AVAILABLE_DATA_BUFFER_ENTRY_COUNT - 1].nextEntry = DATA_BUF_NONE;
	dataBufLruList.headEntry = 0 ;
	dataBufLruList.tailEntry = AVAILABLE_DATA_BUFFER_ENTRY_COUNT - 1;

	for(bufEntry = 0; bufEntry < AVAILABLE_TEMPORARY_DATA_BUFFER_ENTRY_COUNT; bufEntry++)
		tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail =  REQ_SLOT_TAG_NONE;
}

unsigned int CheckDataBufHit(unsigned int reqSlotTag)
{
	unsigned int bufEntry, logicalSliceAddr;

	logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
	bufEntry = dataBufHashTablePtr->dataBufHash[FindDataBufHashTableEntry(logicalSliceAddr)].headEntry;

	while(bufEntry != DATA_BUF_NONE)
	{
		if(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr == logicalSliceAddr)
		{
			if((dataBufMapPtr->dataBuf[bufEntry].nextEntry != DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry != DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].prevEntry].nextEntry = dataBufMapPtr->dataBuf[bufEntry].nextEntry;
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].nextEntry].prevEntry = dataBufMapPtr->dataBuf[bufEntry].prevEntry;
			}
			else if((dataBufMapPtr->dataBuf[bufEntry].nextEntry == DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry != DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].prevEntry].nextEntry = DATA_BUF_NONE;
				dataBufLruList.tailEntry = dataBufMapPtr->dataBuf[bufEntry].prevEntry;
			}
			else if((dataBufMapPtr->dataBuf[bufEntry].nextEntry != DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry== DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].nextEntry].prevEntry  = DATA_BUF_NONE;
				dataBufLruList.headEntry = dataBufMapPtr->dataBuf[bufEntry].nextEntry;
			}
			else
			{
				dataBufLruList.tailEntry = DATA_BUF_NONE;
				dataBufLruList.headEntry = DATA_BUF_NONE;
			}

			if(dataBufLruList.headEntry != DATA_BUF_NONE)
			{
				dataBufMapPtr->dataBuf[bufEntry].prevEntry = DATA_BUF_NONE;
				dataBufMapPtr->dataBuf[bufEntry].nextEntry = dataBufLruList.headEntry;
				dataBufMapPtr->dataBuf[dataBufLruList.headEntry].prevEntry = bufEntry;
				dataBufLruList.headEntry = bufEntry;
			}
			else
			{
				dataBufMapPtr->dataBuf[bufEntry].prevEntry = DATA_BUF_NONE;
				dataBufMapPtr->dataBuf[bufEntry].nextEntry = DATA_BUF_NONE;
				dataBufLruList.headEntry = bufEntry;
				dataBufLruList.tailEntry = bufEntry;
			}

			return bufEntry;
		}
		else
			bufEntry = dataBufMapPtr->dataBuf[bufEntry].hashNextEntry;
	}

	return DATA_BUF_FAIL;
}

unsigned int CheckDataBufHitbyLSA(unsigned int logicalSliceAddr)
{
	unsigned int bufEntry;

	bufEntry = dataBufHashTablePtr->dataBufHash[FindDataBufHashTableEntry(logicalSliceAddr)].headEntry;

	while(bufEntry != DATA_BUF_NONE)
	{
		if(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr == logicalSliceAddr)
		{
			if((dataBufMapPtr->dataBuf[bufEntry].nextEntry != DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry != DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].prevEntry].nextEntry = dataBufMapPtr->dataBuf[bufEntry].nextEntry;
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].nextEntry].prevEntry = dataBufMapPtr->dataBuf[bufEntry].prevEntry;
			}
			else if((dataBufMapPtr->dataBuf[bufEntry].nextEntry == DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry != DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].prevEntry].nextEntry = DATA_BUF_NONE;
				dataBufLruList.tailEntry = dataBufMapPtr->dataBuf[bufEntry].prevEntry;
			}
			else if((dataBufMapPtr->dataBuf[bufEntry].nextEntry != DATA_BUF_NONE) && (dataBufMapPtr->dataBuf[bufEntry].prevEntry== DATA_BUF_NONE))
			{
				dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[bufEntry].nextEntry].prevEntry  = DATA_BUF_NONE;
				dataBufLruList.headEntry = dataBufMapPtr->dataBuf[bufEntry].nextEntry;
			}
			else
			{
				dataBufLruList.tailEntry = DATA_BUF_NONE;
				dataBufLruList.headEntry = DATA_BUF_NONE;
			}

			if(dataBufLruList.headEntry != DATA_BUF_NONE)
			{
				dataBufMapPtr->dataBuf[bufEntry].prevEntry = DATA_BUF_NONE;
				dataBufMapPtr->dataBuf[bufEntry].nextEntry = dataBufLruList.headEntry;
				dataBufMapPtr->dataBuf[dataBufLruList.headEntry].prevEntry = bufEntry;
				dataBufLruList.headEntry = bufEntry;
			}
			else
			{
				dataBufMapPtr->dataBuf[bufEntry].prevEntry = DATA_BUF_NONE;
				dataBufMapPtr->dataBuf[bufEntry].nextEntry = DATA_BUF_NONE;
				dataBufLruList.headEntry = bufEntry;
				dataBufLruList.tailEntry = bufEntry;
			}

			return bufEntry;
		}
		else
			bufEntry = dataBufMapPtr->dataBuf[bufEntry].hashNextEntry;
	}

	return DATA_BUF_FAIL;
}

unsigned int AllocateDataBuf()
{
	unsigned int evictedEntry = dataBufLruList.tailEntry;

	if(evictedEntry == DATA_BUF_NONE)
		assert(!"[WARNING] AllocateDataBuf [WARNING]");

	if(dataBufMapPtr->dataBuf[evictedEntry].prevEntry != DATA_BUF_NONE)
	{
		dataBufMapPtr->dataBuf[dataBufMapPtr->dataBuf[evictedEntry].prevEntry].nextEntry = DATA_BUF_NONE;
		dataBufLruList.tailEntry = dataBufMapPtr->dataBuf[evictedEntry].prevEntry;

		dataBufMapPtr->dataBuf[evictedEntry].prevEntry = DATA_BUF_NONE;
		dataBufMapPtr->dataBuf[evictedEntry].nextEntry = dataBufLruList.headEntry;
		dataBufMapPtr->dataBuf[dataBufLruList.headEntry].prevEntry = evictedEntry;
		dataBufLruList.headEntry = evictedEntry;

	}
	else
	{
		dataBufMapPtr->dataBuf[evictedEntry].prevEntry = DATA_BUF_NONE;
		dataBufMapPtr->dataBuf[evictedEntry].nextEntry = DATA_BUF_NONE;
		dataBufLruList.headEntry = evictedEntry;
		dataBufLruList.tailEntry = evictedEntry;
	}

	SelectiveGetFromDataBufHashList(evictedEntry);

	return evictedEntry;
}

void PutToDsmRangeHashList(unsigned int bufEntry)
{
	unsigned int hashEntry, newLength, currentEntry, prevEntry;

	newLength = dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks;
//	xil_printf("PutToDsmRangeHashList Length: %u\r\n", newLength);

	if (newLength == 0)
		return;

	hashEntry = FindDsmRangeHashTableEntry(newLength);

	dsmRangeHashTable->Range_Flag[hashEntry] = 1;
	currentEntry = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;
	prevEntry = DATA_BUF_NONE;

	while (currentEntry != DATA_BUF_NONE)
	{
		if (dsmRangePtr->dsmRange[currentEntry].lengthInLogicalBlocks <= newLength)
			break;
		prevEntry = currentEntry;
		currentEntry = dsmRangePtr->dsmRange[currentEntry].hashNextEntry;
//		xil_printf("nextEntry2: %u\r\n", currentEntry);
	}

	dsmRangePtr->dsmRange[bufEntry].hashPrevEntry = DATA_BUF_NONE;
	dsmRangePtr->dsmRange[bufEntry].hashNextEntry = DATA_BUF_NONE;
//	xil_printf("nextEntry3: %u\r\n", dsmRangePtr->dsmRange[bufEntry].hashNextEntry);

	if (prevEntry == DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[bufEntry].hashNextEntry = currentEntry;
//		xil_printf("nextEntry4: %u\r\n", dsmRangePtr->dsmRange[bufEntry].hashNextEntry);
		dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry = bufEntry;

		if (currentEntry != DATA_BUF_NONE)
			dsmRangePtr->dsmRange[currentEntry].hashPrevEntry = bufEntry;
		else
			dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry = bufEntry;
	}
	else
	{
		dsmRangePtr->dsmRange[bufEntry].hashPrevEntry = prevEntry;
		dsmRangePtr->dsmRange[bufEntry].hashNextEntry = currentEntry;
//		xil_printf("nextEntry5: %u\r\n", dsmRangePtr->dsmRange[bufEntry].hashNextEntry);
		dsmRangePtr->dsmRange[prevEntry].hashNextEntry = bufEntry;

		if (currentEntry != DATA_BUF_NONE)
			dsmRangePtr->dsmRange[currentEntry].hashPrevEntry = bufEntry;
		else
			dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry = bufEntry;
	}
}

void SelectiveGetFromDsmRangeHashList(unsigned int bufEntry)
{
//	xil_printf("bufEntry: %d\r\n", bufEntry);
	if(dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks != 0)
	{
		unsigned int prevBufEntry, nextBufEntry, hashEntry;

		prevBufEntry =  dsmRangePtr->dsmRange[bufEntry].hashPrevEntry;
		nextBufEntry =  dsmRangePtr->dsmRange[bufEntry].hashNextEntry;
//		xil_printf("nextEntry65: %u\r\n", nextBufEntry);

		hashEntry = FindDsmRangeHashTableEntry(dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks);

		if((nextBufEntry != DATA_BUF_NONE) && (prevBufEntry != DATA_BUF_NONE))
		{
			dsmRangePtr->dsmRange[prevBufEntry].hashNextEntry = nextBufEntry;
			dsmRangePtr->dsmRange[nextBufEntry].hashPrevEntry = prevBufEntry;
		}
		else if((nextBufEntry == DATA_BUF_NONE) && (prevBufEntry != DATA_BUF_NONE))
		{
			dsmRangePtr->dsmRange[prevBufEntry].hashNextEntry = DATA_BUF_NONE;
			dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry = prevBufEntry;
		}
		else if((nextBufEntry != DATA_BUF_NONE) && (prevBufEntry == DATA_BUF_NONE))
		{
			dsmRangePtr->dsmRange[nextBufEntry].hashPrevEntry = DATA_BUF_NONE;
			dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry = nextBufEntry;
		}
		else
		{
			dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry = DATA_BUF_NONE;
			dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry = DATA_BUF_NONE;
			dsmRangeHashTable->Range_Flag[hashEntry] = 0;
		}

		dsmRangePtr->dsmRange[bufEntry].hashPrevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[bufEntry].hashNextEntry = DATA_BUF_NONE;
//		xil_printf("nextEntry7: %u\r\n", nextBufEntry);
	}
	else
		assert(!"[WARNING] SelectiveGetFromDsmRangeHashList [WARNING]");

	PutDSMBuftoLRUList(bufEntry);
}

void PutDSMBuftoLRUList(unsigned int bufEntry)
{
	if(dsmRangeLruList.tailEntry != DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[bufEntry].prevEntry = dsmRangeLruList.tailEntry;
		dsmRangePtr->dsmRange[bufEntry].nextEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[dsmRangeLruList.tailEntry].nextEntry = bufEntry;
		dsmRangeLruList.tailEntry = bufEntry;
	}
	else
	{
		dsmRangePtr->dsmRange[bufEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[bufEntry].nextEntry = DATA_BUF_NONE;
		dsmRangeLruList.headEntry = bufEntry;
		dsmRangeLruList.tailEntry = bufEntry;
	}
}

unsigned int SmallestDSMBuftoLRUList()
{
	unsigned int smallest, smallestEntry, tmpEntry;
	for (int i = 0; i < 21; i++)
		if(dsmRangeHashTable->Range_Flag[i] == 1)
		{
			tmpEntry = dsmRangeHashTable->dsmRangeHash[i].headEntry;
			smallest = dsmRangePtr->dsmRange[tmpEntry].lengthInLogicalBlocks;
			smallestEntry = tmpEntry;
			while(tmpEntry != DATA_BUF_NONE)
			{
				if (smallest > dsmRangePtr->dsmRange[tmpEntry].lengthInLogicalBlocks)
				{
					smallest = dsmRangePtr->dsmRange[tmpEntry].lengthInLogicalBlocks;
					smallestEntry = tmpEntry;
					tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;
				}
				else
					tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;

			}
//			xil_printf("SmallestDSMBuftoLRUList SelectiveGetFromDsmRangeHashList\r\n");
			SelectiveGetFromDsmRangeHashList(smallestEntry);
			return smallestEntry;
		}
	return DATA_BUF_NONE;
}

unsigned int AllocateDSMBuf()
{
	unsigned int evictedEntry = dsmRangeLruList.tailEntry;

	if(evictedEntry == DATA_BUF_NONE)
		evictedEntry = SmallestDSMBuftoLRUList();

	if (evictedEntry == DATA_BUF_NONE)
		assert(!"[WARNING] There is no valid DSM buffer entry [WARNING]");

	if(dsmRangePtr->dsmRange[evictedEntry].prevEntry != DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[dsmRangePtr->dsmRange[evictedEntry].prevEntry].nextEntry = DATA_BUF_NONE;
		dsmRangeLruList.tailEntry = dsmRangePtr->dsmRange[evictedEntry].prevEntry;

		dsmRangePtr->dsmRange[evictedEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[evictedEntry].nextEntry = DATA_BUF_NONE;

	}
	else
	{
		dsmRangePtr->dsmRange[evictedEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[evictedEntry].nextEntry = DATA_BUF_NONE;
		dsmRangeLruList.headEntry = DATA_BUF_NONE;
		dsmRangeLruList.tailEntry = DATA_BUF_NONE;
	}

	return evictedEntry;
}

void UpdateDataBufEntryInfoBlockingReq(unsigned int bufEntry, unsigned int reqSlotTag)
{
	if(dataBufMapPtr->dataBuf[bufEntry].blockingReqTail != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = dataBufMapPtr->dataBuf[bufEntry].blockingReqTail;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq].nextBlockingReq  = reqSlotTag;
	}

	dataBufMapPtr->dataBuf[bufEntry].blockingReqTail = reqSlotTag;
}


unsigned int AllocateTempDataBuf(unsigned int dieNo)
{
	return dieNo;
}


void UpdateTempDataBufEntryInfoBlockingReq(unsigned int bufEntry, unsigned int reqSlotTag)
{

	if(tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq].nextBlockingReq  = reqSlotTag;
	}

	tempDataBufMapPtr->tempDataBuf[bufEntry].blockingReqTail = reqSlotTag;
}

void PutToDataBufHashList(unsigned int bufEntry)
{
	unsigned int hashEntry;

	hashEntry = FindDataBufHashTableEntry(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr);

	if(dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry != DATA_BUF_NONE)
	{
		dataBufMapPtr->dataBuf[bufEntry].hashPrevEntry = dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry ;
		dataBufMapPtr->dataBuf[bufEntry].hashNextEntry = REQ_SLOT_TAG_NONE;
		dataBufMapPtr->dataBuf[dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry].hashNextEntry = bufEntry;
		dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry = bufEntry;
	}
	else
	{
		dataBufMapPtr->dataBuf[bufEntry].hashPrevEntry = REQ_SLOT_TAG_NONE;
		dataBufMapPtr->dataBuf[bufEntry].hashNextEntry = REQ_SLOT_TAG_NONE;
		dataBufHashTablePtr->dataBufHash[hashEntry].headEntry = bufEntry;
		dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry = bufEntry;
	}
}


void SelectiveGetFromDataBufHashList(unsigned int bufEntry)
{
	if(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr != LSA_NONE)
	{
		unsigned int prevBufEntry, nextBufEntry, hashEntry;

		prevBufEntry =  dataBufMapPtr->dataBuf[bufEntry].hashPrevEntry;
		nextBufEntry =  dataBufMapPtr->dataBuf[bufEntry].hashNextEntry;
		hashEntry = FindDataBufHashTableEntry(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr);

		if((nextBufEntry != DATA_BUF_NONE) && (prevBufEntry != DATA_BUF_NONE))
		{
			dataBufMapPtr->dataBuf[prevBufEntry].hashNextEntry = nextBufEntry;
			dataBufMapPtr->dataBuf[nextBufEntry].hashPrevEntry = prevBufEntry;
		}
		else if((nextBufEntry == DATA_BUF_NONE) && (prevBufEntry != DATA_BUF_NONE))
		{
			dataBufMapPtr->dataBuf[prevBufEntry].hashNextEntry = DATA_BUF_NONE;
			dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry = prevBufEntry;
		}
		else if((nextBufEntry != DATA_BUF_NONE) && (prevBufEntry == DATA_BUF_NONE))
		{
			dataBufMapPtr->dataBuf[nextBufEntry].hashPrevEntry = DATA_BUF_NONE;
			dataBufHashTablePtr->dataBufHash[hashEntry].headEntry = nextBufEntry;
		}
		else
		{
			dataBufHashTablePtr->dataBufHash[hashEntry].headEntry = DATA_BUF_NONE;
			dataBufHashTablePtr->dataBufHash[hashEntry].tailEntry = DATA_BUF_NONE;
		}
	}
}

unsigned int FindDsmRangeHashTableEntry(unsigned int length)
{
	if ((1 <= length) && (length < 2))
		return 0;
	else if ((2 <= length) && (length < 4))
		return 1;
	else if ((4 <= length) && (length < 8))
		return 2;
	else if ((8 <= length) && (length < 16))
		return 3;
	else if ((16 <= length) && (length < 32))
		return 4;
	else if ((32 <= length) && (length < 64))
		return 5;
	else if ((64 <= length) && (length < 128))
		return 6;
	else if ((128 <= length) && (length < 256))
		return 7;
	else if ((256 <= length) && (length < 512))
		return 8;
	else if ((512 <= length) && (length < 1024))
		return 9;
	else if ((1024 <= length) && (length < 2048))
		return 10;
	else if ((2048 <= length) && (length < 4096))
		return 11;
	else if ((4096 <= length) && (length < 8192))
		return 12;
	else if ((8192 <= length) && (length < 16384))
		return 13;
	else if ((16384 <= length) && (length < 32768))
		return 14;
	else if ((32768 <= length) && (length < 65536))
		return 15;
	else if ((65536 <= length) && (length < 131072))
		return 16;
	else if ((131072 <= length) && (length < 262144))
		return 17;
	else if ((262144 <= length) && (length < 524288))
		return 18;
	else if ((524288 <= length) && (length < 1048576))
		return 19;
	else
		return 20;
}

