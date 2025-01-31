//////////////////////////////////////////////////////////////////////////////////
// request_transform.c for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//			      Sangjin Lee <sjlee@enc.hanyang.ac.kr>
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
// Module Name: Request Scheduler
// File Name: request_transform.c
//
// Version: v1.0.0
//
// Description:
//	 - transform request information
//   - check dependency between requests
//   - issue host DMA request to host DMA engine
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include <assert.h>
#include "nvme/nvme.h"
#include "nvme/host_lld.h"
#include "memory_map.h"
#include "ftl_config.h"
#include "data_buffer.h"

P_ROW_ADDR_DEPENDENCY_TABLE rowAddrDependencyTablePtr;

void InitDependencyTable()
{
	unsigned int blockNo, wayNo, chNo;
	rowAddrDependencyTablePtr = (P_ROW_ADDR_DEPENDENCY_TABLE)ROW_ADDR_DEPENDENCY_TABLE_ADDR;

	for(blockNo=0 ; blockNo<MAIN_BLOCKS_PER_DIE ; blockNo++)
	{
		for(wayNo=0 ; wayNo<USER_WAYS ; wayNo++)
		{
			for(chNo=0 ; chNo<USER_CHANNELS ; chNo++)
			{
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 0;
			}
		}
	}
}

void ReqTransNvmeToSliceForDSM(unsigned int cmdSlotTag, unsigned int nr)
{
	unsigned int reqSlotTag, reqCode;
	reqSlotTag = GetFromFreeReqQ();

	reqCode = REQ_CODE_DSM;

	reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_SLICE;
	reqPoolPtr->reqPool[reqSlotTag].reqCode = reqCode;
	reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = cmdSlotTag;
	if (trim_flag == 0)
	{
		trim_flag = 1;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = LSA_DSM;
		trim_LSA = LSA_DSM - 1;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = trim_LSA;
		trim_LSA = LSA_DSM - 1;
	}
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.startIndex = 0;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nvmeBlockOffset = 0;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock = 1;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nr = nr;

	PutToSliceReqQ(reqSlotTag);
}

