//////////////////////////////////////////////////////////////////////////////////
// garbage_collection.c for Cosmos+ OpenSSD
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
// Module Name: Garbage Collector
// File Name: garbage_collection.c
//
// Version: v1.0.0
//
// Description:
//   - select a victim block
//   - collect valid pages to a free block
//   - erase a victim block to make a free block
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
#include "nvme/nvme_io_cmd.h"

P_GC_VICTIM_MAP gcVictimMapPtr;

void InitGcVictimMap()
{
	int dieNo, invalidSliceCnt;

	gcVictimMapPtr = (P_GC_VICTIM_MAP) GC_VICTIM_MAP_ADDR;

	for(dieNo=0 ; dieNo<USER_DIES; dieNo++)
	{
		for(invalidSliceCnt=0 ; invalidSliceCnt<SLICES_PER_BLOCK+1; invalidSliceCnt++)
		{
			gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
			gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
		}
	}
}

unsigned int SelectBestDieForGC()
{
    unsigned int best_die = 0;
    unsigned int min_valid_page = 9999;

    for (unsigned int die = 0; die<USER_DIES; die++) {
        unsigned int victimBlockNo = GetFromGcVictimListNum(die);
        unsigned int valid_page = 128 - virtualBlockMapPtr->block[die][victimBlockNo].invalidSliceCnt;

        if (valid_page < min_valid_page) {
            min_valid_page = valid_page;
            best_die = die;
        }
    }
    return best_die;
}

unsigned int GarbageCollection()
{
	static unsigned int prev_utilization = 0;

	gc_cnt++;
	tcheck = 0;
	unsigned int victimBlockNo, pageNo, virtualSliceAddr, logicalSliceAddr, dieNoForGcCopy, reqSlotTag, dieNo;
	unsigned int utilization =
			100 * (((real_write_cnt) * 16) / 1024) /
		    		(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));

	dieNo = SelectBestDieForGC();
	dieNoForGcCopy = dieNo;
	victimBlockNo = GetFromGcVictimListNum(dieNo);

//	if ((utilization == 70) || (utilization == 75) ||(utilization == 80) || (utilization == 85) ||(utilization == 90) ||(utilization == 95))
//	{
//		if (utilization == 70 && u70 == 0)
//		{
//			u70 = 1;
//			utrim = 0;
//			xil_printf("utilization 70\r\n");
//		}
//		else if (utilization == 75 && u75 == 0)
//		{
//			u75 = 1;
//			utrim = 0;
//			xil_printf("utilization 75\r\n");
//		}
//		else if (utilization == 80 && u80 == 0)
//		{
//			u80 = 1;
//			utrim = 0;
//			xil_printf("utilization 80\r\n");
//		}
//		else if (utilization == 85 && u85 == 0)
//		{
//			u85 = 1;
//			utrim = 0;
//			xil_printf("utilization 85\r\n");
//		}
//		else if (utilization == 90 && u90 == 0)
//		{
//			u90 = 1;
//			utrim = 0;
//			xil_printf("utilization 90\r\n");
//		}
//		else if (utilization == 95 && u95 == 0)
//		{
//			u95 = 1;
//			utrim = 0;
//			xil_printf("utilization 95\r\n");
//		}
//
//		if (utrim < 3)
//		{
//			unsigned int og_valid_page = (128-virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt);
//			ForceDeallocation(256*128);
//			dieNo = SelectBestDieForGC();
//			dieNoForGcCopy = dieNo;
//			victimBlockNo = GetFromGcVictimListNum(dieNo);
//			unsigned int new_valid_page = (128-virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt);
//			if (new_valid_page < og_valid_page)
//			{
//				utrim += 1;
//				xil_printf("Util: %u, og: %u, new: %u, diff: %u\r\n", utilization, og_valid_page, new_valid_page, og_valid_page-new_valid_page);
//			}
//			else
//				xil_printf("Util: %u, no update\r\n", utilization);
//		}
//	}

	victimBlockNo = GetFromGcVictimList(dieNo);
	SyncAllLowLevelReqDone();

//	xil_printf("Util:%u, Valid Page:%u\r\n",utilization, (128-virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt));

	if(virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt != SLICES_PER_BLOCK)
	{
		for(pageNo=0 ; pageNo<USER_PAGES_PER_BLOCK ; pageNo++)
		{
			virtualSliceAddr = Vorg2VsaTranslation(dieNo, victimBlockNo, pageNo);
			logicalSliceAddr = virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr;
			if(logicalSliceAddr != LSA_NONE)
				if(logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr ==  virtualSliceAddr) //valid data
				{
					//read
					reqSlotTag = GetFromFreeReqQ();

					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);
					SyncAllLowLevelReqDone();

					//write
					reqSlotTag = GetFromFreeReqQ();

					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = FindFreeVirtualSliceForGc(dieNoForGcCopy, victimBlockNo);

					logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr;
					virtualSliceMapPtr->virtualSlice[reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);
					SyncAllLowLevelReqDone();
					gc_page_copy += 1;
				}
		}
	}

	EraseBlock(dieNo, victimBlockNo);
	SyncAllLowLevelReqDone();
	return dieNo;
}

