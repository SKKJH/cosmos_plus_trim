//////////////////////////////////////////////////////////////////////////////////
// nvme_io_cmd.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
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
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//			 Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe IO Command Handler
// File Name: nvme_io_cmd.c
//
// Version: v1.0.1
//
// Description:
//   - handles NVMe IO command
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.1
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include "debug.h"
#include "io_access.h"

#include "nvme.h"
#include "trim.h"
#include "host_lld.h"
#include "nvme_io_cmd.h"

#include "../data_buffer.h"
#include "../ftl_config.h"
#include "../request_transform.h"
#include "../memory_map.h"

void handle_nvme_io_dataset_management(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	unsigned int ad;
	IO_DATASET_MANAGEMENT_COMMAND_DW10 dsmInfo10;
	IO_DATASET_MANAGEMENT_COMMAND_DW11 dsmInfo11;

	dsmInfo10.dword = nvmeIOCmd->dword10;
	dsmInfo11.dword = nvmeIOCmd->dword11;
	dsm_req += 1;
	unsigned int nr = dsmInfo10.NR + 1;

	ad = dsmInfo11.AD;

	if (ad==1)
		ReqTransNvmeToSliceForDSM(cmdSlotTag, nr);
	else
		xil_printf("No Discard\r\n");
}

void handle_nvme_io_read(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	IO_READ_COMMAND_DW12 readInfo12;
	//IO_READ_COMMAND_DW13 readInfo13;
	//IO_READ_COMMAND_DW15 readInfo15;
	unsigned int startLba[2];
	unsigned int nlb;

	readInfo12.dword = nvmeIOCmd->dword[12];
	//readInfo13.dword = nvmeIOCmd->dword[13];
	//readInfo15.dword = nvmeIOCmd->dword[15];

	startLba[0] = nvmeIOCmd->dword[10];
	startLba[1] = nvmeIOCmd->dword[11];
	nlb = readInfo12.NLB;

	if ((nlb == 77) && (startLba[0] == 77))
	{
		unsigned int util=
				100 * (((real_write_cnt) * 16) / 1024) /
				(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));

		xil_printf("=========================================================\r\n");
		xil_printf("parameter information\r\n");
		xil_printf("write_cnt: %u, gc_cnt: %u, gc copy cnt: %u, dsm_req: %u\n", write_cnt, gc_cnt, gc_page_copy, dsm_req);
		xil_printf("requested trim cnt: %u, async performed block cnt: %u, async buffered block cnt: %u, sync performed block cnt: %u, GC performed block cnt: %u real trimmed block: %u, err cnt: %u\n", trim_cnt, async_trim_cnt, async_req_blcok, sync_trim_cnt, gc_trim_cnt, realtrimmedRange, err);
		xil_printf("regression train cnt: %u, fallback train cnt: %u\n", train_cnt, fallback_cnt);
		xil_printf("regression return cnt: %u, fallback return cnt: %u\n", return_rg, return_fb);
		xil_printf("utilization: %d Percent, allocate_full_cnt: %u, async_trim_buf: %u, sync_trim_buf: %u\n", util, allocate_full_cnt, async_trim_buf , sync_trim_buf);
		xil_printf("WRITE_ERR: %u, READ_ERR: %u \r\n", WRITE_ERR, READ_ERR);
		xil_printf("parameter initialized\r\n");

		xil_printf("idle trim cnt: %u, gc trim cnt: %u, done return: %u, force return: %u\r\n", asynctrim, gctrim, bufnone, forcereturn);
		xil_printf("Perfom Dealloc total_cycles: %u, write_invalidate_ov: %u, trim_invalid_cnt: %u\r\n", total_us, total_write_us, trim_invalid);
		xil_printf("=========================================================\r\n");

		write_cnt = 0;
		total_us = 0;
		WRITE_ERR = 0;
		READ_ERR = 0;
		asynctrim = 0;
		gctrim = 0;
		bufnone = 0;
		forcereturn = 0;
		async_req_blcok = 0;
		trim_cnt = 0;
		dsm_req = 0;
		async_trim_cnt = 0;
		sync_trim_buf = 0;
		async_trim_buf = 0;
		sync_trim_cnt = 0;
		trim_invalid = 0;
		total_write_us = 0;
		gc_trim_cnt = 0;
		gc_cnt = 0;
		realtrimmedRange = 0;
		gc_page_copy = 0;
		train_cnt = 0;
		fallback_cnt = 0;
		return_fb = 0;
		return_rg = 0;
		err = 0;

		if (pcheck != 1)
			pcheck = 1;

		if (pcheck == 1)
			g_time_cnt = 2000000000;

		tb_start += 1;
	}

	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((nvmeIOCmd->PRP1[0] & 0x3) == 0 && (nvmeIOCmd->PRP2[0] & 0x3) == 0); //error
	ASSERT(nvmeIOCmd->PRP1[1] < 0x10000 && nvmeIOCmd->PRP2[1] < 0x10000);

	ReqTransNvmeToSlice(cmdSlotTag, startLba[0], nlb, IO_NVM_READ);
}