void ReqTransNvmeToSlice(unsigned int cmdSlotTag, unsigned int startLba, unsigned int nlb, unsigned int cmdCode)
{
	unsigned int dma_proc, slsa, elsa, start_index, end_index, reqSlotTag, requestedNvmeBlock, tempNumOfNvmeBlock, transCounter, tempLsa, loop, nvmeBlockOffset, nvmeDmaStartIndex, reqCode;
	unsigned long long start_mask, end_mask;

	requestedNvmeBlock = nlb + 1;
	transCounter = 0;
	nvmeDmaStartIndex = 0;
	tempLsa = startLba / NVME_BLOCKS_PER_SLICE;
	loop = ((startLba % NVME_BLOCKS_PER_SLICE) + requestedNvmeBlock) / NVME_BLOCKS_PER_SLICE;

	if(cmdCode == IO_NVM_WRITE)
	{
		reqCode = REQ_CODE_WRITE;

		if (trim_flag == 1)
		{
			slsa = startLba/4;
			elsa = (startLba + nlb - 1)/4;
			start_index = slsa/64;
			end_index = elsa/64;

			start_mask = ~0ULL << (slsa % 64);
			end_mask = ~0ULL >> (64 - ((elsa % 64)+1));

			if (start_index == end_index)
			{
				asyncTrimBitMapPtr->writeBitMap[start_index] |= (start_mask & end_mask);
			}
			else
			{
				asyncTrimBitMapPtr->writeBitMap[start_index] |= start_mask;
				asyncTrimBitMapPtr->writeBitMap[end_index] |= end_mask;
				for (int i = start_index + 1; i < end_index; i++)
				{
					asyncTrimBitMapPtr->writeBitMap[i] = ~0ULL;
				}
			}
		//			if (trimDmaCnt == 0)
		//			{
		//				slsa = startLba/4;
		//				elsa = (startLba + nlb - 1)/4;
		//				start_index = slsa/64;
		//				end_index = elsa/64;
		//
		//				start_mask = ~0ULL << (slsa % 64);
		//				end_mask = ~0ULL >> (64 - ((elsa % 64)+1));
		//
		//				if (start_index == end_index)
		//				{
		//					asyncTrimBitMapPtr->trimBitMap[start_index] &= ~(start_mask & end_mask);
		//				}
		//				else
		//				{
		//					asyncTrimBitMapPtr->trimBitMap[start_index] &= ~start_mask;
		//					asyncTrimBitMapPtr->trimBitMap[end_index] &= ~end_mask;
		//					while((start_index+1) < end_index)
		//					{
		//						asyncTrimBitMapPtr->trimBitMap[++start_index] &= 0ULL;
		//					}
		//
		//				}
		//				dma_proc = 0;
		//			}
		//			else
		//				dma_proc = 1;
		}

	}
	else if(cmdCode == IO_NVM_READ)
		reqCode = REQ_CODE_READ;
	else
		assert(!"[WARNING] Not supported command code [WARNING]");

	//first transform
	nvmeBlockOffset = (startLba % NVME_BLOCKS_PER_SLICE);
	if(loop)
		tempNumOfNvmeBlock = NVME_BLOCKS_PER_SLICE - nvmeBlockOffset;
	else
		tempNumOfNvmeBlock = requestedNvmeBlock;

	reqSlotTag = GetFromFreeReqQ();

	reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_SLICE;
	reqPoolPtr->reqPool[reqSlotTag].reqCode = reqCode;
	reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = cmdSlotTag;
	reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = tempLsa;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.startIndex = nvmeDmaStartIndex;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nvmeBlockOffset = nvmeBlockOffset;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock = tempNumOfNvmeBlock;
	reqPoolPtr->reqPool[reqSlotTag].reqOpt.trimDmaFlag = dma_proc;

	if (nvmeBlockOffset == 0)
	{
		if (nlb == 1)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
		}
		else if (nlb == 2)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
		}
		else if (nlb == 3)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
		}
		else
		{
			reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;
		}
	}
	else if (nvmeBlockOffset == 1)
	{
		if (nlb == 1)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
		}
		else if (nlb == 2)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
		}
		else
		{
			reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;
		}
	}
	else if (nvmeBlockOffset == 2)
	{
		if (nlb == 1)
		{
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
		}
		else
		{
			reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
			reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;
		}
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;
	}

	PutToSliceReqQ(reqSlotTag);

	tempLsa++;
	transCounter++;
	nvmeDmaStartIndex += tempNumOfNvmeBlock;

	//transform continue
	while(transCounter < loop)
	{
		nvmeBlockOffset = 0;
		tempNumOfNvmeBlock = NVME_BLOCKS_PER_SLICE;

		reqSlotTag = GetFromFreeReqQ();

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_SLICE;
		reqPoolPtr->reqPool[reqSlotTag].reqCode = reqCode;
		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = cmdSlotTag;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = tempLsa;
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.startIndex = nvmeDmaStartIndex;
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nvmeBlockOffset = nvmeBlockOffset;
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock = tempNumOfNvmeBlock;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.trimDmaFlag = dma_proc;
		reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;

		PutToSliceReqQ(reqSlotTag);

		tempLsa++;
		transCounter++;
		nvmeDmaStartIndex += tempNumOfNvmeBlock;
	}

	//last transform
	nvmeBlockOffset = 0;
	tempNumOfNvmeBlock = (startLba + requestedNvmeBlock) % NVME_BLOCKS_PER_SLICE;
	if((tempNumOfNvmeBlock == 0) || (loop == 0))
		return ;

	reqSlotTag = GetFromFreeReqQ();

	reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_SLICE;
	reqPoolPtr->reqPool[reqSlotTag].reqCode = reqCode;
	reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = cmdSlotTag;
	reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = tempLsa;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.startIndex = nvmeDmaStartIndex;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nvmeBlockOffset = nvmeBlockOffset;
	reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock = tempNumOfNvmeBlock;
	reqPoolPtr->reqPool[reqSlotTag].reqOpt.trimDmaFlag = dma_proc;

	if (tempNumOfNvmeBlock == 1)
	{
		reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
	}
	else if (tempNumOfNvmeBlock == 2)
	{
		reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
	}
	else if (tempNumOfNvmeBlock == 3)
	{
		reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
	}
	else
	{
		reqPoolPtr->reqPool[reqSlotTag].blk0 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk1 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk2 = 1;
		reqPoolPtr->reqPool[reqSlotTag].blk3 = 1;
	}

	PutToSliceReqQ(reqSlotTag);
}