unsigned int dTRIM_GarbageCollection()
{
	gc_cnt++;
	tcheck = 0;

	unsigned int victimBlockNo, pageNo, virtualSliceAddr, logicalSliceAddr, dieNoForGcCopy, reqSlotTag, valid_page, new_valid_page;
	unsigned int utilization =
			100 * (((real_write_cnt) * 16) / 1024) /
		    		(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));
	unsigned int dieNo = SelectBestDieForGC();
//	static unsigned int prev_utilization = 0;
	int prev_utilization = 0;

//	if ((utilization == 70) || (utilization == 75) ||(utilization == 80) || (utilization == 85) ||(utilization == 90) ||(utilization == 95))
//	{
//		if (utilization == 70 && u70 == 0)
//		{
//			u70 = 1;
//			utrim = 0;
//			xil_printf("utilization 70\r\n");
//		}
//		else if (utilization == 75 && u75 == 0)
//		{
//			u75 = 1;
//			utrim = 0;
//			xil_printf("utilization 75\r\n");
//		}
//		else if (utilization == 80 && u80 == 0)
//		{
//			u80 = 1;
//			utrim = 0;
//			xil_printf("utilization 80\r\n");
//		}
//		else if (utilization == 85 && u85 == 0)
//		{
//			u85 = 1;
//			utrim = 0;
//			xil_printf("utilization 85\r\n");
//		}
//		else if (utilization == 90 && u90 == 0)
//		{
//			u90 = 1;
//			utrim = 0;
//			xil_printf("utilization 90\r\n");
//		}
//		else if (utilization == 95 && u95 == 0)
//		{
//			u95 = 1;
//			utrim = 0;
//			xil_printf("utilization 95\r\n");
//		}
//
//		if (utrim < 5)
//		{
//			victimBlockNo = GetFromGcVictimListNum(dieNo);
//			valid_page = (128 - virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt);
//
//			force_trim_size = 0;
//			if(do_trim_flag != 0)
//				handle_asyncTrim(1, 256);
//
//			xil_printf("force_trim_size: %u\r\n", force_trim_size);
//			victimBlockNo = GetFromGcVictimListNum(dieNo);
//			new_valid_page = (128 - virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt);
//			if (new_valid_page < valid_page)
//			{
//				utrim += 1;
//				xil_printf("og_valid_page: %u\r\n", new_valid_page);
//			}
//		}
//	}

	victimBlockNo = GetFromGcVictimList(dieNo);
	dieNoForGcCopy = dieNo;

	valid_page = 128 - virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt;
	if (utilization != prev_utilization)
	{
		prev_utilization = utilization;
		xil_printf("utilization: %u, valid_page: %u\r\n", utilization, valid_page);
	}

	SyncAllLowLevelReqDone();
	if(virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt != SLICES_PER_BLOCK)
	{
		for(pageNo=0 ; pageNo<USER_PAGES_PER_BLOCK ; pageNo++)
		{
			virtualSliceAddr = Vorg2VsaTranslation(dieNo, victimBlockNo, pageNo);
			logicalSliceAddr = virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr;
			if(logicalSliceAddr != LSA_NONE)
				if(logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr ==  virtualSliceAddr) //valid data
				{
					//read
					reqSlotTag = GetFromFreeReqQ();

					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);
					SyncAllLowLevelReqDone();

					//write
					reqSlotTag = GetFromFreeReqQ();

					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = FindFreeVirtualSliceForGc(dieNoForGcCopy, victimBlockNo);

					logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr;
					virtualSliceMapPtr->virtualSlice[reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;

					SelectLowLevelReqQ(reqSlotTag);
					SyncAllLowLevelReqDone();
				}
		}
	}
	EraseBlock(dieNo, victimBlockNo);
	SyncAllLowLevelReqDone();
	return dieNo;
}

