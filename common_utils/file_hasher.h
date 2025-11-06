#ifndef FILE_HASHER_H
#define FILE_HASHER_H

#include <stdint.h>
#include<stdio.h>
#include <stddef.h>     
#include "protocol.h"

uint32_t rsync_weak_checksum(const unsigned char *buf, size_t len);
void compute_sigs_for_file(FILE *f, block_sig_t *sigs, int nblocks, size_t file_size);
void rsync_roll_checksum(uint32_t *a_out, uint32_t *b_out,
                         const unsigned char *old_block,
                         const unsigned char *new_block,
                         size_t block_size, size_t offset);

void md5_hash(const unsigned char *buf, size_t len, unsigned char out16[16]);

#endif