void EvictDataBufEntry(unsigned int originReqSlotTag)
{
	unsigned int reqSlotTag, virtualSliceAddr, dataBufEntry;

	dataBufEntry = reqPoolPtr->reqPool[originReqSlotTag].dataBufInfo.entry;
	if(dataBufMapPtr->dataBuf[dataBufEntry].dirty == DATA_BUF_DIRTY)
	{
		CheckDoneNvmeDmaReq(); //Is it Ok?
		reqSlotTag = GetFromFreeReqQ();
		virtualSliceAddr =  AddrTransWrite(dataBufEntry);

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
		reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = reqPoolPtr->reqPool[originReqSlotTag].nvmeCmdSlotTag;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = dataBufMapPtr->dataBuf[dataBufEntry].logicalSliceAddr;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
		reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;
		UpdateDataBufEntryInfoBlockingReq(dataBufEntry, reqSlotTag);
		reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

		SelectLowLevelReqQ(reqSlotTag);

		dataBufMapPtr->dataBuf[dataBufEntry].dirty = DATA_BUF_CLEAN;
	}
}

void DataReadFromNand(unsigned int originReqSlotTag)
{
	unsigned int reqSlotTag, virtualSliceAddr;

	virtualSliceAddr =  AddrTransRead(reqPoolPtr->reqPool[originReqSlotTag].logicalSliceAddr);

	if(virtualSliceAddr != VSA_FAIL)
	{
		reqSlotTag = GetFromFreeReqQ();

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
		reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
		reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag = reqPoolPtr->reqPool[originReqSlotTag].nvmeCmdSlotTag;
		reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = reqPoolPtr->reqPool[originReqSlotTag].logicalSliceAddr;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_ON;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;

		reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = reqPoolPtr->reqPool[originReqSlotTag].dataBufInfo.entry;
		UpdateDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
		reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

		SelectLowLevelReqQ(reqSlotTag);
	}
}


void ReqTransSliceToLowLevel()
{
	unsigned int reqSlotTag, dataBufEntry;

	while(sliceReqQ.headReq != REQ_SLOT_TAG_NONE)
	{
		reqSlotTag = GetFromSliceReqQ();
		if(reqSlotTag == REQ_SLOT_TAG_FAIL)
			return ;

		//allocate a data buffer entry for this request
		dataBufEntry = CheckDataBufHit(reqSlotTag);
		if(dataBufEntry != DATA_BUF_FAIL)
		{
			//data buffer hit
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;
		}
		else
		{
			//data buffer miss, allocate a new buffer entry
			dataBufEntry = AllocateDataBuf();
			reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = dataBufEntry;

			//clear the allocated data buffer entry being used by a previous request
			EvictDataBufEntry(reqSlotTag);

			//update meta-data of the allocated data buffer entry
			dataBufMapPtr->dataBuf[dataBufEntry].logicalSliceAddr = reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr;
			PutToDataBufHashList(dataBufEntry);

			if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
				DataReadFromNand(reqSlotTag);
			else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE)
				if(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock != NVME_BLOCKS_PER_SLICE) //for read modify write
					DataReadFromNand(reqSlotTag);
		}

		//transform this slice request to nvme request
		if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_WRITE)
		{
			wr_cnt++;
			dataBufMapPtr->dataBuf[dataBufEntry].dirty = DATA_BUF_DIRTY;
			dataBufMapPtr->dataBuf[dataBufEntry].blk0 = reqPoolPtr->reqPool[reqSlotTag].blk0;
			dataBufMapPtr->dataBuf[dataBufEntry].blk1 = reqPoolPtr->reqPool[reqSlotTag].blk1;
			dataBufMapPtr->dataBuf[dataBufEntry].blk2 = reqPoolPtr->reqPool[reqSlotTag].blk2;
			dataBufMapPtr->dataBuf[dataBufEntry].blk3 = reqPoolPtr->reqPool[reqSlotTag].blk3;
//			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_RxDMA;
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_READ)
			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_TxDMA;
		else if(reqPoolPtr->reqPool[reqSlotTag].reqCode  == REQ_CODE_DSM)
		{
//			reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_RxDMA;
			dataBufMapPtr->dataBuf[dataBufEntry].dirty = DATA_BUF_CLEAN;
		}
		else
			assert(!"[WARNING] Not supported reqCode. [WARNING]");

		reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NVME_DMA;
		reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_ENTRY;

		UpdateDataBufEntryInfoBlockingReq(dataBufEntry, reqSlotTag);
		SelectLowLevelReqQ(reqSlotTag);
	}
}

