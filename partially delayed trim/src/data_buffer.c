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
//DSM_RANGE_LRU_LIST dsmRangeLruList;
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
		dsmRangePtr->dsmRange[i].flag = 0;
	}

	for (int i = 0; i < 33; i++)
	{
		dsmRangeHashTable->dsmRangeHash[i].headEntry = DATA_BUF_NONE;
		dsmRangeHashTable->dsmRangeHash[i].tailEntry = DATA_BUF_NONE;
		dsmRangeHashTable->Range_Flag[i] = 0;
	}

	dsmRangePtr->dsmRange[0].prevEntry = DATA_BUF_NONE;
	dsmRangePtr->dsmRange[AVAILABLE_DSM_RANGE_ENTRY_COUNT - 1].nextEntry = DATA_BUF_NONE;

	dsmRangePtr->tailEntry = AVAILABLE_DSM_RANGE_ENTRY_COUNT - 1;

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

unsigned int FindDsmRangeHashTableEntry(unsigned int length)
{
    unsigned int size_in_mb = (length * 4) / 1024;  // KB -> MB

    if (size_in_mb <= 1)
        return 0;
    else if (size_in_mb >= 1024)
        return 32;

    unsigned int index = ((size_in_mb - 1) * 31) / (1024 - 1) + 1;

    return index;
}

void DebugCheckHashListOrder(unsigned int hashEntry) {
    unsigned int prevRealLB = 0xFFFFFFFF;
    unsigned int entry = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;

    xil_printf("Hash List [%u]: ", hashEntry);
    while (entry != DATA_BUF_NONE) {
        unsigned int realLB = dsmRangePtr->dsmRange[entry].RealLB;
        xil_printf("%u -> ", realLB);

        if (realLB > prevRealLB) {
            xil_printf("\n[ERROR] List not sorted! prev: %u, current: %u\n", prevRealLB, realLB);
        }

        prevRealLB = realLB;
        entry = dsmRangePtr->dsmRange[entry].hashNextEntry;
    }
    xil_printf("NULL\n");
}

void DebugCheckBidirectionalLink(unsigned int hashEntry) {
    unsigned int entry = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;

    while (entry != DATA_BUF_NONE) {
        unsigned int next = dsmRangePtr->dsmRange[entry].hashNextEntry;
        if (next != DATA_BUF_NONE) {
            unsigned int back = dsmRangePtr->dsmRange[next].hashPrevEntry;
            if (back != entry) {
                xil_printf("[ERROR] Backward link mismatch: %u -> %u but back != %u\n", entry, next, entry);
            }
        }
        entry = next;
    }
}

void DebugCheckHeadTail(unsigned int hashEntry) {
    unsigned int head = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;
    unsigned int tail = dsmRangeHashTable->dsmRangeHash[hashEntry].tailEntry;

    // tail에서 역방향으로 탐색해서 head까지 가야 함
    unsigned int entry = tail;
    while (entry != DATA_BUF_NONE) {
        if (entry == head) {
            xil_printf("Head/Tail linkage OK for hash %u\n", hashEntry);
            return;
        }
        entry = dsmRangePtr->dsmRange[entry].hashPrevEntry;
    }
    xil_printf("[ERROR] Tail to Head traversal failed for hash %u\n", hashEntry);
}

void VerifyAllHashLists() {
    for (int i = 0; i < 33; i++) {
        DebugCheckHashListOrder(i);
        DebugCheckBidirectionalLink(i);
        DebugCheckHeadTail(i);
    }
}

