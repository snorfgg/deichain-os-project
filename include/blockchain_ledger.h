/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef BLOCKCHAIN_LEDGER_H
#define BLOCKCHAIN_LEDGER_H

#include "config.h"
#include "transaction.h"  // Required for Transaction
#include <time.h>         // Required for time_t

// Compile-time max bounds (must be >= runtime config)
#define MAX_TRANSACTIONS_PER_BLOCK 20
#define MAX_BLOCKCHAIN_BLOCKS 100

// Safety checks to catch mismatched config.cfg at compile-time
#if TRANSACTIONS_PER_BLOCK > MAX_TRANSACTIONS_PER_BLOCK
#error "TRANSACTIONS_PER_BLOCK exceeds MAX_TRANSACTIONS_PER_BLOCK — update blockchain_ledger.h or lower config.cfg"
#endif

#if BLOCKCHAIN_BLOCKS > MAX_BLOCKCHAIN_BLOCKS
#error "BLOCKCHAIN_BLOCKS exceeds MAX_BLOCKCHAIN_BLOCKS — update blockchain_ledger.h or lower config.cfg"
#endif

// Block structure stored in shared memory
typedef struct {
    int id;
    int miner_id;
    int nonce;
    char hash[65];
    char previous_hash[65];
    time_t timestamp;
    int reward_sum;
    Transaction transactions[MAX_TRANSACTIONS_PER_BLOCK];
} Block;

// Ledger structure stored in shared memory
typedef struct {
    int block_count;
    Block blocks[MAX_BLOCKCHAIN_BLOCKS];
} SharedLedger;

// Function prototypes
void add_block(Block *block);
void cleanup_ledger(void);
int get_block_count(void);
SharedLedger *get_ledger(void);
void init_ledger(void);
void print_ledger(void);

#endif  // BLOCKCHAIN_LEDGER_H