unsigned int CheckBufDep(unsigned int reqSlotTag)
{
	if(reqPoolPtr->reqPool[reqSlotTag].prevBlockingReq == REQ_SLOT_TAG_NONE)
		return BUF_DEPENDENCY_REPORT_PASS;
	else
		return BUF_DEPENDENCY_REPORT_BLOCKED;
}


unsigned int CheckRowAddrDep(unsigned int reqSlotTag, unsigned int checkRowAddrDepOpt)
{
	unsigned int dieNo,chNo, wayNo, blockNo, pageNo;

	if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_VSA)
	{
		dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		chNo =  Vdie2PchTranslation(dieNo);
		wayNo = Vdie2PwayTranslation(dieNo);
		blockNo = Vsa2VblockTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		pageNo = Vsa2VpageTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
	}
	else
		assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");

	if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
	{
		if(checkRowAddrDepOpt == ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT)
		{
			if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag)
				SyncReleaseEraseReq(chNo, wayNo, blockNo);

			if(pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
				return ROW_ADDR_DEPENDENCY_REPORT_PASS;

			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
		}
		else if(checkRowAddrDepOpt == ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE)
		{
			if(pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
			{
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt--;
				return	ROW_ADDR_DEPENDENCY_REPORT_PASS;
			}
		}
		else
			assert(!"[WARNING] Not supported checkRowAddrDepOpt [WARNING]");
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_WRITE)
	{
		if(pageNo == rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
		{
			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage++;

			return ROW_ADDR_DEPENDENCY_REPORT_PASS;
		}
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_ERASE)
	{
		if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage == reqPoolPtr->reqPool[reqSlotTag].nandInfo.programmedPageCnt)
			if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt == 0)
			{
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage = 0;
				rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 0;

				return ROW_ADDR_DEPENDENCY_REPORT_PASS;
			}

		if(checkRowAddrDepOpt == ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT)
			rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 1;
		else if(checkRowAddrDepOpt == ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE)
		{
			//pass, go to return
		}
		else
			assert(!"[WARNING] Not supported checkRowAddrDepOpt [WARNING]");
	}
	else
		assert(!"[WARNING] Not supported reqCode [WARNING]");

	return ROW_ADDR_DEPENDENCY_REPORT_BLOCKED;
}


unsigned int UpdateRowAddrDepTableForBufBlockedReq(unsigned int reqSlotTag)
{
	unsigned int dieNo, chNo, wayNo, blockNo, pageNo, bufDepCheckReport;

	if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_VSA)
	{
		dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		chNo =  Vdie2PchTranslation(dieNo);
		wayNo = Vdie2PwayTranslation(dieNo);
		blockNo = Vsa2VblockTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
		pageNo = Vsa2VpageTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
	}
	else
		assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");

	if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_READ)
	{
		if(rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag)
		{
			SyncReleaseEraseReq(chNo, wayNo, blockNo);

			bufDepCheckReport = CheckBufDep(reqSlotTag);
			if(bufDepCheckReport == BUF_DEPENDENCY_REPORT_PASS)
			{
				if(pageNo < rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].permittedProgPage)
					PutToNandReqQ(reqSlotTag, chNo, wayNo);
				else
				{
					rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
					PutToBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				}

				return ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_SYNC;
			}
		}
		rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedReadReqCnt++;
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_ERASE)
		rowAddrDependencyTablePtr->block[chNo][wayNo][blockNo].blockedEraseReqFlag = 1;

	return ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_DONE;
}