void PutToDsmRangeHashList(unsigned int bufEntry)
{
	unsigned int hashEntry, newLength, currentEntry, prevEntry;

	newLength = dsmRangePtr->dsmRange[bufEntry].RealLB;
	hashEntry = FindDsmRangeHashTableEntry(newLength);
	dsmRangeHashTable->Range_Flag[hashEntry] = 1;
	currentEntry = dsmRangeHashTable->dsmRangeHash[hashEntry].headEntry;
	prevEntry = DATA_BUF_NONE;
//	while (currentEntry != DATA_BUF_NONE)
//	{
//		if (dsmRangePtr->dsmRange[currentEntry].RealLB <= newLength) {
//			break;
//		}
//		prevEntry = currentEntry;
//		currentEntry = dsmRangePtr->dsmRange[currentEntry].hashNextEntry;
//	}
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
	if(dsmRangePtr->tailEntry != DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[bufEntry].prevEntry = dsmRangePtr->tailEntry;
		dsmRangePtr->dsmRange[bufEntry].nextEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[dsmRangePtr->tailEntry].nextEntry = bufEntry;
		dsmRangePtr->tailEntry = bufEntry;
	}
	else
	{
		dsmRangePtr->dsmRange[bufEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[bufEntry].nextEntry = DATA_BUF_NONE;
		dsmRangePtr->tailEntry = bufEntry;
	}
}

void SelectiveGetFromDsmRangeHashList(unsigned int bufEntry)
{
	unsigned int prevBufEntry, nextBufEntry, hashEntry;
	if (bufEntry == DATA_BUF_NONE)
	{
	    assert(!"[WARNING] Wrong BufEntry for SelectiveGet [WARNING]");
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
    for (int i = 0; i < 21; i++)
    {
        unsigned int tmpEntry = dsmRangeHashTable->dsmRangeHash[i].tailEntry;
        if (tmpEntry == DATA_BUF_NONE)
            continue;
        SelectiveGetFromDsmRangeHashList(tmpEntry);
        return tmpEntry;
//        unsigned int smallestEntry = tmpEntry;
//        unsigned int smallest = dsmRangePtr->dsmRange[tmpEntry].RealLB;
//
//        tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;
//        while (tmpEntry != DATA_BUF_NONE)
//        {
//            unsigned int realLB = dsmRangePtr->dsmRange[tmpEntry].RealLB;
//            if (realLB < smallest)
//            {
//                smallest = realLB;
//                smallestEntry = tmpEntry;
//            }
//            tmpEntry = dsmRangePtr->dsmRange[tmpEntry].hashNextEntry;
//        }
//        xil_printf("By SmallestDSMBuftoLRUList\r\n");
//        SelectiveGetFromDsmRangeHashList(smallestEntry);
//        return smallestEntry;
    }
    assert(!"[WARNING] No valid DSM buffer entry found [WARNING]");
    return DATA_BUF_NONE;
}



unsigned int AllocateDSMBuf()
{
	unsigned int allocEntry = dsmRangePtr->tailEntry;

	if(allocEntry == DATA_BUF_NONE) {
		allocate_full_cnt += 1;
		allocEntry = SmallestDSMBuftoLRUList();
	}

	if(dsmRangePtr->dsmRange[allocEntry].prevEntry != DATA_BUF_NONE)
	{
		dsmRangePtr->dsmRange[dsmRangePtr->dsmRange[allocEntry].prevEntry].nextEntry = DATA_BUF_NONE;
		dsmRangePtr->tailEntry = dsmRangePtr->dsmRange[allocEntry].prevEntry;
		dsmRangePtr->dsmRange[allocEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[allocEntry].nextEntry = DATA_BUF_NONE;
	}
	else
	{
		dsmRangePtr->dsmRange[allocEntry].prevEntry = DATA_BUF_NONE;
		dsmRangePtr->dsmRange[allocEntry].nextEntry = DATA_BUF_NONE;
		dsmRangePtr->tailEntry = DATA_BUF_NONE;
	}
	return allocEntry;
}

void TRIM(unsigned int lba, unsigned int blk0, unsigned int blk1, unsigned int blk2, unsigned int blk3, unsigned int check)
{
	XTime tStart, tEnd;
	XTime tTime;
	unsigned int xtime_hi, xtime_lo, map, buff;

	unsigned int lsa = lba / 4;
	map = 0;
	buff = 0;

    if (check && (asyncTrimBitMapPtr->writeBitMap[lsa / 64] & (1ULL << (lsa % 64)))) {
        trim_invalid++;
        return;
    }

    XTime_GetTime(&tStart);
    unsigned int bufEntry = CheckDataBufHitbyLSA(lsa);
    XTime_GetTime(&tEnd);
    tTime = (tEnd - tStart);
    xtime_hi = (unsigned int)(tTime >> 32);
    xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);

    if (bufEntry != DATA_BUF_FAIL) {
        DATA_BUF_ENTRY *buf = &dataBufMapPtr->dataBuf[bufEntry];

        buf->blk0 = blk0 ? buf->blk0 : 0;
        buf->blk1 = blk1 ? buf->blk1 : 0;
        buf->blk2 = blk2 ? buf->blk2 : 0;
        buf->blk3 = blk3 ? buf->blk3 : 0;

        // 전체 blk가 0인지 빠른 검사
        if (!(buf->blk0 | buf->blk1 | buf->blk2 | buf->blk3)) {
            // LRU 리스트에서 현재 bufEntry 제거 후 tail로 이동
//            unsigned int prev = buf->prevEntry;
//            unsigned int next = buf->nextEntry;
//
//            if (prev != DATA_BUF_NONE)
//                dataBufMapPtr->dataBuf[prev].nextEntry = next;
//            else
//                dataBufLruList.headEntry = next;
//
//            if (next != DATA_BUF_NONE)
//                dataBufMapPtr->dataBuf[next].prevEntry = prev;
//            else
//                dataBufLruList.tailEntry = prev;
//
//            // tail로 이동
//            if (dataBufLruList.tailEntry != bufEntry) {
//                if (dataBufLruList.tailEntry != DATA_BUF_NONE)
//                    dataBufMapPtr->dataBuf[dataBufLruList.tailEntry].nextEntry = bufEntry;
//                buf->prevEntry = dataBufLruList.tailEntry;
//                buf->nextEntry = DATA_BUF_NONE;
//                dataBufLruList.tailEntry = bufEntry;
//
//                if (dataBufLruList.headEntry == DATA_BUF_NONE)
//                    dataBufLruList.headEntry = bufEntry;
//            }
//
//            // 상태값 클리어
//            SelectiveGetFromDataBufHashList(bufEntry);
//            buf->blockingReqTail = REQ_SLOT_TAG_NONE;
            buf->dirty = DATA_BUF_CLEAN;
        }
    }
    LOGICAL_SLICE_ENTRY *slice = &logicalSliceMapPtr->logicalSlice[lsa];
    if (slice->virtualSliceAddr != VSA_NONE) {
        slice->blk0 = blk0 ? slice->blk0 : 0;
        slice->blk1 = blk1 ? slice->blk1 : 0;
        slice->blk2 = blk2 ? slice->blk2 : 0;
        slice->blk3 = blk3 ? slice->blk3 : 0;

        if (!(slice->blk0 | slice->blk1 | slice->blk2 | slice->blk3)) {
            realtrimmedRange += 4;
            InvalidateOldVsa(lsa);
        }
    }
}

unsigned int cmp(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void add_sample(unsigned int util, unsigned int valid_page) {

//    xil_printf("ADD Util: %d, ", util);
//    xil_printf("ADD Valid Page: %d\r\n", valid_page);
    if (regressionBufMapPtr->head != regressionBufMapPtr->tail) {
        unsigned int last_index = (regressionBufMapPtr->tail + MAX_SAMPLES - 1) % MAX_SAMPLES;
        unsigned int last_valid = regressionBufMapPtr->regressionEnrty[last_index].valid_page;

        if (valid_page == 0 || (last_valid >= 10 && valid_page < last_valid / 2)) {
            return;
        }
    }

    unsigned int index = regressionBufMapPtr->tail;
    if ((regressionBufMapPtr->tail + 1) % MAX_SAMPLES == regressionBufMapPtr->head) {
        regressionBufMapPtr->head = (regressionBufMapPtr->head + 1) % MAX_SAMPLES;
    }
    regressionBufMapPtr->regressionEnrty[index].util = util;
    regressionBufMapPtr->regressionEnrty[index].valid_page = valid_page;
    regressionBufMapPtr->tail = (regressionBufMapPtr->tail + 1) % MAX_SAMPLES;
}

void empty_sample() {
    regressionBufMapPtr->head = 0;
    regressionBufMapPtr->tail = 0;
}

unsigned int get_sample_count() {
    if (regressionBufMapPtr->tail >= regressionBufMapPtr->head)
        return regressionBufMapPtr->tail - regressionBufMapPtr->head;
    else
        return MAX_SAMPLES - regressionBufMapPtr->head + regressionBufMapPtr->tail;
}

void train_model() {
    int count = get_sample_count();
    int sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    int idx = regressionBufMapPtr->head;
    int util_first = regressionBufMapPtr->regressionEnrty[idx].util;
    int util_all_same = 1;
    int total = 0;

    for (int i = 0; i < count; i++) {
        int x = regressionBufMapPtr->regressionEnrty[idx].util;         // 0~100
        int y = regressionBufMapPtr->regressionEnrty[idx].valid_page;   // 0~128
//        xil_printf("Train Util: %d, ", x);
//        xil_printf("Train Valid Page: %d\r\n", y);

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
        reg_model.fallback_value = (total + count / 2) / count; // 반올림 처리
        reg_model.valid = 0;
        fallback_cnt++;
        return;
    }

    int n = count;
    int denom = n * sum_xx - sum_x * sum_x;

    // denom이 0인 경우 추가 처리
    if (denom == 0) {
        reg_model.a_fixed = 0;
        reg_model.b_fixed = 0;
        reg_model.fallback_value = (total + count / 2) / count;
        reg_model.valid = 0;
        fallback_cnt++;
        return;
    }

    // 더 정확한 실수 연산 후 반올림하여 고정 소수점으로 변환
    reg_model.a_fixed = (int)((((double)(n * sum_xy - sum_x * sum_y)) * 1000.0 / denom) + 0.5);
    reg_model.b_fixed = (int)((((double)(sum_y * sum_xx - sum_x * sum_xy)) * 1000.0 / denom) + 0.5);
    reg_model.fallback_value = 0;
    reg_model.valid = 1;
    train_cnt++;
}

unsigned int predict_valid_page(int util) {
    if (!reg_model.valid) {
        return_fb++;
        return reg_model.fallback_value;
    }

    int predicted = (reg_model.a_fixed * util + reg_model.b_fixed) / 1000;

    // 범위 제한: 최소 0, 최대 128
    if (predicted < 0) predicted = 0;
    if (predicted > 128) predicted = 128;

    int a_int = reg_model.a_fixed / 1000;
    int a_frac = reg_model.a_fixed % 1000;
    if (a_frac < 0) a_frac = -a_frac;
    int b_int = reg_model.b_fixed / 1000;
    int b_frac = reg_model.b_fixed % 1000;
    if (b_frac < 0) b_frac = -b_frac;

    return_rg++;
    return predicted;
}

