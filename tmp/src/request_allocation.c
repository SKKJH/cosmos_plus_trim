//////////////////////////////////////////////////////////////////////////////////
// request_allocation.c for Cosmos+ OpenSSD
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
// Module Name: Request Allocator
// File Name: request_allocation.c
//
// Version: v1.0.0
//
// Description:
//   - allocate requests to each request queue
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

P_REQ_POOL reqPoolPtr;
FREE_REQUEST_QUEUE freeReqQ;
SLICE_REQUEST_QUEUE sliceReqQ;
BLOCKED_BY_BUFFER_DEPENDENCY_REQUEST_QUEUE blockedByBufDepReqQ;
BLOCKED_BY_ROW_ADDR_DEPENDENCY_REQUEST_QUEUE blockedByRowAddrDepReqQ[USER_CHANNELS][USER_WAYS];
NVME_DMA_REQUEST_QUEUE nvmeDmaReqQ;
NAND_REQUEST_QUEUE nandReqQ[USER_CHANNELS][USER_WAYS];

unsigned int notCompletedNandReqCnt;
unsigned int blockedReqCnt;

void InitReqPool()
{
	int chNo, wayNo, reqSlotTag;

	reqPoolPtr = (P_REQ_POOL) REQ_POOL_ADDR; //revise address

	freeReqQ.headReq = 0;
	freeReqQ.tailReq = AVAILABLE_OUNTSTANDING_REQ_COUNT - 1;

	sliceReqQ.headReq = REQ_SLOT_TAG_NONE;
	sliceReqQ.tailReq = REQ_SLOT_TAG_NONE;
	sliceReqQ.reqCnt = 0;

	blockedByBufDepReqQ.headReq = REQ_SLOT_TAG_NONE;
	blockedByBufDepReqQ.tailReq = REQ_SLOT_TAG_NONE;
	blockedByBufDepReqQ.reqCnt = 0;

	nvmeDmaReqQ.headReq = REQ_SLOT_TAG_NONE;
	nvmeDmaReqQ.tailReq = REQ_SLOT_TAG_NONE;
	nvmeDmaReqQ.reqCnt = 0;

	for(chNo = 0; chNo<USER_CHANNELS; chNo++)
		for(wayNo = 0; wayNo<USER_WAYS; wayNo++)
		{
			blockedByRowAddrDepReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
			blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
			blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt = 0;

			nandReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
			nandReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
			nandReqQ[chNo][wayNo].reqCnt = 0;
		}

	for(reqSlotTag = 0; reqSlotTag < AVAILABLE_OUNTSTANDING_REQ_COUNT; reqSlotTag++)
	{
		reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_FREE;
		reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].prevReq = reqSlotTag - 1;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = reqSlotTag + 1;
	}

	reqPoolPtr->reqPool[0].prevReq = REQ_SLOT_TAG_NONE;
	reqPoolPtr->reqPool[AVAILABLE_OUNTSTANDING_REQ_COUNT - 1].nextReq = REQ_SLOT_TAG_NONE;
	freeReqQ.reqCnt = AVAILABLE_OUNTSTANDING_REQ_COUNT;

	notCompletedNandReqCnt = 0;
	blockedReqCnt = 0;
}


