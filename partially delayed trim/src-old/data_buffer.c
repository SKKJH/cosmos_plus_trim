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

P_REG_BUF_MAP regressionBufMapPtr;

void InitDataBuf()
{
	int bufEntry;

	dataBufMapPtr = (P_DATA_BUF_MAP) DATA_BUFFER_MAP_ADDR;
	dataBufHashTablePtr = (P_DATA_BUF_HASH_TABLE)DATA_BUFFFER_HASH_TABLE_ADDR;
	tempDataBufMapPtr = (P_TEMPORARY_DATA_BUF_MAP)TEMPORARY_DATA_BUFFER_MAP_ADDR;

	asyncTrimBitMapPtr = (P_ASYNC_TRIM_BIT_MAP)ASYNC_TRIM_BIT_MAP_ADDR;
	dsmRangePtr = (P_DSM_RANGE)DSM_RANGE_ADDR;
	dsmRangeHashTable = (P_DSM_RANGE_HASH_TABLE)DSM_RANGE_HASH_TABLE_ADDR;

	regressionBufMapPtr = (P_REG_BUF_MAP)TEMPORARY_REG_BUFFER_MAP_ADDR;

	for (int i = 0; i < AVAILABLE_DSM_RANGE_ENTRY_COUNT; i++)
	{
		dsmRangePtr->dsmRange[i].lengthInLogicalBlocks = 0;
		dsmRangePtr->dsmRange[i].RealLB = 0;
		dsmRangePtr->dsmRange[i].startingLBA[0] = 0xffffffff;
		dsmRangePtr->dsmRange[i].startingLBA[1] = 0xffffffff;
		dsmRangePtr->dsmRange[i].ContextAttributes.value = 0;

		dsmRangePtr->dsmRange[i].prevEntry = i-1;
		dsmRangePtr->dsmRange[i].nextEntry = i+1;

		dsmRangePtr->dsmRange[i].hashPrevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[i].hashNextEntry = DATA_BUF_NONE;
	}

	for (int i = 0; i < 33; i++)
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
		asyncTrimBitMapPtr->writeBitMap[i] = 0ULL;

	for(bufEntry = 0; bufEntry < AVAILABLE_DATA_BUFFER_ENTRY_COUNT; bufEntry++)
	{
		dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr = LSA_NONE;
		dataBufMapPtr->dataBuf[bufEntry].prevEntry = bufEntry-1;
		dataBufMapPtr->dataBuf[bufEntry].nextEntry = bufEntry+1;
		dataBufMapPtr->dataBuf[bufEntry].dirty = DATA_BUF_CLEAN;
		dataBufMapPtr->dataBuf[bufEntry].blockingReqTail =  REQ_SLOT_TAG_NONE;

		dataBufMapPtr->dataBuf[bufEntry].blk0 = 0;
		dataBufMapPtr->dataBuf[bufEntry].blk1 = 0;
		dataBufMapPtr->dataBuf[bufEntry].blk2 = 0;
		dataBufMapPtr->dataBuf[bufEntry].blk3 = 0;

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

	regressionBufMapPtr->head = 0;
	regressionBufMapPtr->tail = 0;
	for (int i = 0; i < MAX_SAMPLES; i++) {
	    regressionBufMapPtr->regressionEnrty[i].util = 0;
	    regressionBufMapPtr->regressionEnrty[i].valid_page = 0;
	}
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

unsigned int AllocateDataBuf()
{
	unsigned int evictedEntry = dataBufLruList.tailEntry;

	if(evictedEntry == DATA_BUF_NONE)
		assert(!"[WARNING] There is no valid buffer entry [WARNING]");

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

unsigned int CheckDataBufHitbyLSA(unsigned int logicalSliceAddr)
{
	unsigned int bufEntry;

	bufEntry = dataBufHashTablePtr->dataBufHash[FindDataBufHashTableEntry(logicalSliceAddr)].headEntry;

	while(bufEntry != DATA_BUF_NONE)
	{
		if(dataBufMapPtr->dataBuf[bufEntry].logicalSliceAddr == logicalSliceAddr)
			return bufEntry;
		else
			bufEntry = dataBufMapPtr->dataBuf[bufEntry].hashNextEntry;
	}

	return DATA_BUF_FAIL;
}

//unsigned int FindDsmRangeHashTableEntry(unsigned int length)
//{
//	if ((1 <= length) && (length < 2))
//		return 0;
//	else if ((2 <= length) && (length < 4))
//		return 1;
//	else if ((4 <= length) && (length < 8))
//		return 2;
//	else if ((8 <= length) && (length < 16))
//		return 3;
//	else if ((16 <= length) && (length < 32))
//		return 4;
//	else if ((32 <= length) && (length < 64))
//		return 5;
//	else if ((64 <= length) && (length < 128))
//		return 6;
//	else if ((128 <= length) && (length < 256))
//		return 7;
//	else if ((256 <= length) && (length < 512))
//		return 8;
//	else if ((512 <= length) && (length < 1024))
//		return 9;
//	else if ((1024 <= length) && (length < 2048))
//		return 10;
//	else if ((2048 <= length) && (length < 4096))
//		return 11;
//	else if ((4096 <= length) && (length < 8192))
//		return 12;
//	else if ((8192 <= length) && (length < 16384))
//		return 13;
//	else if ((16384 <= length) && (length < 32768))
//		return 14;
//	else if ((32768 <= length) && (length < 65536))
//		return 15;
//	else if ((65536 <= length) && (length < 131072))
//		return 16;
//	else if ((131072 <= length) && (length < 262144))
//		return 17;
//	else if ((262144 <= length) && (length < 524288))
//		return 18;
//	else if ((524288 <= length) && (length < 1048576))
//		return 19;
//	else
//		return 20;
//}

unsigned int FindDsmRangeHashTableEntry(unsigned int length)
{
    if (length < 8192)
        return 0;
    else if (length < 16384)
        return 1;
    else if (length < 24576)
        return 2;
    else if (length < 32768)
        return 3;
    else if (length < 40960)
        return 4;
    else if (length < 49152)
        return 5;
    else if (length < 57344)
        return 6;
    else if (length < 65536)
        return 7;
    else if (length < 73728)
        return 8;
    else if (length < 81920)
        return 9;
    else if (length < 90112)
        return 10;
    else if (length < 98304)
        return 11;
    else if (length < 106496)
        return 12;
    else if (length < 114688)
        return 13;
    else if (length < 122880)
        return 14;
    else if (length < 131072)
        return 15;
    else if (length < 139264)
        return 16;
    else if (length < 147456)
        return 17;
    else if (length < 155648)
        return 18;
    else if (length < 163840)
        return 19;
    else if (length < 172032)
        return 20;
    else if (length < 180224)
        return 21;
    else if (length < 188416)
        return 22;
    else if (length < 196608)
        return 23;
    else if (length < 204800)
        return 24;
    else if (length < 212992)
        return 25;
    else if (length < 221184)
        return 26;
    else if (length < 229376)
        return 27;
    else if (length < 237568)
        return 28;
    else if (length < 245760)
        return 29;
    else if (length < 253952)
        return 30;
    else if (length < 262144)
        return 31;
    else
    	return 32;
}


void PutToDsmRangeHashList(unsigned int bufEntry)
{
	unsigned int hashEntry, newLength, currentEntry, prevEntry;

	newLength = dsmRangePtr->dsmRange[bufEntry].RealLB;

	if (newLength == 0)
		return;

	hashEntry = FindDsmRangeHashTableEntry(newLength);

	dsmRangeHashTable->Range_Flag[hashEntry] = 1;
	currentEntry = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;
	prevEntry = DATA_BUF_NONE;

	while (currentEntry != DATA_BUF_NONE)
	{
		if (dsmRangePtr->dsmRange[currentEntry].RealLB <= newLength)
			break;
		prevEntry = currentEntry;
		currentEntry = dsmRangePtr->dsmRange[currentEntry].hashNextEntry;
	}

	dsmRangePtr->dsmRange[bufEntry].hashPrevEntry = DATA_BUF_NONE;
	dsmRangePtr->dsmRange[bufEntry].hashNextEntry = DATA_BUF_NONE;

	if (prevEntry == DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[bufEntry].hashNextEntry = currentEntry;
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
		dsmRangePtr->dsmRange[prevEntry].hashNextEntry = bufEntry;

		if (currentEntry != DATA_BUF_NONE)
			dsmRangePtr->dsmRange[currentEntry].hashPrevEntry = bufEntry;
		else
			dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry = bufEntry;
	}
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

void SelectiveGetFromDsmRangeHashList(unsigned int bufEntry)
{
	unsigned int prevBufEntry, nextBufEntry, hashEntry;
	if (bufEntry == DATA_BUF_NONE)
	{
		xil_printf("Wrong BufEntry for SelectiveGet\r\n");
		return;
	}

	prevBufEntry =  dsmRangePtr->dsmRange[bufEntry].hashPrevEntry;
	nextBufEntry =  dsmRangePtr->dsmRange[bufEntry].hashNextEntry;

	hashEntry = FindDsmRangeHashTableEntry(dsmRangePtr->dsmRange[bufEntry].RealLB);

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

	PutDSMBuftoLRUList(bufEntry);
}

unsigned int SmallestDSMBuftoLRUList()
{
	unsigned int smallest, smallestEntry, tmpEntry;
	for (int i = 0; i < 21; i++)
	{
		tmpEntry = dsmRangeHashTable->dsmRangeHash[i].headEntry;
		smallest = dsmRangePtr->dsmRange[tmpEntry].RealLB;
		smallestEntry = tmpEntry;
		while(tmpEntry != DATA_BUF_NONE)
		{
			if (smallest > dsmRangePtr->dsmRange[tmpEntry].RealLB)
			{
				smallest = dsmRangePtr->dsmRange[tmpEntry].RealLB;
				smallestEntry = tmpEntry;
				tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;
			}
			else
				tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;
		}
	}
	SelectiveGetFromDsmRangeHashList(smallestEntry);
	return smallestEntry;
}

unsigned int AllocateDSMBuf()
{
	unsigned int evictedEntry = dsmRangeLruList.tailEntry;

	if(evictedEntry == DATA_BUF_NONE)
		evictedEntry = SmallestDSMBuftoLRUList();

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

void TRIM (unsigned int lba, unsigned int blk0, unsigned int blk1, unsigned int blk2, unsigned int blk3)
{
	unsigned int lsa, bufEntry;
	lsa = lba/4;

	if (asyncTrimBitMapPtr->writeBitMap[lsa/64] & (1ULL << (lsa % 64)))
		return;

//	xil_printf("LSA %d will be checked\r\n",lsa);
//	if ((blk0 == 0)&&(blk1 == 0)&&(blk2 == 0)&&(blk3 == 0))
//		xil_printf("LSA %d will be trimmed\r\n", lsa);

	bufEntry = CheckDataBufHitbyLSA(lsa);
	if (bufEntry != DATA_BUF_FAIL)
	{
//    	xil_printf("This Buffer will be cleaned: %d!!\r\n", bufEntry);
        if (blk0 == 0)
        {
            dataBufMapPtr->dataBuf[bufEntry].blk0 = 0;
        }
        if (blk1 == 0)
        {
            dataBufMapPtr->dataBuf[bufEntry].blk1 = 0;
        }
        if (blk2 == 0)
        {
            dataBufMapPtr->dataBuf[bufEntry].blk2 = 0;
        }
        if (blk3 == 0)
        {
            dataBufMapPtr->dataBuf[bufEntry].blk3 = 0;
        }
        if ((dataBufMapPtr->dataBuf[bufEntry].blk0 == 0) &&
            (dataBufMapPtr->dataBuf[bufEntry].blk1 == 0) &&
            (dataBufMapPtr->dataBuf[bufEntry].blk2 == 0) &&
            (dataBufMapPtr->dataBuf[bufEntry].blk3 == 0))
        {
//        	xil_printf("This Buffer cleaned: %d!!\r\n", bufEntry);
            unsigned int prevBufEntry, nextBufEntry;
            prevBufEntry = dataBufMapPtr->dataBuf[bufEntry].prevEntry;
            nextBufEntry = dataBufMapPtr->dataBuf[bufEntry].nextEntry;

            if (prevBufEntry != DATA_BUF_NONE && nextBufEntry != DATA_BUF_NONE) {
                dataBufMapPtr->dataBuf[prevBufEntry].nextEntry = nextBufEntry;
                dataBufMapPtr->dataBuf[nextBufEntry].prevEntry = prevBufEntry;
                nextBufEntry = DATA_BUF_NONE;
                prevBufEntry = dataBufLruList.tailEntry;
                dataBufMapPtr->dataBuf[dataBufLruList.tailEntry].nextEntry = bufEntry;
                dataBufLruList.tailEntry = bufEntry;
            } else if (prevBufEntry != DATA_BUF_NONE && nextBufEntry == DATA_BUF_NONE) {
                dataBufLruList.tailEntry = bufEntry;
            } else if (prevBufEntry == DATA_BUF_NONE && nextBufEntry != DATA_BUF_NONE) {
                dataBufMapPtr->dataBuf[nextBufEntry].prevEntry = DATA_BUF_NONE;
                dataBufLruList.headEntry = nextBufEntry;
                prevBufEntry = dataBufLruList.tailEntry;
                dataBufMapPtr->dataBuf[dataBufLruList.tailEntry].nextEntry = bufEntry;
                dataBufLruList.tailEntry = bufEntry;
            } else {
                prevBufEntry = DATA_BUF_NONE;
                nextBufEntry = DATA_BUF_NONE;
                dataBufLruList.headEntry = bufEntry;
                dataBufLruList.tailEntry = bufEntry;
            }
            SelectiveGetFromDataBufHashList(bufEntry);
            dataBufMapPtr->dataBuf[bufEntry].blockingReqTail = REQ_SLOT_TAG_NONE;
            dataBufMapPtr->dataBuf[bufEntry].dirty = DATA_BUF_CLEAN;
            dataBufMapPtr->dataBuf[bufEntry].reserved0 = 0;
        }
	}
	unsigned int virtualSliceAddr = logicalSliceMapPtr->logicalSlice[lsa].virtualSliceAddr;
	if (virtualSliceAddr != VSA_NONE) {
//    	xil_printf("This LSA will be cleaned: %d!!\r\n", lsa);
		if (blk0 == 0) {
			logicalSliceMapPtr->logicalSlice[lsa].blk0 = 0;
		}
		if (blk1 == 0) {
			logicalSliceMapPtr->logicalSlice[lsa].blk1 = 0;
		}
		if (blk2 == 0) {
			logicalSliceMapPtr->logicalSlice[lsa].blk2 = 0;
		}
		if (blk3 == 0) {
			logicalSliceMapPtr->logicalSlice[lsa].blk3 = 0;
		}
		if ((logicalSliceMapPtr->logicalSlice[lsa].blk0 == 0) &&
			(logicalSliceMapPtr->logicalSlice[lsa].blk1 == 0) &&
			(logicalSliceMapPtr->logicalSlice[lsa].blk2 == 0) &&
	        (logicalSliceMapPtr->logicalSlice[lsa].blk3 == 0))
		{
//        	xil_printf("This LSA cleaned: %d!!\r\n", lsa);
			InvalidateOldVsa(lsa);
		}
	}
}

unsigned int cmp(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void add_sample(unsigned int util, unsigned int valid_page) {
    // 다음에 쓸 위치 = tail
    unsigned int index = regressionBufMapPtr->tail;

    // 이전 util 값 제거 (버퍼가 가득 찼을 경우에만)
    if ((regressionBufMapPtr->tail + 1) % MAX_SAMPLES == regressionBufMapPtr->head) {
        int old_util = regressionBufMapPtr->regressionEnrty[regressionBufMapPtr->head].util;
        util_count[old_util]--;  // 오래된 값 제거
        regressionBufMapPtr->head = (regressionBufMapPtr->head + 1) % MAX_SAMPLES;  // head 이동
    }

    // 새 값 삽입
    regressionBufMapPtr->regressionEnrty[index].util = util;
    regressionBufMapPtr->regressionEnrty[index].valid_page = valid_page;

    regressionBufMapPtr->tail = (regressionBufMapPtr->tail + 1) % MAX_SAMPLES;  // tail 이동
    util_count[util]++;
}

unsigned int get_sample_count() {
    if (regressionBufMapPtr->tail >= regressionBufMapPtr->head)
        return regressionBufMapPtr->tail - regressionBufMapPtr->head;
    else
        return MAX_SAMPLES;
}

void train_model() {
    int count = get_sample_count();
    int sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    int idx = regressionBufMapPtr->head;
    int util_first = regressionBufMapPtr->regressionEnrty[idx].util;
    int util_all_same = 1;
    int total = 0;

    for (int i = 0; i < count; i++) {
        int x = regressionBufMapPtr->regressionEnrty[idx].util;
        int y = regressionBufMapPtr->regressionEnrty[idx].valid_page;

        if (x != util_first)
            util_all_same = 0;

        sum_x += x;
        sum_y += y;
        sum_xx += x * x;
        sum_xy += x * y;

        total += y;
        idx = (idx + 1) % MAX_SAMPLES;
    }

    if (util_all_same) {
        reg_model.a_fixed = 0;
        reg_model.b_fixed = 0;
        reg_model.fallback_value = total / count;
        reg_model.valid = 0;
        fallback_cnt++;
        return;
    }

    int n = count;
    int denom = n * sum_xx - sum_x * sum_x;

    reg_model.a_fixed = (int)(((n * sum_xy - sum_x * sum_y) * 1000) / denom);
    reg_model.b_fixed = (int)((sum_y * 1000 - (int)(reg_model.a_fixed) * sum_x) / n);
    reg_model.fallback_value = 0;
    reg_model.valid = 1;
    train_cnt++;
}

unsigned int predict_valid_page(int util) {
    if (!reg_model.valid) {
//        xil_printf(" 회귀 불가 또는 데이터 없음 → fallback 사용: valid_page = %d\n",
//                   reg_model.fallback_value);
    	return_fb++;
        return reg_model.fallback_value;
    }

    int predicted = (reg_model.a_fixed * util + reg_model.b_fixed) / 1000;

    int a_int = reg_model.a_fixed / 1000;
    int a_frac = reg_model.a_fixed % 1000;
    if (a_frac < 0) a_frac = -a_frac;

    int b_int = reg_model.b_fixed / 1000;
    int b_frac = reg_model.b_fixed % 1000;
    if (b_frac < 0) b_frac = -b_frac;

//    xil_printf(" 회귀 적용: y = (%d.%02d)x + (%d.%02d) → 예측값: %d\n",
//               a_int, a_frac, b_int, b_frac, predicted);
    return_rg++;
    return predicted;
}