void SelectLowLevelReqQ(unsigned int reqSlotTag)
{
	unsigned int dieNo, chNo, wayNo, bufDepCheckReport, rowAddrDepCheckReport, rowAddrDepTableUpdateReport;

	bufDepCheckReport = CheckBufDep(reqSlotTag);
	if(bufDepCheckReport == BUF_DEPENDENCY_REPORT_PASS)
	{
		if((reqPoolPtr->reqPool[reqSlotTag].reqType  == REQ_TYPE_NVME_DMA))
		{
//			if(trim_flag == 1)
//			{
//				if (reqPoolPtr->reqPool[reqSlotTag].ioType == REQ_CODE_DSM)
//					xil_printf("dsm command2\r\n");
//			}
			IssueNvmeDmaReq(reqSlotTag);
			PutToNvmeDmaReqQ(reqSlotTag);
		}
		else if(reqPoolPtr->reqPool[reqSlotTag].reqType  == REQ_TYPE_NAND)
		{
			if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_VSA)
			{
				dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr);
				chNo =  Vdie2PchTranslation(dieNo);
				wayNo = Vdie2PwayTranslation(dieNo);
			}
			else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_PHY_ORG)
			{
				chNo =  reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalCh;
				wayNo = reqPoolPtr->reqPool[reqSlotTag].nandInfo.physicalWay;
			}
			else
				assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");

			if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK)
			{
				rowAddrDepCheckReport = CheckRowAddrDep(reqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_SELECT);

				if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_PASS)
					PutToNandReqQ(reqSlotTag, chNo, wayNo);
				else if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_BLOCKED)
					PutToBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				else
					assert(!"[WARNING] Not supported report [WARNING]");
			}
			else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_NONE)
				PutToNandReqQ(reqSlotTag, chNo, wayNo);
			else
				assert(!"[WARNING] Not supported reqOpt [WARNING]");

		}
		else
			assert(!"[WARNING] Not supported reqType [WARNING]");
	}
	else if(bufDepCheckReport == BUF_DEPENDENCY_REPORT_BLOCKED)
	{
		if(reqPoolPtr->reqPool[reqSlotTag].reqType  == REQ_TYPE_NAND)
			if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK)
			{
				rowAddrDepTableUpdateReport = UpdateRowAddrDepTableForBufBlockedReq(reqSlotTag);

				if(rowAddrDepTableUpdateReport == ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_DONE)
				{
					//pass, go to PutToBlockedByBufDepReqQ
				}
				else if(rowAddrDepTableUpdateReport == ROW_ADDR_DEPENDENCY_TABLE_UPDATE_REPORT_SYNC)
					return;
				else
					assert(!"[WARNING] Not supported report [WARNING]");
			}

		PutToBlockedByBufDepReqQ(reqSlotTag);
	}
	else
		assert(!"[WARNING] Not supported report [WARNING]");
}


