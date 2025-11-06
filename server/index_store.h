#ifndef INDEX_STORE_H
#define INDEX_STORE_H

#include <stddef.h>
#include "../common_utils/protocol.h"   
#include "../common_utils/file_hasher.h"

typedef struct {
    char filename[MAX_PATH_LEN];
    size_t filesize;
    int nblocks;
    block_sig_t *sigs;
} file_index_t;

int save_all_indices(const char *path, const file_index_t *indices, int count);
file_index_t *load_all_indices(const char *path, int *out_count);
void free_indices(file_index_t *indices, int count);
file_index_t *find_index_by_name(file_index_t *indices, int count, const char *filename);
int replace_or_add_index(file_index_t **indices_ptr, int *count_ptr, const file_index_t *newidx);

#endif
