//////////////////////////////////////////////////////////////////////////////////
// nvme_main.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//				  Kibin Park <kbpark@enc.hanyang.ac.kr>
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
//			 Kibin Park <kbpark@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe Main
// File Name: nvme_main.c
//
// Version: v1.2.0
//
// Description:
//   - initializes FTL and NAND
//   - handles NVMe controller
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.2.0
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//   - Low level scheduler execution is allowed when there is no i/o command
//
// * v1.1.0
//   - DMA status initialization is added
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "xil_printf.h"
#include "debug.h"
#include "io_access.h"

#include "nvme.h"
#include "host_lld.h"
#include "nvme_main.h"
#include "nvme_admin_cmd.h"
#include "nvme_io_cmd.h"

#include "../memory_map.h"

volatile NVME_CONTEXT g_nvmeTask;

static XTime tStart, tEnd;

void nvme_main()
{
	dsmCount = 0;
	err = 0;
	unsigned int exeLlr;
	unsigned int rstCnt = 0;
	for (int i=0; i<MAX_UTIL; i++)
		util_count[i] = 0;
	gc_train_cnt = 0;
	sample_count = 0;
	real_write_cnt = 0;
	gc_cnt = 0;
	write_cnt = 0;
	gc_page_copy = 0;
	trim_cnt = 0;
	trim_call_cnt = 0;
	req_cnt = 0;
	nlb_sum = 0;
	profile = 0;

	train_rg = 0;
	train_fb = 0;

	trim_perf = 0;

	train_thres_check = 0;
	train_thres = 0;
	trim_flag = 0;

	xil_printf("!!! Wait until FTL reset complete !!! \r\n");

	InitFTL();

	xil_printf("\r\nFTL reset complete!!! \r\n");
	xil_printf("Turn on the host PC \r\n");

	while(1)
	{
		exeLlr = 1;

		if(g_nvmeTask.status == NVME_TASK_WAIT_CC_EN)
		{
			unsigned int ccEn;
			ccEn = check_nvme_cc_en();
			if(ccEn == 1)
			{
				set_nvme_admin_queue(1, 1, 1);
				set_nvme_csts_rdy(1);
				g_nvmeTask.status = NVME_TASK_RUNNING;
				xil_printf("\r\nNVMe ready!!!\r\n");
			}
		}
		else if(g_nvmeTask.status == NVME_TASK_RUNNING)
		{
			NVME_COMMAND nvmeCmd;
			unsigned int cmdValid;
			cmdValid = get_nvme_cmd(&nvmeCmd.qID, &nvmeCmd.cmdSlotTag, &nvmeCmd.cmdSeqNum, nvmeCmd.cmdDword);
			if(cmdValid == 1)
			{	rstCnt = 0;
				if(nvmeCmd.qID == 0)
				{
					handle_nvme_admin_cmd(&nvmeCmd);
				}
				else
				{
					handle_nvme_io_cmd(&nvmeCmd);
					ReqTransSliceToLowLevel();
					exeLlr=0;
				}
			}
		}
		else if(g_nvmeTask.status == NVME_TASK_SHUTDOWN)
		{

//			u32 diff_high = (u32)(ov_cnt >> 32);
//			u32 diff_low    = (u32)(ov_cnt);
//			xil_printf("Total Overhead: High 0x%08X Low 0x%08X\r\n", diff_high, diff_low);

			xil_printf("=========================================================\r\n");
			xil_printf("TASK SHUTDOWN\r\n");
			xil_printf("write_cnt: %u, invalid_cnt: %u, gc_cnt: %u, nlb_sum: %u, trim_call_cnt: %u, ", write_cnt, trim_cnt, gc_cnt, nlb_sum, trim_call_cnt);
			xil_printf("gc copy cnt: %u\n", gc_page_copy);
			xil_printf("train_rg: %u, train_fb: %u\r\n", train_rg, train_fb);
			int utilization =
					100 * (((real_write_cnt) * 16) / 1024) /
					(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));

			xil_printf("utilization: %d Percent\n", utilization);
			write_cnt = 0;
			nlb_sum = 0;
			trim_cnt = 0;
			gc_cnt = 0;
			gc_page_copy = 0;
			xil_printf("write_cnt and trim_cnt initialized\r\n");
			xil_printf("=========================================================\r\n");
//			xil_printf("Total Reduce GC Copy Cnt: %u\r\n", rd_gc_copy);

			NVME_STATUS_REG nvmeReg;
			nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR);
			if(nvmeReg.ccShn != 0)
			{
				unsigned int qID;
				set_nvme_csts_shst(1);

				for(qID = 0; qID < 8; qID++)
				{
					set_io_cq(qID, 0, 0, 0, 0, 0, 0);
					set_io_sq(qID, 0, 0, 0, 0, 0);
				}

				set_nvme_admin_queue(0, 0, 0);
				g_nvmeTask.cacheEn = 0;
				set_nvme_csts_shst(2);
				g_nvmeTask.status = NVME_TASK_WAIT_RESET;

				//flush grown bad block info
				UpdateBadBlockTableForGrownBadBlock(RESERVED_DATA_BUFFER_BASE_ADDR);
				xil_printf("\r\nNVMe shutdown!!!\r\n");
			}
		}
		else if(g_nvmeTask.status == NVME_TASK_WAIT_RESET)
		{
			unsigned int ccEn;
			ccEn = check_nvme_cc_en();
			if(ccEn == 0)
			{
				g_nvmeTask.cacheEn = 0;
				set_nvme_csts_shst(0);
				set_nvme_csts_rdy(0);
				g_nvmeTask.status = NVME_TASK_IDLE;
				xil_printf("\r\nNVMe disable!!!\r\n");
			}
		}
		else if(g_nvmeTask.status == NVME_TASK_RESET)
		{
			unsigned int qID;
			for(qID = 0; qID < 8; qID++)
			{
				set_io_cq(qID, 0, 0, 0, 0, 0, 0);
				set_io_sq(qID, 0, 0, 0, 0, 0);
			}

			if (rstCnt>= 5){
				pcie_async_reset(rstCnt);
				rstCnt = 0;
				xil_printf("\r\nPcie iink disable!!!\r\n");
				xil_printf("Wait few minute or reconnect the PCIe cable\r\n");
			}
			else
				rstCnt++;

			g_nvmeTask.cacheEn = 0;
			set_nvme_admin_queue(0, 0, 0);
			set_nvme_csts_shst(0);
			set_nvme_csts_rdy(0);
			g_nvmeTask.status = NVME_TASK_IDLE;

			xil_printf("\r\nNVMe reset!!!\r\n");
		}

		if(exeLlr && ((nvmeDmaReqQ.headReq != REQ_SLOT_TAG_NONE) || notCompletedNandReqCnt || blockedReqCnt))
		{
			CheckDoneNvmeDmaReq();
			SchedulingNandReq();
		}

		if (trim_perf == 1)
		{
			for (int i = 0; i < dsmCount; i++)
				PerformDeallocation(reqPoolPtr->dsmReqList[i]);
			dsmCount = 0;
			trim_perf = 0;
		}
	}
}


