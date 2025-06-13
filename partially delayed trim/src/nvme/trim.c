/*
 * trim.c
 *
 *  Created on: 2025. 5. 12.
 *      Author: User
 */


void set_trim_mask(unsigned int mod, unsigned int partial_len, int* blk0, int* blk1, int* blk2, int* blk3)
{
    *blk0 = *blk1 = *blk2 = *blk3 = 1;

    switch (mod) {
        case 0:
            switch (partial_len) {
                case 1: *blk0 = 0; break;
                case 2: *blk0 = *blk1 = 0; break;
                case 3: *blk0 = *blk1 = *blk2 = 0; break;
                case 4: *blk0 = *blk1 = *blk2 = *blk3 = 0; break;
            }
            break;
        case 1:
            switch (partial_len) {
                case 1: *blk1 = 0; break;
                case 2: *blk1 = *blk2 = 0; break;
                case 3: *blk1 = *blk2 = *blk3 = 0; break;
            }
            break;
        case 2:
            switch (partial_len) {
                case 1: *blk2 = 0; break;
                case 2: *blk2 = *blk3 = 0; break;
            }
            break;
        case 3:
            *blk3 = 0;
            break;
    }
}
