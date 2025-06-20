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

void nvme_main()
{
	XTime tStart, tEnd;
	XTime tTime;
	unsigned int exeLlr;
	unsigned int rstCnt = 0;
	utrim = u70 = u75 = u80 = u85 = u90 = u95 = 0;

	time_cnt = 0;
	dsmCount = 0;
	g_time_cnt = 0;
	trim_invalid = 0;
	real_write_cnt = 0;
	do_trim_flag = 0;
	trim_flag = 0;
	total_us = 0;
	total_write_us = 0;

	gc_cnt = 0;
	write_cnt = 0;
	dsm_req = 0;
	READ_ERR = 0;
	WRITE_ERR = 0;
	trim_cnt = 0;
	realtrimmedRange = 0;
	async_trim_cnt = 0;
	sync_trim_cnt = 0;
	gc_trim_cnt = 0;
	gc_page_copy = 0;
	pcheck = 0;

	cmd_by_trim = 0;
	asynctrim = 0;
	gctrim = 0;
	bufnone = 0;
	forcereturn = 0;
	trim_perf = 0;

	train_cnt = 0;
	train_init = 0;
	fallback_cnt = 0;
	train_thres = 0;
	tb_start = -1;
	async_req_blcok = 0;
	tcheck = 0;

	return_rg = 0;
	return_fb = 0;

	async_trim_buf = 0;
	sync_trim_buf = 0;

	allocate_full_cnt = 0;

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
			{
				rstCnt = 0;
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
			unsigned int util=
					100 * (((real_write_cnt) * 16) / 1024) /
					(storageCapacity_L / ((1024 * 1024) / BYTES_PER_NVME_BLOCK));

			xil_printf("=========================================================\r\n");
			xil_printf("parameter information\r\n");
			xil_printf("write_cnt: %u, gc_cnt: %u, gc copy cnt: %u\n", write_cnt, gc_cnt, gc_page_copy);
			xil_printf("requested trim cnt: %u, async performed block cnt: %u\n", trim_cnt, async_trim_cnt);
			xil_printf("sync performed block cnt: %u, GC performed block cnt: %u real trimmed block: %u, err cnt: %u\n", sync_trim_cnt, gc_trim_cnt, realtrimmedRange, err);
			xil_printf("regression train cnt: %u, fallback train cnt: %u\n", train_cnt, fallback_cnt);
			xil_printf("regression return cnt: %u, fallback return cnt: %u\n", return_rg, return_fb);
			xil_printf("utilization: %d Percent, allocate_full_cnt: %u, async_trim_buf: %u, sync_trim_buf: %u\n", util, allocate_full_cnt, async_trim_buf , sync_trim_buf);
			xil_printf("parameter initialized\r\n");

			xil_printf("idle trim cnt: %u, gc trim cnt: %u, done return: %u, force return: %u\r\n", asynctrim, gctrim, bufnone, forcereturn);
			xil_printf("Perfom Dealloc total_us: %u, write_invalidate_ov: %u, trim_invalid_cnt: %u\r\n", total_us, total_write_us, trim_invalid);
			xil_printf("=========================================================\r\n");

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

		if(do_trim_flag != 0)
		{
			time_cnt++;
			if (time_cnt > g_time_cnt)
			{
				time_cnt = 0;
//				handle_asyncTrim(0,0);
			}
		}
	}
}