void ReleaseBlockedByBufDepReq(unsigned int reqSlotTag)
{
	unsigned int targetReqSlotTag, dieNo, chNo, wayNo, rowAddrDepCheckReport;

	targetReqSlotTag = REQ_SLOT_TAG_NONE;
	if(reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq != REQ_SLOT_TAG_NONE)
	{
		targetReqSlotTag = reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq;
		reqPoolPtr->reqPool[targetReqSlotTag].prevBlockingReq = REQ_SLOT_TAG_NONE;
		reqPoolPtr->reqPool[reqSlotTag].nextBlockingReq = REQ_SLOT_TAG_NONE;
	}

	if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_ENTRY)
	{
		if(dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail == reqSlotTag)
			dataBufMapPtr->dataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail = REQ_SLOT_TAG_NONE;
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat == REQ_OPT_DATA_BUF_TEMP_ENTRY)
	{
		if(tempDataBufMapPtr->tempDataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail == reqSlotTag)
			tempDataBufMapPtr->tempDataBuf[reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry].blockingReqTail = REQ_SLOT_TAG_NONE;
	}

	if((targetReqSlotTag != REQ_SLOT_TAG_NONE) && (reqPoolPtr->reqPool[targetReqSlotTag].reqQueueType == REQ_QUEUE_TYPE_BLOCKED_BY_BUF_DEP))
	{
		SelectiveGetFromBlockedByBufDepReqQ(targetReqSlotTag);

		if(reqPoolPtr->reqPool[targetReqSlotTag].reqType == REQ_TYPE_NVME_DMA)
		{
//			if(trim_flag == 1)
//			{
//				if (reqPoolPtr->reqPool[reqSlotTag].ioType == REQ_CODE_DSM)
//					xil_printf("dsm command3\r\n");
//			}
			IssueNvmeDmaReq(targetReqSlotTag);
			PutToNvmeDmaReqQ(targetReqSlotTag);
		}
		else if(reqPoolPtr->reqPool[targetReqSlotTag].reqType  == REQ_TYPE_NAND)
		{
			if(reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.nandAddr == REQ_OPT_NAND_ADDR_VSA)
			{
				dieNo = Vsa2VdieTranslation(reqPoolPtr->reqPool[targetReqSlotTag].nandInfo.virtualSliceAddr);
				chNo =  Vdie2PchTranslation(dieNo);
				wayNo = Vdie2PwayTranslation(dieNo);
			}
			else
				assert(!"[WARNING] Not supported reqOpt-nandAddress [WARNING]");

			if(reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK)
			{
				rowAddrDepCheckReport = CheckRowAddrDep(targetReqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE);

				if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_PASS)
					PutToNandReqQ(targetReqSlotTag, chNo, wayNo);
				else if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_BLOCKED)
					PutToBlockedByRowAddrDepReqQ(targetReqSlotTag, chNo, wayNo);
				else
					assert(!"[WARNING] Not supported report [WARNING]");
			}
			else if(reqPoolPtr->reqPool[targetReqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_NONE)
				PutToNandReqQ(targetReqSlotTag, chNo, wayNo);
			else
				assert(!"[WARNING] Not supported reqOpt [WARNING]");
		}
	}
}


void ReleaseBlockedByRowAddrDepReq(unsigned int chNo, unsigned int wayNo)
{
	unsigned int reqSlotTag, nextReq, rowAddrDepCheckReport;

	reqSlotTag = blockedByRowAddrDepReqQ[chNo][wayNo].headReq;

	while(reqSlotTag != REQ_SLOT_TAG_NONE)
	{
		nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;

		if(reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck == REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK)
		{
			rowAddrDepCheckReport = CheckRowAddrDep(reqSlotTag, ROW_ADDR_DEPENDENCY_CHECK_OPT_RELEASE);

			if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_PASS)
			{
				SelectiveGetFromBlockedByRowAddrDepReqQ(reqSlotTag, chNo, wayNo);
				PutToNandReqQ(reqSlotTag, chNo, wayNo);
			}
			else if(rowAddrDepCheckReport == ROW_ADDR_DEPENDENCY_REPORT_BLOCKED)
			{
				//pass, go to while loop
			}
			else
				assert(!"[WARNING] Not supported report [WARNING]");
		}
		else
			assert(!"[WARNING] Not supported reqOpt [WARNING]");

		reqSlotTag = nextReq;
	}
}


void IssueNvmeDmaReq(unsigned int reqSlotTag)
{
	unsigned int devAddr, dmaIndex, numOfNvmeBlock;

	dmaIndex = reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.startIndex;
	devAddr = GenerateDataBufAddr(reqSlotTag);
	numOfNvmeBlock = 0;

	if((reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_WRITE)||(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_DSM))
	{
		while(numOfNvmeBlock < reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock)
		{
			set_auto_rx_dma(reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag, dmaIndex, devAddr, NVME_COMMAND_AUTO_COMPLETION_ON);

			numOfNvmeBlock++;
			dmaIndex++;
			devAddr += BYTES_PER_NVME_BLOCK;
		}
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.reqTail = g_hostDmaStatus.fifoTail.autoDmaRx;
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.overFlowCnt = g_hostDmaAssistStatus.autoDmaRxOverFlowCnt;
	}
	else if(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_TxDMA)
	{
		while(numOfNvmeBlock < reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.numOfNvmeBlock)
		{
			set_auto_tx_dma(reqPoolPtr->reqPool[reqSlotTag].nvmeCmdSlotTag, dmaIndex, devAddr, NVME_COMMAND_AUTO_COMPLETION_ON);

			numOfNvmeBlock++;
			dmaIndex++;
			devAddr += BYTES_PER_NVME_BLOCK;
		}
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.reqTail =  g_hostDmaStatus.fifoTail.autoDmaTx;
		reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.overFlowCnt = g_hostDmaAssistStatus.autoDmaTxOverFlowCnt;
	}
	else
		assert(!"[WARNING] Not supported reqCode [WARNING]");
}

