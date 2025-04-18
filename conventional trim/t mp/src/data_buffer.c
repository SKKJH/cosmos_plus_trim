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
#include "nvme/nvme.h"


P_DATA_BUF_MAP dataBufMapPtr;
DATA_BUF_LRU_LIST dataBufLruList;
P_DATA_BUF_HASH_TABLE dataBufHashTablePtr;
P_TEMPORARY_DATA_BUF_MAP tempDataBufMapPtr;
P_REG_BUF_MAP regressionBufMapPtr;

void InitDataBuf()
{
	int bufEntry;

	dataBufMapPtr = (P_DATA_BUF_MAP) DATA_BUFFER_MAP_ADDR;
	dataBufHashTablePtr = (P_DATA_BUF_HASH_TABLE)DATA_BUFFFER_HASH_TABLE_ADDR;
	tempDataBufMapPtr = (P_TEMPORARY_DATA_BUF_MAP)TEMPORARY_DATA_BUFFER_MAP_ADDR;
	regressionBufMapPtr = (P_REG_BUF_MAP)TEMPORARY_REG_BUFFER_MAP_ADDR;

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
    	train_fb++;
        return;
    }

    int n = count;
    int denom = n * sum_xx - sum_x * sum_x;

    reg_model.a_fixed = (int)(((n * sum_xy - sum_x * sum_y) * 1000) / denom);
    reg_model.b_fixed = (int)((sum_y * 1000 - (int)(reg_model.a_fixed) * sum_x) / n);
    reg_model.fallback_value = 0;
    reg_model.valid = 1;
	train_rg++;
}

unsigned int predict_valid_page(int util) {
    if (!reg_model.valid) {
//        xil_printf(" 회귀 불가 또는 데이터 없음 → fallback 사용: valid_page = %d\n",
//                   reg_model.fallback_value);
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

    return predicted;
}




