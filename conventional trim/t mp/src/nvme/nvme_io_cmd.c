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
#include "host_lld.h"
#include "nvme_io_cmd.h"

#include "../ftl_config.h"
#include "../request_transform.h"

void handle_nvme_io_dataset_management(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	unsigned int ad;
	IO_DATASET_MANAGEMENT_COMMAND_DW10 dsmInfo10;
	IO_DATASET_MANAGEMENT_COMMAND_DW11 dsmInfo11;

	dsmInfo10.dword = nvmeIOCmd->dword10;
	dsmInfo11.dword = nvmeIOCmd->dword11;
	unsigned int nr = dsmInfo10.NR + 1;

	ad = dsmInfo11.AD;

	if (ad==1)
	{
//		xil_printf("NR: %u\r\n", nr);
		ReqTransNvmeToSliceForDSM(cmdSlotTag, nr);
	}
	else
	{
		xil_printf("No Discard\r\n");
	}
}

void handle_nvme_io_read(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	IO_READ_COMMAND_DW12 readInfo12;
	//IO_READ_COMMAND_DW13 readInfo13;
	//IO_READ_COMMAND_DW15 readInfo15;
	unsigned int startLba[2];
	unsigned int nlb, utilization;

	readInfo12.dword = nvmeIOCmd->dword[12];
	//readInfo13.dword = nvmeIOCmd->dword[13];
	//readInfo15.dword = nvmeIOCmd->dword[15];

	startLba[0] = nvmeIOCmd->dword[10];
	startLba[1] = nvmeIOCmd->dword[11];
	nlb = readInfo12.NLB;

	if ((nlb == 77) && (startLba[0] == 77))
	{
		xil_printf("=========================================================\r\n");
		xil_printf("TASK SHUTDOWN\r\n");
		xil_printf("write_cnt: %u, invalid_cnt: %u, gc_cnt: %u, nlb_sum: %u, trim_call_cnt: %u, ", write_cnt, trim_cnt, gc_cnt, nlb_sum, trim_call_cnt);
		xil_printf("gc copy cnt: %u, err: %u\n", gc_page_copy, err);
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
		profile = 1;
	}
	else if ((nlb == 88) && (startLba[0] == 88))
	{
		xil_printf("=========================================================\r\n");
		xil_printf("TASK SHUTDOWN\r\n");
		xil_printf("write_cnt: %u, invalid_cnt: %u, gc_cnt: %u, nlb_sum: %u, trim_call_cnt: %u, ", write_cnt, trim_cnt, gc_cnt, nlb_sum, trim_call_cnt);
		xil_printf("gc copy cnt: %u, err: %u\n", gc_page_copy, err);
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
		profile = 1;
	}
	else if ((nlb == 99) && (startLba[0] == 99))
	{
		xil_printf("=========================================================\r\n");
		xil_printf("TASK SHUTDOWN\r\n");
		xil_printf("write_cnt: %u, invalid_cnt: %u, gc_cnt: %u, nlb_sum: %u, trim_call_cnt: %u, ", write_cnt, trim_cnt, gc_cnt, nlb_sum, trim_call_cnt);
		xil_printf("gc copy cnt: %u, err: %u\n", gc_page_copy, err);
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
		profile = 1;
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
//			xil_printf("IO Flush Command\r\n");
			nvmeCPL.dword[0] = 0;
			nvmeCPL.specific = 0x0;
			set_auto_nvme_cpl(nvmeCmd->cmdSlotTag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
			break;
		}
		case IO_NVM_WRITE:
		{
//			xil_printf("IO Write Command\r\n");
			handle_nvme_io_write(nvmeCmd->cmdSlotTag, nvmeIOCmd);
			break;
		}
		case IO_NVM_READ:
		{
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

