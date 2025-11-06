#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index_store.h"

int save_all_indices(const char *path, const file_index_t *indices, int count) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { perror("fopen tmp"); return -1; }

    fwrite(&count, sizeof(int), 1, f);
    for (int i = 0; i < count; i++) {
        fwrite(indices[i].filename, sizeof(indices[i].filename), 1, f);
        fwrite(&indices[i].filesize, sizeof(indices[i].filesize), 1, f);
        fwrite(&indices[i].nblocks, sizeof(indices[i].nblocks), 1, f);
        fwrite(indices[i].sigs, sizeof(block_sig_t), indices[i].nblocks, f);
    }

    fclose(f);
    rename(tmp, path);
    return 0;
}

file_index_t *load_all_indices(const char *path, int *out_count) {
    FILE *f = fopen(path, "rb");
    if (!f) { *out_count = 0; return NULL; }

    int count = 0;
    if (fread(&count, sizeof(int), 1, f) != 1) {
        fclose(f);
        *out_count = 0;
        return NULL;
    }

    file_index_t *arr = calloc(count, sizeof(file_index_t));
    if (!arr) { fclose(f); *out_count = 0; return NULL; }

    for (int i = 0; i < count; i++) {
        fread(arr[i].filename, sizeof(arr[i].filename), 1, f);
        fread(&arr[i].filesize, sizeof(arr[i].filesize), 1, f);
        fread(&arr[i].nblocks, sizeof(arr[i].nblocks), 1, f);
        arr[i].sigs = malloc(sizeof(block_sig_t) * arr[i].nblocks);
        fread(arr[i].sigs, sizeof(block_sig_t), arr[i].nblocks, f);
    }

    fclose(f);
    *out_count = count;
    return arr;
}

void free_indices(file_index_t *indices, int count) {
    if (!indices) return;
    for (int i = 0; i < count; i++)
        if (indices[i].sigs) free(indices[i].sigs);
    free(indices);
}

file_index_t *find_index_by_name(file_index_t *indices, int count, const char *filename) {
    for (int i = 0; i < count; i++) {
        if (strcmp(indices[i].filename, filename) == 0)
            return &indices[i];
    }
    return NULL;
}

int replace_or_add_index(file_index_t **indices_ptr, int *count_ptr, const file_index_t *newidx) {
    file_index_t *indices = *indices_ptr;
    int count = *count_ptr;

    for (int i = 0; i < count; i++) {
        if (strcmp(indices[i].filename, newidx->filename) == 0) {
            if (indices[i].sigs) free(indices[i].sigs);
            indices[i].sigs = malloc(sizeof(block_sig_t) * newidx->nblocks);
            memcpy(indices[i].sigs, newidx->sigs, sizeof(block_sig_t) * newidx->nblocks);
            indices[i].filesize = newidx->filesize;
            indices[i].nblocks = newidx->nblocks;
            return 0;
        }
    }

    file_index_t *tmp = realloc(indices, sizeof(file_index_t) * (count + 1));
    if (!tmp) return -1;

    indices = tmp;
    indices[count].sigs = malloc(sizeof(block_sig_t) * newidx->nblocks);
    memcpy(indices[count].sigs, newidx->sigs, sizeof(block_sig_t) * newidx->nblocks);
    strncpy(indices[count].filename, newidx->filename, MAX_PATH_LEN - 1);
    indices[count].filesize = newidx->filesize;
    indices[count].nblocks = newidx->nblocks;

    *indices_ptr = indices;
    *count_ptr = count + 1;
    return 0;
}
