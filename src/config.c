/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>

// var globais
int NUM_MINERS = 0;
int TX_POOL_SIZE = 0;
int TRANSACTIONS_PER_BLOCK = 0;
int BLOCKCHAIN_BLOCKS = 0;

void read_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("[Config] Failed to open config.cfg");
        exit(EXIT_FAILURE);
    }

    int read = fscanf(file, "%d %d %d %d",
                      &NUM_MINERS,
                      &TX_POOL_SIZE,
                      &TRANSACTIONS_PER_BLOCK,
                      &BLOCKCHAIN_BLOCKS);
    fclose(file);

    if (read != 4) {
        fprintf(stderr, "[Config] Invalid config format. Expected 4 integers.\n");
        fprintf(stderr, "Hint: You may be missing a config.cfg file or its contents are malformed.\n");
        exit(EXIT_FAILURE);
    }

    if (NUM_MINERS <= 0 || NUM_MINERS > 32) {
        fprintf(stderr, "[Config] NUM_MINERS out of bounds: %d\n", NUM_MINERS);
        exit(EXIT_FAILURE);
    }

    if (TX_POOL_SIZE <= 0 || TX_POOL_SIZE > 1000) {
        fprintf(stderr, "[Config] TX_POOL_SIZE out of bounds: %d\n", TX_POOL_SIZE);
        exit(EXIT_FAILURE);
    }

    if (TRANSACTIONS_PER_BLOCK <= 0 || TRANSACTIONS_PER_BLOCK > TX_POOL_SIZE) {
        fprintf(stderr, "[Config] TRANSACTIONS_PER_BLOCK invalid: %d (must be <= TX_POOL_SIZE)\n", TRANSACTIONS_PER_BLOCK);
        exit(EXIT_FAILURE);
    }

    if (BLOCKCHAIN_BLOCKS <= 0 || BLOCKCHAIN_BLOCKS > 1000) {
        fprintf(stderr, "[Config] BLOCKCHAIN_BLOCKS out of bounds: %d\n", BLOCKCHAIN_BLOCKS);
        exit(EXIT_FAILURE);
    }

    printf("[Config] Loaded: MINERS=%d | POOL=%d | TX_PER_BLOCK=%d | BLOCKCHAIN_LIMIT=%d\n",
           NUM_MINERS, TX_POOL_SIZE, TRANSACTIONS_PER_BLOCK, BLOCKCHAIN_BLOCKS);
}