void handle_nvme_io_write(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	IO_READ_COMMAND_DW12 writeInfo12;
	//IO_READ_COMMAND_DW13 writeInfo13;
	//IO_READ_COMMAND_DW15 writeInfo15;
	unsigned int startLba[2];
	unsigned int nlb;

	writeInfo12.dword = nvmeIOCmd->dword[12];
	//writeInfo13.dword = nvmeIOCmd->dword[13];
	//writeInfo15.dword = nvmeIOCmd->dword[15];

	//if(writeInfo12.FUA == 1)
	//	xil_printf("write FUA\r\n");

	startLba[0] = nvmeIOCmd->dword[10];
	startLba[1] = nvmeIOCmd->dword[11];
	nlb = writeInfo12.NLB;

	ASSERT(startLba[0] < storageCapacity_L && (startLba[1] < STORAGE_CAPACITY_H || startLba[1] == 0));
	//ASSERT(nlb < MAX_NUM_OF_NLB);
	ASSERT((nvmeIOCmd->PRP1[0] & 0xF) == 0 && (nvmeIOCmd->PRP2[0] & 0xF) == 0);
	ASSERT(nvmeIOCmd->PRP1[1] < 0x10000 && nvmeIOCmd->PRP2[1] < 0x10000);

	ReqTransNvmeToSlice(cmdSlotTag, startLba[0], nlb, IO_NVM_WRITE);
}