//void dTRIM_GarbageCollection(unsigned int dieNo)
//{
//	gc_cnt++;
//	tcheck = 0;
//
//	XTime tStart, tEnd;
//	XTime tTime;
//	unsigned int xtime_hi, xtime_lo;
//
//	unsigned int victimBlockNo, pageNo, virtualSliceAddr, logicalSliceAddr, dieNoForGcCopy, reqSlotTag;
//	int valid_page, predicted_valid;
//
//	unsigned int utilization =
//			100 * (((real_write_cnt) * 16) / 1024) /
//		    		(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));
//
//	victimBlockNo = GetFromGcVictimListNum(dieNo);
//	valid_page = (128 - virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt);
//
////	if (valid_page == 0)
////		empty_sample();
//
////	add_sample(utilization, valid_page);
//
//	if(do_trim_flag != 0)
//	{
////		if (train_init == 0)
////			reg_model.fallback_value = valid_page;
////		train_init = 1;
//
////        XTime_GetTime(&tStart);
////		predicted_valid = predict_valid_page(utilization);
////        XTime_GetTime(&tEnd);
////        tTime = (tEnd - tStart);
////        xtime_hi = (unsigned int)(tTime >> 32);
////        xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);
////        xil_printf("First predict_valid overhead: hi = %u, lo = %u\r\n", xtime_hi, xtime_lo);
////        xil_printf("predicted_valid: %u, real valid: %u\r\n", predicted_valid, valid_page);
//
////		if (((predicted_valid > valid_page) && (((predicted_valid - valid_page)) > 5)) ||
////				((valid_page > predicted_valid) && (((valid_page - predicted_valid)) > 5)))
////		{
////			XTime_GetTime(&tStart);
////			train_model();
////			XTime_GetTime(&tEnd);
////			tTime = (tEnd - tStart);
////			xtime_hi = (unsigned int)(tTime >> 32);
////			xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);
//////			xil_printf("Train_model overhead: hi = %u, lo = %u\r\n", xtime_hi, xtime_lo);
////			tcheck = 1;
////		}
//
////		if (tcheck == 1)
////		{
////	        XTime_GetTime(&tStart);
////			predicted_valid = predict_valid_page(utilization);
////	        XTime_GetTime(&tEnd);
////	        tTime = (tEnd - tStart);
////	        xtime_hi = (unsigned int)(tTime >> 32);
////	        xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);
//////	        xil_printf("Second predict_valid_page overhead: hi = %u, lo = %u\r\n", xtime_hi, xtime_lo);
////		}
//
//        XTime_GetTime(&tStart);
////		handle_asyncTrim(1, 256);
//        XTime_GetTime(&tEnd);
//        tTime = (tEnd - tStart);
//        xtime_hi = (unsigned int)(tTime >> 32);
//        xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);
////        xil_printf("Handle_asyncTrim overhead: hi = %u, lo = %u, predicted valid page: %u\r\n", xtime_hi, xtime_lo, predicted_valid + 1);
//	}
//
//	victimBlockNo = GetFromGcVictimList(dieNo);
//	dieNoForGcCopy = dieNo;
//	unsigned int copy_cnt = 0;
//	SyncAllLowLevelReqDone();
//
//	if(virtualBlockMapPtr->block[dieNo][victimBlockNo].invalidSliceCnt != SLICES_PER_BLOCK)
//	{
//        XTime_GetTime(&tStart);
//		for(pageNo=0 ; pageNo<USER_PAGES_PER_BLOCK ; pageNo++)
//		{
//			virtualSliceAddr = Vorg2VsaTranslation(dieNo, victimBlockNo, pageNo);
//			logicalSliceAddr = virtualSliceMapPtr->virtualSlice[virtualSliceAddr].logicalSliceAddr;
//			if(logicalSliceAddr != LSA_NONE)
//				if(logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr ==  virtualSliceAddr) //valid data
//				{
//					//read
//					reqSlotTag = GetFromFreeReqQ();
//
//					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
//					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_READ;
//					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
//					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
//					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
//					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = virtualSliceAddr;
//
//					SelectLowLevelReqQ(reqSlotTag);
//					SyncAllLowLevelReqDone();
//
//					//write
//					reqSlotTag = GetFromFreeReqQ();
//
//					reqPoolPtr->reqPool[reqSlotTag].reqType = REQ_TYPE_NAND;
//					reqPoolPtr->reqPool[reqSlotTag].reqCode = REQ_CODE_WRITE;
//					reqPoolPtr->reqPool[reqSlotTag].logicalSliceAddr = logicalSliceAddr;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.dataBufFormat = REQ_OPT_DATA_BUF_TEMP_ENTRY;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandAddr = REQ_OPT_NAND_ADDR_VSA;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEcc = REQ_OPT_NAND_ECC_ON;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.nandEccWarning = REQ_OPT_NAND_ECC_WARNING_OFF;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.rowAddrDependencyCheck = REQ_OPT_ROW_ADDR_DEPENDENCY_CHECK;
//					reqPoolPtr->reqPool[reqSlotTag].reqOpt.blockSpace = REQ_OPT_BLOCK_SPACE_MAIN;
//					reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry = AllocateTempDataBuf(dieNo);
//					UpdateTempDataBufEntryInfoBlockingReq(reqPoolPtr->reqPool[reqSlotTag].dataBufInfo.entry, reqSlotTag);
//					reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr = FindFreeVirtualSliceForGc(dieNoForGcCopy, victimBlockNo);
//
//					logicalSliceMapPtr->logicalSlice[logicalSliceAddr].virtualSliceAddr = reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr;
//					virtualSliceMapPtr->virtualSlice[reqPoolPtr->reqPool[reqSlotTag].nandInfo.virtualSliceAddr].logicalSliceAddr = logicalSliceAddr;
//
//					SelectLowLevelReqQ(reqSlotTag);
//					SyncAllLowLevelReqDone();
//					gc_page_copy++;
//					copy_cnt++;
//				}
//		}
//        XTime_GetTime(&tEnd);
//        tTime = (tEnd - tStart);
//        xtime_hi = (unsigned int)(tTime >> 32);
//        xtime_lo = (unsigned int)(tTime & 0xFFFFFFFFU);
////        xil_printf("GC Overhead: hi - %u, lo - %u, with Valid Page: %u\r\n", xtime_hi, xtime_lo, copy_cnt);
//	}
////	else
////		xil_printf("GC Overhead: hi - %u, lo - %u, with Valid Page: %u\r\n", 0, 0, 0);
//
//	EraseBlock(dieNo, victimBlockNo);
//	SyncAllLowLevelReqDone();
//}

