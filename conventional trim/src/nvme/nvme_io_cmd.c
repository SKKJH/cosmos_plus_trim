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
#include "../data_buffer.h"

#include "../ftl_config.h"
#include "../request_transform.h"

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

void handle_nvme_io_dataset_management(unsigned int cmdSlotTag, NVME_IO_COMMAND *nvmeIOCmd)
{
	unsigned int ad;
	IO_DATASET_MANAGEMENT_COMMAND_DW10 dsmInfo10;
	IO_DATASET_MANAGEMENT_COMMAND_DW11 dsmInfo11;

	dsmInfo10.dword = nvmeIOCmd->dword10;
	dsmInfo11.dword = nvmeIOCmd->dword11;
	trimDmaCnt++;
	unsigned int nr = dsmInfo10.NR + 1;
//	xil_printf("Request here, num of range: %d\r\n", nr);
	ad = dsmInfo11.AD;

	if (ad==1)
	{
		ReqTransNvmeToSliceForDSM(cmdSlotTag, nr);
	}
	else
	{
		xil_printf("No Discard\r\n");
	}
}

void handle_asyncTrim(int forced)
{
	int blk0, blk1, blk2, blk3, tempSlba , tempNlb;
	int head, nlb, slba, temp;

	if ((forced == 1) && (nr_sum > 10))
		temp = 10;
	else
		temp = nr_sum;

	for (int i=0; i<temp; i++)
	{
		head = dsmRangePtr->head;
		slba = dsmRangePtr->dmRange[head].startingLBA[0];
		nlb = dsmRangePtr->dmRange[head].lengthInLogicalBlocks;
		blk0 = 1;
		blk1 = 1;
		blk2 = 1;
		blk3 = 1;

//		xil_printf("nr_sum : %d\r\n",nr_sum);
//		xil_printf("nlb : %d\r\n",nlb);
//		xil_printf("slba : %d\r\n",slba);

		if((nlb>0)&&(slba>=0)&&(nlb<(SLICES_PER_SSD * 4))&&(slba<(SLICES_PER_SSD * 4)))
		{
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

			if (forced == 0)
			{
				cmd_by_trim = check_nvme_cmd_come();
				if(cmd_by_trim == 1)
				{
//					xil_printf("new io cmd requested\r\n");
					return;
				}
			}
		}
		dsmRangePtr->head = (dsmRangePtr->head + 1)%3000;
		nr_sum--;
	}

	trim_flag = 0;
	nr_sum = 0;
	dsmRangePtr->head = 0;
	dsmRangePtr->tail = 0;
}

void handle_nvme_io_cmd(NVME_COMMAND *nvmeCmd)
{
	NVME_IO_COMMAND *nvmeIOCmd;
	NVME_COMPLETION nvmeCPL;
	unsigned int opc;
	nvmeIOCmd = (NVME_IO_COMMAND*)nvmeCmd->cmdDword;

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
		default:
		{
			xil_printf("Not Support IO Command OPC: %X\r\n", opc);
			ASSERT(0);
			break;
		}
	}
}