void handle_asyncTrim(unsigned int forced, unsigned int Range)
{
    if (forced == 0)
        asynctrim++;
    else
        gctrim++;

    int blk0, blk1, blk2, blk3;
    int nlb, slba, bufEntry, hashIndex;
    unsigned int nextEntry;
    unsigned int trimmedRange = 0;
    int tcheck = 0;

    Range = Range / 4;
    bufEntry = DATA_BUF_NONE;

    // 가장 높은 해시 인덱스부터 유효한 DSM 범위를 찾음
    for (int j = 32; j >= 0; j--) {
        if (dsmRangeHashTable->Range_Flag[j] == 1) {
            bufEntry = dsmRangeHashTable->dsmRangeHash[j].headEntry;
            hashIndex = j;
            break;
        }
    }

    while (bufEntry != DATA_BUF_NONE) {
        slba = dsmRangePtr->dsmRange[bufEntry].startingLBA[0];
        nlb  = dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks;
//        xil_printf("handle_asyncTrim: bufEntry: %u, slba: %u, nlb: %u\r\n", bufEntry, slba, nlb);

        nextEntry = dsmRangePtr->dsmRange[bufEntry].hashNextEntry;

        if ((nlb > 0) && (slba < (SLICES_PER_SSD * 4)) && (nlb < (SLICES_PER_SSD * 4))) {
            int mod = slba % 4;
            int partial_len = (mod == 0) ? 0 : 4 - mod;
            if (partial_len > nlb) partial_len = nlb;

            // 1. 처음 unaligned 영역 처리
            if (partial_len > 0) {
                set_trim_mask(mod, partial_len, &blk0, &blk1, &blk2, &blk3);
                TRIM(slba, blk0, blk1, blk2, blk3, 1);
                tcheck = 1;
                trimmedRange += partial_len;
                force_trim_size += partial_len;
                slba += partial_len;
                nlb  -= partial_len;
            }

            // 2. 정렬된 full TRIM 처리
            while (nlb >= 4) {
                TRIM(slba, 0, 0, 0, 0, 1);
                tcheck = 1;
                trimmedRange += 4;
                force_trim_size += 4;
                slba += 4;
                nlb  -= 4;

                if (forced == 1 && trimmedRange > Range) {
                    trimmedRange = 0;
                    dsmRangePtr->dsmRange[bufEntry].startingLBA[0] = slba;
                    dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks = nlb;
                    dsmRangePtr->dsmRange[bufEntry].flag = 1;
                    forcereturn++;
                    return;
                }

                if (forced == 0)
                	if (check_nvme_cmd_come()) {
//                		xil_printf("new i/o come\r\n");
                		dsmRangePtr->dsmRange[bufEntry].startingLBA[0] = slba;
                		dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks = nlb;
                		return;
                	}
            }
            // 3. 남은 tail 영역 처리
            if (nlb > 0) {
                set_trim_mask(0, nlb, &blk0, &blk1, &blk2, &blk3);
                TRIM(slba, blk0, blk1, blk2, blk3, 1);
                tcheck = 1;
                trimmedRange += nlb;
                force_trim_size += nlb;
            }
        }

        // 통계 업데이트
        if (forced == 0)
            async_trim_cnt += dsmRangePtr->dsmRange[bufEntry].RealLB;
        else
            gc_trim_cnt += dsmRangePtr->dsmRange[bufEntry].RealLB;

        // 해시 리스트에서 제거
        SelectiveGetFromDsmRangeHashList(bufEntry);

        // 다음 entry 탐색
        if ((hashIndex == 0) && (dsmRangeHashTable->dsmRangeHash[hashIndex].headEntry == DATA_BUF_NONE)) {
            bufEntry = DATA_BUF_NONE;
        } else if (nextEntry == DATA_BUF_NONE) {
            bufEntry = DATA_BUF_NONE;
            for (int j = hashIndex; j >= 0; j--) {
                if (dsmRangeHashTable->Range_Flag[j] == 1) {
                    bufEntry = dsmRangeHashTable->dsmRangeHash[j].headEntry;
                    hashIndex = j;
                    break;
                }
            }
        } else {
            bufEntry = nextEntry;
        }
    }

    do_trim_flag = 0;

    if (tcheck == 1) {
        for (int i = 0; i < BITMAP_SIZE; i++)
            asyncTrimBitMapPtr->writeBitMap[i] = 0ULL;
    }

    bufnone += 1;
    allocate_full_cnt = 0;
}