void PutToFreeReqQ(unsigned int reqSlotTag)
{
	if(freeReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = freeReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[freeReqQ.tailReq].nextReq = reqSlotTag;
		freeReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		freeReqQ.headReq = reqSlotTag;
		freeReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_FREE;
	freeReqQ.reqCnt++;
}

unsigned int GetFromFreeReqQ()
{
	unsigned int reqSlotTag;

	reqSlotTag = freeReqQ.headReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
	{
		SyncAvailFreeReq();
		reqSlotTag = freeReqQ.headReq;
	}

	if(reqPoolPtr->reqPool[reqSlotTag].nextReq != REQ_SLOT_TAG_NONE)
	{
		freeReqQ.headReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		freeReqQ.headReq = REQ_SLOT_TAG_NONE;
		freeReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	freeReqQ.reqCnt--;

	return reqSlotTag;
}

void PutToSliceReqQ(unsigned int reqSlotTag)
{
	if(sliceReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = sliceReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[sliceReqQ.tailReq].nextReq = reqSlotTag;
		sliceReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		sliceReqQ.headReq = reqSlotTag;
		sliceReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_SLICE;
	sliceReqQ.reqCnt++;
}

unsigned int GetFromSliceReqQ()
{
	unsigned int reqSlotTag;

	reqSlotTag = sliceReqQ.headReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		return REQ_SLOT_TAG_FAIL;

	if(reqPoolPtr->reqPool[reqSlotTag].nextReq != REQ_SLOT_TAG_NONE)
	{
		sliceReqQ.headReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		sliceReqQ.headReq = REQ_SLOT_TAG_NONE;
		sliceReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	sliceReqQ.reqCnt--;

	return reqSlotTag;
}

void PutToBlockedByBufDepReqQ(unsigned int reqSlotTag)
{
	if(blockedByBufDepReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = blockedByBufDepReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[blockedByBufDepReqQ.tailReq].nextReq = reqSlotTag;
		blockedByBufDepReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.headReq = reqSlotTag;
		blockedByBufDepReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_BLOCKED_BY_BUF_DEP;
	blockedByBufDepReqQ.reqCnt++;
	blockedReqCnt++;
}
void SelectiveGetFromBlockedByBufDepReqQ(unsigned int reqSlotTag)
{
	unsigned int prevReq, nextReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		assert(!"[WARNING] Wrong reqSlotTag [WARNING]");

	prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;
	nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.headReq = nextReq;
	}
	else
	{
		blockedByBufDepReqQ.headReq = REQ_SLOT_TAG_NONE;
		blockedByBufDepReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_NONE;
	blockedByBufDepReqQ.reqCnt--;
	blockedReqCnt--;
}

void PutToBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	if(blockedByRowAddrDepReqQ[chNo][wayNo].tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = blockedByRowAddrDepReqQ[chNo][wayNo].tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[blockedByRowAddrDepReqQ[chNo][wayNo].tailReq].nextReq = reqSlotTag;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = reqSlotTag;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType =  REQ_QUEUE_TYPE_BLOCKED_BY_ROW_ADDR_DEP;
	blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt++;
	blockedReqCnt++;
}
void SelectiveGetFromBlockedByRowAddrDepReqQ(unsigned int reqSlotTag, unsigned int chNo, unsigned int wayNo)
{
	unsigned int prevReq, nextReq;

	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		assert(!"[WARNING] Wrong reqSlotTag [WARNING]");

	prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;
	nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = nextReq;
	}
	else
	{
		blockedByRowAddrDepReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
		blockedByRowAddrDepReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NONE;
	blockedByRowAddrDepReqQ[chNo][wayNo].reqCnt--;
	blockedReqCnt--;
}

void PutToNvmeDmaReqQ(unsigned int reqSlotTag)
{
	if(nvmeDmaReqQ.tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = nvmeDmaReqQ.tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[nvmeDmaReqQ.tailReq].nextReq = reqSlotTag;
		nvmeDmaReqQ.tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.headReq = reqSlotTag;
		nvmeDmaReqQ.tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NVME_DMA;
	nvmeDmaReqQ.reqCnt++;
}

void TRIM (unsigned int lba, unsigned int blk0, unsigned int blk1, unsigned int blk2, unsigned int blk3)
{
	unsigned int lsa, bufEntry;
	lsa = lba/4;

	bufEntry = CheckDataBufHitbyLSA(lsa);
	if (bufEntry != DATA_BUF_FAIL)
	{
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
//        	xil_printf("This LSA will be Cleaned in WB: %d!!\r\n", lsa);
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
			InvalidateOldVsa(lsa);
		}
	}
}

void PerformDeallocation(unsigned int reqSlotTag)
{
	int nlb, slba, tempNlb, tempSlba, blk0, blk1, blk2, blk3;
//	static XTime tStart, tEnd;
	unsigned int *devAddr = (unsigned int*)GenerateDataBufAddr(reqSlotTag);
	unsigned int nr = reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nr;

	for (int i=0; i<nr; i++)
	{
		blk0 = 1;
		blk1 = 1;
		blk2 = 1;
		blk3 = 1;

		nlb = *(devAddr + 1);
		slba = *(devAddr + 2);

//		xil_printf("nlb: %u\r\n", nlb);
//		XTime_GetTime(&tStart);

		if ( (nlb <= 0) || ((SLICES_PER_SSD * 4) < nlb) || ((SLICES_PER_SSD * 4) < slba) || (0 > slba))
		{
			break;
		}

		if ((slba % 4) == 0)
		{
			if(nlb == 1)
				blk0 = 0;
			else if (nlb == 2)
			{
				blk0 = 0;
				blk1 = 0;
			}
			else if (nlb == 3)
			{
				blk0 = 0;
				blk1 = 0;
				blk2 = 0;
			}
			else
			{
				blk0 = 0;
				blk1 = 0;
				blk2 = 0;
				blk3 = 0;
			}
		}
		else if ((slba % 4) == 1)
		{
			if(nlb == 1)
				blk1 = 0;
			else if (nlb == 2)
			{
				blk1 = 0;
				blk2 = 0;
			}
			else
			{
				blk1 = 0;
				blk2 = 0;
				blk3 = 0;
			}
		}
		else if ((slba % 4) == 2)
		{
			if(nlb == 1)
				blk2 = 0;
			else
			{
				blk2 = 0;
				blk3 = 0;
			}
		}
		else
		{
			blk3 = 0;
		}
		TRIM(slba, blk0, blk1, blk2, blk3);
		tempSlba = slba + (4 - (slba % 4));
		tempNlb = nlb - (4 - (slba % 4));
		nlb = tempNlb;
		slba = tempSlba;

		while(nlb > 4)
		{
			TRIM(slba, 0, 0, 0, 0);
			slba = slba + 4;
			nlb = nlb - 4;
		}

		blk0 = 1;
		blk1 = 1;
		blk2 = 1;
		blk3 = 1;

		if (nlb == 1)
		{
			blk0 = 0;
		}
		else if (nlb == 2)
		{
			blk0 = 0;
			blk1 = 0;
		}
		else if (nlb == 3)
		{
			blk0 = 0;
			blk1 = 0;
			blk2 = 0;
		}
		else
		{
			blk0 = 0;
			blk1 = 0;
			blk2 = 0;
			blk3 = 0;
		}
		TRIM(slba, blk0, blk1, blk2, blk3);
		devAddr += 4;
//		XTime_GetTime(&tEnd);
//		print_clock_cycles(tStart, tEnd);
	}
}

void SelectiveGetFromNvmeDmaReqQ(unsigned int reqSlotTag)
{
	unsigned int prevReq, nextReq;

	prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;
	nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

	if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = nextReq;
		reqPoolPtr->reqPool[nextReq].prevReq = prevReq;
	}
	else if((nextReq == REQ_SLOT_TAG_NONE) && (prevReq != REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[prevReq].nextReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.tailReq = prevReq;
	}
	else if((nextReq != REQ_SLOT_TAG_NONE) && (prevReq == REQ_SLOT_TAG_NONE))
	{
		reqPoolPtr->reqPool[nextReq].prevReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.headReq = nextReq;
	}
	else
	{
		nvmeDmaReqQ.headReq = REQ_SLOT_TAG_NONE;
		nvmeDmaReqQ.tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NONE;
	nvmeDmaReqQ.reqCnt--;

	if ((trim_flag == 1) && (reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_DSM))
		PerformDeallocation(reqSlotTag);

	PutToFreeReqQ(reqSlotTag);
	ReleaseBlockedByBufDepReq(reqSlotTag);
}

void PutToNandReqQ(unsigned int reqSlotTag, unsigned chNo, unsigned wayNo)
{
	if(nandReqQ[chNo][wayNo].tailReq != REQ_SLOT_TAG_NONE)
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = nandReqQ[chNo][wayNo].tailReq;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[nandReqQ[chNo][wayNo].tailReq].nextReq = reqSlotTag;
		nandReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].prevReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextReq = REQ_SLOT_TAG_NONE;
		nandReqQ[chNo][wayNo].headReq = reqSlotTag;
		nandReqQ[chNo][wayNo].tailReq = reqSlotTag;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NAND;
	nandReqQ[chNo][wayNo].reqCnt++;
	notCompletedNandReqCnt++;
}

void GetFromNandReqQ(unsigned int chNo, unsigned int wayNo, unsigned int reqStatus, unsigned int reqCode)
{
	unsigned int reqSlotTag;

	reqSlotTag = nandReqQ[chNo][wayNo].headReq;
	if(reqSlotTag == REQ_SLOT_TAG_NONE)
		assert(!"[WARNING] there is no request in Nand-req-queue[WARNING]");

	if(reqPoolPtr->reqPool[reqSlotTag].nextReq != REQ_SLOT_TAG_NONE)
	{
		nandReqQ[chNo][wayNo].headReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
		reqPoolPtr->reqPool[reqPoolPtr->reqPool[reqSlotTag].nextReq].prevReq = REQ_SLOT_TAG_NONE;
	}
	else
	{
		nandReqQ[chNo][wayNo].headReq = REQ_SLOT_TAG_NONE;
		nandReqQ[chNo][wayNo].tailReq = REQ_SLOT_TAG_NONE;
	}

	reqPoolPtr->reqPool[reqSlotTag].reqQueueType = REQ_QUEUE_TYPE_NONE;
	nandReqQ[chNo][wayNo].reqCnt--;
	notCompletedNandReqCnt--;

	PutToFreeReqQ(reqSlotTag);
	ReleaseBlockedByBufDepReq(reqSlotTag);
}
