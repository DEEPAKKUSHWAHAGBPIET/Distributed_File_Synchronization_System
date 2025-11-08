#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include "../common_utils/protocol.h"
#include "../common_utils/compressor.h"
#include "../common_utils/file_hasher.h"
#include "index_store.h"


#define PORT 9000
#define BACKLOG 10
#define INDEX_FILE "index.db"
#define SYNC_FOLDER "syncedData"

file_index_t *indices = NULL;
int indices_count = 0;
pthread_mutex_t index_lock = PTHREAD_MUTEX_INITIALIZER;

ssize_t read_n(int fd, void *buf, size_t n) {
    char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r <= 0) return r;
        left -= r;
        p += r;
    }
    return n;
}

ssize_t write_n(int fd, const void *buf, size_t n) {
    const char *p = buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) return w;
        left -= w;
        p += w;
    }
    return n;
}

void ensure_folder(const char *folder) {
    struct stat st;
    if (stat(folder, &st) == -1) {
        mkdir(folder, 0755);
    }
}

void handle_client(int client_fd) {
    FILE *outf = NULL;
    char line[4096];
    ssize_t rr = recv(client_fd, line, sizeof(line) - 1, 0);
    if (rr <= 0) { close(client_fd); return; }

    line[rr] = '\0';
    if (strncmp(line, MSG_FILE_HDR, strlen(MSG_FILE_HDR)) != 0) {
        close(client_fd);
        return;
    }

    char fname[MAX_PATH_LEN];
    size_t fsize;
    int nblocks;
    sscanf(line, "FILE_HDR %s %zu %d", fname, &fsize, &nblocks);

    const char *base = strrchr(fname, '/');
    const char *basename = base ? base + 1 : fname;

    char *p = strstr(line, "\n");
    int header_bytes = (p ? (p - line + 1) : rr);
    int remaining = rr - header_bytes;
    block_sig_t *sigs = malloc(sizeof(block_sig_t) * nblocks);
    if (remaining > 0)
        memcpy(sigs, line + header_bytes, remaining);
    if ((int)remaining < (int)(sizeof(block_sig_t) * nblocks)) {
        ssize_t need = sizeof(block_sig_t) * nblocks - remaining;
        read_n(client_fd, ((char *)sigs) + remaining, need);
    }

    printf("Server: file hdr: %s size=%zu nblocks=%d\n", basename, fsize, nblocks);

    pthread_mutex_lock(&index_lock);
    file_index_t *existing = find_index_by_name(indices, indices_count, basename);
    pthread_mutex_unlock(&index_lock);

    int *req = malloc(sizeof(int) * nblocks);
    int req_count = 0;
    for (int i = 0; i < nblocks; i++) {
        int match = 0;
        if (existing && existing->nblocks == nblocks) {
            if (existing->sigs[i].weak == sigs[i].weak &&
                memcmp(existing->sigs[i].strong, sigs[i].strong, 16) == 0)
                match = 1;
        }
        if (!match) req[req_count++] = i;
    }

    char outbuf[8192];
    int pos = snprintf(outbuf, sizeof(outbuf), "BLOCK_REQ %d\n", req_count);
    for (int i = 0; i < req_count; i++)
        pos += snprintf(outbuf + pos, sizeof(outbuf) - pos, "%d ", req[i]);
    pos += snprintf(outbuf + pos, sizeof(outbuf) - pos, "\n");
    write_n(client_fd, outbuf, pos);
    free(req);

    if (req_count > 0) {
        ensure_folder(SYNC_FOLDER);
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", SYNC_FOLDER, basename);
        outf = fopen(path, "r+b");
        if (!outf) outf = fopen(path, "wb");
    } else {
        outf = NULL;
        printf("No blocks requested; file up-to-date.\n");
    }

    while (1) {
        char hdr[256];
        int hpos = 0;
        char ch;
        while (read(client_fd, &ch, 1) == 1) {
            hdr[hpos++] = ch;
            if (ch == '\n' || hpos >= sizeof(hdr) - 1)
                break;
        }
        hdr[hpos] = '\0';
        if (strncmp(hdr, "BLOCK_END", 9) == 0) break;

        int idx = -1, c_len = 0, orig_len = 0;
        sscanf(hdr, "BLOCK_DATA %d %d %d", &idx, &c_len, &orig_len);

        unsigned char *cbuf = malloc(c_len);
        read_n(client_fd, cbuf, c_len);

        unsigned char *blockbuf = NULL;
        int dec_len = decompress_block(cbuf, c_len, &blockbuf, orig_len);
        free(cbuf);

        if (outf) {
            fseek(outf, (long)idx * BLOCK_SIZE, SEEK_SET);
            fwrite(blockbuf, 1, dec_len, outf);
        }
        free(blockbuf);
    }

    if (outf) {
        fflush(outf);
        fclose(outf);
    }

    file_index_t newidx;
    memset(&newidx, 0, sizeof(newidx));
    strncpy(newidx.filename, basename, MAX_PATH_LEN - 1);
    newidx.filesize = fsize;
    newidx.nblocks = nblocks;
    newidx.sigs = sigs;

    pthread_mutex_lock(&index_lock);
    replace_or_add_index(&indices, &indices_count, &newidx);
    save_all_indices(INDEX_FILE, indices, indices_count);
    pthread_mutex_unlock(&index_lock);

    write_n(client_fd, "FILE_OK\n", 8);
    close(client_fd);
    printf("Connection closed for %s\n", basename);
}

void *thread_main(void *arg) {
    int client_fd = (intptr_t)arg;
    handle_client(client_fd);
    return NULL;
}

int main() {
    ensure_folder(SYNC_FOLDER);

    indices = load_all_indices(INDEX_FILE, &indices_count);
    if (indices)
        printf("Loaded %d existing indices.\n", indices_count);
    else
        indices_count = 0;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sockfd, BACKLOG);
    printf("Server listening on port %d\n", PORT);

    while (1) {
        int c = accept(sockfd, NULL, NULL);
        pthread_t tid;
        pthread_create(&tid, NULL, thread_main, (void *)(intptr_t)c);
        pthread_detach(tid);
    }
    return 0;
}