//void handle_asyncTrim(unsigned int forced, unsigned int Range)
//{
//	if(forced == 0)
//		asynctrim++;
//	else
//		gctrim++;
//
//	int blk0, blk1, blk2, blk3, tcheck;
//    tcheck = 0;
//    int tempSlba, tempNlb;
//    int nlb, slba, bufEntry, hashIndex;
//    unsigned int nextEntry;
//    unsigned int trimmedRange = 0;
//    Range = Range * 4000;
//    bufEntry = DATA_BUF_NONE;
//
//    for (int j = 32; j >= 0; j--)
//    {
//        if (dsmRangeHashTable->Range_Flag[j] == 1)
//        {
//            bufEntry = dsmRangeHashTable->dsmRangeHash[j].headEntry;
//            hashIndex = j;
//            break;
//        }
//    }
//
//    while (bufEntry != DATA_BUF_NONE)
//    {
//        slba = dsmRangePtr->dsmRange[bufEntry].startingLBA[0];
//        nlb  = dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks;
//        xil_printf("bufEntry: %u, handle_asyncTrim slba: %u, nlb: %u\r\n", bufEntry, slba, nlb);
//
//        nextEntry = dsmRangePtr->dsmRange[bufEntry].hashNextEntry;
//        blk0 = blk1 = blk2 = blk3 = 1;
//
//        if ((nlb > 0) &&
//            (slba >= 0) &&
//            (nlb < (SLICES_PER_SSD * 4)) &&
//            (slba < (SLICES_PER_SSD * 4)))
//        {
//            int partialTrimCount = 0;
//            switch (slba % 4)
//            {
//                case 0:
//                    if (nlb == 1) {
//                        blk0 = 0;
//                        partialTrimCount = 1;
//                    }
//                    else if (nlb == 2) {
//                        blk0 = 0;
//                        blk1 = 0;
//                        partialTrimCount = 2;
//                    }
//                    else if (nlb == 3) {
//                        blk0 = 0;
//                        blk1 = 0;
//                        blk2 = 0;
//                        partialTrimCount = 3;
//                    }
//                    else {
//                        blk0 = 0;
//                        blk1 = 0;
//                        blk2 = 0;
//                        blk3 = 0;
//                        partialTrimCount = 4;
//                    }
//                    break;
//
//                case 1:
//                    if (nlb == 1) {
//                        blk1 = 0;
//                        partialTrimCount = 1;
//                    }
//                    else if (nlb == 2) {
//                        blk1 = 0;
//                        blk2 = 0;
//                        partialTrimCount = 2;
//                    }
//                    else {
//                        blk1 = 0;
//                        blk2 = 0;
//                        blk3 = 0;
//                        partialTrimCount = 3;
//                    }
//                    break;
//
//                case 2:
//                    if (nlb == 1) {
//                        blk2 = 0;
//                        partialTrimCount = 1;
//                    }
//                    else {
//                        blk2 = 0;
//                        blk3 = 0;
//                        partialTrimCount = 2;
//                    }
//                    break;
//
//                case 3:
//                    blk3 = 0;      // nlb >= 1이면 blk3 한 개만 해제
//                    partialTrimCount = 1;
//                    break;
//            }
//            trimmedRange += partialTrimCount;
//            TRIM(slba, blk0, blk1, blk2, blk3, 1);
//            tcheck = 1;
//            tempSlba = slba + (4 - (slba % 4));
//            tempNlb  = nlb  - (4 - (slba % 4));
//            slba     = tempSlba;
//            nlb      = tempNlb;
//
//            while (nlb > 4)
//            {
//                TRIM(slba, 0, 0, 0, 0, 1);
//                tcheck = 1;
//                trimmedRange += 4;
//                slba += 4;
//                nlb  -= 4;
//
//                if ((forced == 1) && (trimmedRange > Range))
//                {
//                    trimmedRange = 0;
//                    dsmRangePtr->dsmRange[bufEntry].startingLBA[0] = slba;
//                    dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks = nlb;
//                    dsmRangePtr->dsmRange[bufEntry].flag = 1;
//                    forcereturn++;
//                    return;
//                }
//
//                if ((forced == 0) && (g_time_cnt != 0))
//                {
//                    cmd_by_trim = check_nvme_cmd_come();
//                    if (cmd_by_trim == 1)
//                    {
//                    	xil_printf("new i/o come\r\n");
//
//                        trimmedRange = 0;
//                        dsmRangePtr->dsmRange[bufEntry].startingLBA[0] = slba;
//                        dsmRangePtr->dsmRange[bufEntry].lengthInLogicalBlocks = nlb;
//                        return;
//                    }
//                }
//            }
//
//            blk0 = blk1 = blk2 = blk3 = 1;
//            switch (nlb)
//            {
//                case 1:
//                    blk0 = 0;
//                    break;
//
//                case 2:
//                    blk0 = 0;
//                    blk1 = 0;
//                    break;
//
//                case 3:
//                    blk0 = 0;
//                    blk1 = 0;
//                    blk2 = 0;
//                    break;
//
//                case 4:
//                    blk0 = 0;
//                    blk1 = 0;
//                    blk2 = 0;
//                    blk3 = 0;
//                    break;
//            }
//            if (nlb > 0)
//            {
//                TRIM(slba, blk0, blk1, blk2, blk3, 1);
//				tcheck = 1;
//            }
//        }
//        if(forced == 0)
//        {
////        	asynctrim++;
//        	async_trim_cnt += dsmRangePtr->dsmRange[bufEntry].RealLB;
//        }
//        else
//        {
////        	gctrim++;
//    		gc_trim_cnt += dsmRangePtr->dsmRange[bufEntry].RealLB;
//        }
//
//        SelectiveGetFromDsmRangeHashList(bufEntry);
//
//        if ((hashIndex == 0) &&
//            (dsmRangeHashTable->dsmRangeHash[hashIndex].headEntry == DATA_BUF_NONE))
//        {
//            bufEntry = DATA_BUF_NONE;
//        }
//        else
//        {
//            if (nextEntry == DATA_BUF_NONE)
//            {
//                for (int j = hashIndex; j >= 0; j--)
//                {
//                    if (dsmRangeHashTable->Range_Flag[j] == 1)
//                    {
//                        bufEntry  = dsmRangeHashTable->dsmRangeHash[j].headEntry;
//                        hashIndex = j;
//                        break;
//                    }
//                    bufEntry = DATA_BUF_NONE;
//                }
//            }
//            else
//            {
//                bufEntry = nextEntry;
//            }
//        }
//    }
//
//    do_trim_flag = 0;
//
//    if (tcheck == 1)
//    {
////    	for(int sliceAddr=0; sliceAddr<SLICES_PER_SSD ; sliceAddr++)
////    		logicalSliceMapPtr->logicalSlice[sliceAddr].Trim_Write = 0;
//    	for (int i = 0; i < BITMAP_SIZE; i++)
//    		asyncTrimBitMapPtr->writeBitMap[i] = 0ULL;
//    }
//    bufnone += 1;
//    allocate_full_cnt = 0;
//}

