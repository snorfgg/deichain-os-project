/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "blockchain_ledger.h"

#define LEDGER_KEY (SHM_KEY + 1)
#define LEDGER_SEM_KEY (SEM_KEY + 1)

static SharedLedger *ledger = NULL;
static int shm_id = -1;
static int sem_id = -1;

static void lock() {
    struct sembuf op = {0, -1, 0};
    if (semop(sem_id, &op, 1) == -1) {
        perror("[Ledger] lock semop failed");
        exit(EXIT_FAILURE);
    }
}

static void unlock() {
    struct sembuf op = {0, 1, 0};
    if (semop(sem_id, &op, 1) == -1) {
        perror("[Ledger] unlock semop failed");
        exit(EXIT_FAILURE);
    }
}

void init_ledger() {
    size_t total_size = sizeof(SharedLedger);

    shm_id = shmget(LEDGER_KEY, total_size, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("[Ledger] shmget failed");
        exit(EXIT_FAILURE);
    }

    void *raw = shmat(shm_id, NULL, 0);
    if (raw == (void *)-1) {
        perror("[Ledger] shmat failed");
        exit(EXIT_FAILURE);
    }

    ledger = (SharedLedger *)raw;

    if (ledger->block_count < 0 || ledger->block_count > BLOCKCHAIN_BLOCKS) {
        ledger->block_count = 0;
        memset(ledger->blocks, 0, sizeof(Block) * BLOCKCHAIN_BLOCKS);
    }

    sem_id = semget(LEDGER_SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id == -1) {
        if (errno == EEXIST) {
            sem_id = semget(LEDGER_SEM_KEY, 1, 0666);
            if (sem_id < 0) {
                perror("[Ledger] semget existing failed");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("[Ledger] semget failed");
            exit(EXIT_FAILURE);
        }
    } else {
        if (semctl(sem_id, 0, SETVAL, 1) == -1) {
            perror("[Ledger] semctl SETVAL failed");
            exit(EXIT_FAILURE);
        }
    }
}

void add_block(Block *block) {
    lock();

    if (ledger->block_count < BLOCKCHAIN_BLOCKS) {
        if (ledger->block_count == 0) {
            strcpy(block->previous_hash, "INITIAL_HASH");
        } else {
            strcpy(block->previous_hash, ledger->blocks[ledger->block_count - 1].hash);
        }

        block->timestamp = time(NULL);
        ledger->blocks[ledger->block_count++] = *block;
    } else {
        fprintf(stderr, "[Ledger] Blockchain is full. Block not added.\n");
    }

    unlock();
}

int get_block_count() {
    lock();
    int count = ledger->block_count;
    unlock();
    return count;
}

void print_ledger() {
    lock();
    printf("\n=================== Start Ledger ===================\n");
    for (int i = 0; i < ledger->block_count; i++) {
        Block *b = &ledger->blocks[i];

        char time_str[26];
        ctime_r(&b->timestamp, time_str);
        time_str[strcspn(time_str, "\n")] = 0;

        printf("||----  Block %03d --\n", i);
        printf("Block ID: %d\n", b->id);
        printf("Previous Hash:\n%s\n", b->previous_hash);
        printf("Block Timestamp: %s\n", time_str);
        printf("Nonce: %d\n", b->nonce);
        printf("Hash: %s\n", b->hash);
        printf("Reward Sum: %d\n", b->reward_sum);
        printf("Transactions:\n");

        for (int j = 0; j < TRANSACTIONS_PER_BLOCK; j++) {
            Transaction *tx = &b->transactions[j];
            char tx_time_str[26];
            ctime_r(&tx->timestamp, tx_time_str);
            tx_time_str[strcspn(tx_time_str, "\n")] = 0;

            printf("  [%d] TX ID: %d | Reward: %d | Value: %.2f | Age: %d | Time: %s\n",
                   j, tx->id, tx->reward, tx->value, tx->age, tx_time_str);
        }

        printf("||------------------------------\n");
    }
    printf("=================== End   Ledger ===================\n");
    unlock();
}//TODO:CHECK IF ITS PRINTING

void cleanup_ledger() {
    if (ledger != NULL)
        shmdt((void *)ledger);
    if (shm_id >= 0)
        shmctl(shm_id, IPC_RMID, NULL);
    if (sem_id >= 0)
        semctl(sem_id, 0, IPC_RMID);
}

SharedLedger *get_ledger() {
    return ledger;
}

