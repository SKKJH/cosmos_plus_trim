/*
 * trim.h
 *
 *  Created on: 2025. 5. 12.
 *      Author: User
 */

#ifndef SRC_NVME_TRIM_H_
#define SRC_NVME_TRIM_H_

void set_trim_mask(unsigned int mod, unsigned int partial_len, int* blk0, int* blk1, int* blk2, int* blk3);


#endif /* SRC_NVME_TRIM_H_ */