void handle_nvme_io_cmd(NVME_COMMAND *nvmeCmd)
{
	NVME_IO_COMMAND *nvmeIOCmd;
	NVME_COMPLETION nvmeCPL;
	unsigned int opc;
	nvmeIOCmd = (NVME_IO_COMMAND*)nvmeCmd->cmdDword;
	/*		xil_printf("OPC = 0x%X\r\n", nvmeIOCmd->OPC);
			xil_printf("PRP1[63:32] = 0x%X, PRP1[31:0] = 0x%X\r\n", nvmeIOCmd->PRP1[1], nvmeIOCmd->PRP1[0]);
			xil_printf("PRP2[63:32] = 0x%X, PRP2[31:0] = 0x%X\r\n", nvmeIOCmd->PRP2[1], nvmeIOCmd->PRP2[0]);
			xil_printf("dword10 = 0x%X\r\n", nvmeIOCmd->dword10);
			xil_printf("dword11 = 0x%X\r\n", nvmeIOCmd->dword11);
			xil_printf("dword12 = 0x%X\r\n", nvmeIOCmd->dword12);*/

	opc = (unsigned int)nvmeIOCmd->OPC;

	switch(opc)
	{
		case IO_NVM_FLUSH:
		{
		//	xil_printf("IO Flush Command\r\n");
			nvmeCPL.dword[0] = 0;
			nvmeCPL.specific = 0x0;
			set_auto_nvme_cpl(nvmeCmd->cmdSlotTag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
			break;
		}
		case IO_NVM_WRITE:
		{
			time_cnt = 0;
//			if(do_trim_flag != 0)
//				xil_printf("IO Write Command\r\n");

			handle_nvme_io_write(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		case IO_NVM_READ:
		{
//			time_cnt = 0;
//			xil_printf("IO Read Command\r\n");
			handle_nvme_io_read(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		case IO_NVM_DATASET_MANAGEMENT:
		{
//			xil_printf("IO Dataset Management Command\r\n");
			handle_nvme_io_dataset_management(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		case IO_NVM_WRITE_ZEROS:
		{
//			xil_printf("IO Write Zeros Command\r\n");
			nvmeCPL.dword[0] = 0;
			nvmeCPL.specific = 0x0;
			set_auto_nvme_cpl(nvmeCmd->cmdSlotTag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
			break;
		}
		default:
		{
			xil_printf("Not Support IO Command OPC: %X\r\n", opc);
			ASSERT(0);
			break;
		}
	}
}