void PutToGcVictimList(unsigned int dieNo, unsigned int blockNo, unsigned int invalidSliceCnt)
{
	if(gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock != BLOCK_NONE)
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock].nextBlock = blockNo;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = blockNo;
	}
	else
	{
		virtualBlockMapPtr->block[dieNo][blockNo].prevBlock = BLOCK_NONE;
		virtualBlockMapPtr->block[dieNo][blockNo].nextBlock = BLOCK_NONE;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = blockNo;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = blockNo;
	}
}

unsigned int GetFromGcVictimList(unsigned int dieNo)
{
	unsigned int evictedBlockNo;
	int invalidSliceCnt;

	for(invalidSliceCnt = SLICES_PER_BLOCK; invalidSliceCnt > 0 ; invalidSliceCnt--)
	{
		if(gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock != BLOCK_NONE)
		{
			evictedBlockNo = gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock;

			if(virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock != BLOCK_NONE)
			{
				virtualBlockMapPtr->block[dieNo][virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock].prevBlock = BLOCK_NONE;
				gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = virtualBlockMapPtr->block[dieNo][evictedBlockNo].nextBlock;

			}
			else
			{
				gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
				gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
			}
			return evictedBlockNo;

		}
	}

	assert(!"[WARNING] There are no free blocks. Abort terminate this ssd. [WARNING]");
	return BLOCK_FAIL;
}

unsigned int GetFromGcVictimListNum(unsigned int dieNo)
{
	unsigned int evictedBlockNo;
	int invalidSliceCnt;

	for(invalidSliceCnt = SLICES_PER_BLOCK; invalidSliceCnt > 0 ; invalidSliceCnt--)
	{
		if(gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock != BLOCK_NONE)
		{
			evictedBlockNo = gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock;
			return evictedBlockNo;
		}
	}

	assert(!"[WARNING] There are no free blocks. Abort terminate this ssd. [WARNING]");
	return BLOCK_FAIL;
}

void SelectiveGetFromGcVictimList(unsigned int dieNo, unsigned int blockNo)
{
	unsigned int nextBlock, prevBlock, invalidSliceCnt;

	nextBlock = virtualBlockMapPtr->block[dieNo][blockNo].nextBlock;
	prevBlock = virtualBlockMapPtr->block[dieNo][blockNo].prevBlock;
	invalidSliceCnt = virtualBlockMapPtr->block[dieNo][blockNo].invalidSliceCnt;

	if((nextBlock != BLOCK_NONE) && (prevBlock != BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][prevBlock].nextBlock = nextBlock;
		virtualBlockMapPtr->block[dieNo][nextBlock].prevBlock = prevBlock;
	}
	else if((nextBlock == BLOCK_NONE) && (prevBlock != BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][prevBlock].nextBlock = BLOCK_NONE;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = prevBlock;
	}
	else if((nextBlock != BLOCK_NONE) && (prevBlock == BLOCK_NONE))
	{
		virtualBlockMapPtr->block[dieNo][nextBlock].prevBlock = BLOCK_NONE;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = nextBlock;
	}
	else
	{
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].headBlock = BLOCK_NONE;
		gcVictimMapPtr->gcVictimList[dieNo][invalidSliceCnt].tailBlock = BLOCK_NONE;
	}
}