void TRIM (unsigned int lba, unsigned int blk0, unsigned int blk1, unsigned int blk2, unsigned int blk3)
{
	trim_cnt++;
	unsigned int lsa, bufEntry;
	lsa = lba/4;

	if (asyncTrimBitMapPtr->writeBitMap[lsa/64] & (1ULL << (lsa % 64)))
		return;

//	xil_printf("LSA %d will be checked\r\n",lsa);
//	if ((blk0 == 0)&&(blk1 == 0)&&(blk2 == 0)&&(blk3 == 0))
//		xil_printf("LSA %d will be trimed\r\n",lsa);

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
//        	xil_printf("This LSA will be Cleaned in L2P: %d!!\r\n", lsa);
			InvalidateOldVsa(lsa);
		}
	}
}

void PerformDeallocation(unsigned int reqSlotTag)
{
    int tempval, tempval2;
    unsigned int newEntry;
    unsigned int *devAddr = (unsigned int*)GenerateDataBufAddr(reqSlotTag);
    unsigned int nr = reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.nr;

    for (int i = 0; i < nr; i++)
    {
        tempval = *(devAddr + 1);
        tempval2 = *(devAddr + 2);

        // Validation check for boundaries
        if (((SLICES_PER_SSD * 4) < tempval) || ((SLICES_PER_SSD * 4) < tempval2))
        {
        	xil_printf("deallocation validation error\r\n");
            break;
        }

        nr_sum++;

        newEntry = AllocateDSMBuf();
        xil_printf("newEntry: %u\r\n", newEntry);
        dsmRangePtr->dsmRange[newEntry].lengthInLogicalBlocks = tempval;
        dsmRangePtr->dsmRange[newEntry].startingLBA[0] = tempval2;
        PutToDsmRangeHashList(newEntry);
        xil_printf("length: %u\r\n", dsmRangePtr->dsmRange[newEntry].lengthInLogicalBlocks);
//        xil_printf("start lba: %u\r\n", dsmRangePtr->dsmRange[newEntry].startingLBA[0]);

        devAddr += 4;
    }
}


void CheckDoneNvmeDmaReq()
{
	unsigned int reqSlotTag, nextReq; //prevReq
	unsigned int rxDone, txDone;

//	reqSlotTag = nvmeDmaReqQ.head;
	reqSlotTag = nvmeDmaReqQ.headReq;
	rxDone = 0;
	txDone = 0;

	while(reqSlotTag != REQ_SLOT_TAG_NONE)
	{
		nextReq = reqPoolPtr->reqPool[reqSlotTag].nextReq;
//		prevReq = reqPoolPtr->reqPool[reqSlotTag].prevReq;

		if((reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_WRITE)||(reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_DSM))
		{
			if(!rxDone)
			{
				rxDone = check_auto_rx_dma_partial_done(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.reqTail , reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.overFlowCnt);
				if (reqPoolPtr->reqPool[reqSlotTag].reqCode == REQ_CODE_DSM)
				{
					reqSlotTag = nvmeDmaReqQ.headReq;
				}
			}

			if(rxDone)
				SelectiveGetFromNvmeDmaReqQ(reqSlotTag);
		}
		else
		{
			if(!txDone)
				txDone = check_auto_tx_dma_partial_done(reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.reqTail , reqPoolPtr->reqPool[reqSlotTag].nvmeDmaInfo.overFlowCnt);

			if(txDone)
				SelectiveGetFromNvmeDmaReqQ(reqSlotTag);
		}
//		reqSlotTag = prevReq;
		reqSlotTag = nextReq;
	}
}



