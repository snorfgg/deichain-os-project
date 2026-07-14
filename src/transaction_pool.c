/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "transaction_pool.h"
#include "config.h"
#include "logger.h"
#include "transaction_generator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_POOL_SIZE 1000

static int shm_id = -1;
static int sem_id = -1;
static int sem_tx_avail_id = -1;
static SharedTransactionPool *shared_pool = NULL;

void lock_transaction_pool() {
    if (sem_id < 0) {
        log_message("[TxPool] Cannot lock: invalid sem_id");
        return;
    }
    struct sembuf op = {0, -1, SEM_UNDO};
    if (semop(sem_id, &op, 1) == -1) {
        perror("[TxPool] lock semop");
    }
}

void unlock_transaction_pool() {
    if (sem_id < 0) {
        log_message("[TxPool] Cannot unlock: invalid sem_id");
        return;
    }
    struct sembuf op = {0, 1, SEM_UNDO};
    if (semop(sem_id, &op, 1) == -1) {
        perror("[TxPool] unlock semop");
    }
}

void init_transaction_pool() {
    if (TX_POOL_SIZE <= 0 || TX_POOL_SIZE > MAX_POOL_SIZE) {
        fprintf(stderr, "[TxPool] Invalid TX_POOL_SIZE: %d.\n", TX_POOL_SIZE);
        exit(EXIT_FAILURE);
    }

    size_t size = sizeof(SharedTransactionPool);
    log_message("[TxPool] SHM size: %zu", size);

    shm_id = shmget(SHM_KEY, size, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("[TxPool] shmget");
        exit(EXIT_FAILURE);
    }

    void *ptr = shmat(shm_id, NULL, 0);
    if (ptr == (void *)-1) {
        perror("[TxPool] shmat");
        exit(EXIT_FAILURE);
    }

    shared_pool = (SharedTransactionPool *)ptr;

    if (shared_pool->count < 0 || shared_pool->count > TX_POOL_SIZE) {
        log_message("[TxPool] Detected invalid count: resetting pool.");
        shared_pool->count = 0;
        memset(shared_pool->transactions, 0, sizeof(shared_pool->transactions));
    }

    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("[TxPool] semget mutex");
        exit(EXIT_FAILURE);
    }

    sem_tx_avail_id = semget(SEM_KEY_TX_AVAIL, 1, IPC_CREAT | 0666);
    if (sem_tx_avail_id < 0) {
        perror("[TxPool] semget TX_AVAIL");
        exit(EXIT_FAILURE);
    }

    log_message("[TxPool] Initialized pool with semaphores.");
}
int add_transaction_struct(Transaction *tx) {
    return add_transaction_full(tx->id, tx->reward, tx->value, tx->timestamp, tx->data);
}

int add_transaction_full(int id, int reward, float value, time_t timestamp, const char *data) {
    static int last_signaled_blocks = 0;

    if (!shared_pool) {
        log_message("[TxPool] add_transaction_full called with NULL shared_pool");
        return 0;
    }

    lock_transaction_pool();

    // Check for duplicate ID
    for (int i = 0; i < shared_pool->count; i++) {
        if (shared_pool->transactions[i].id == id) {
            log_message("[TxPool] Duplicate TX ID %d detected. Rejecting.", id);
            unlock_transaction_pool();
            return 0;
        }
    }

    if (shared_pool->count >= TX_POOL_SIZE) {
        log_message("[TxPool] Pool full! TX %d rejected. Count=%d", id, shared_pool->count);
        unlock_transaction_pool();
        return 0;
    }

    Transaction *tx = &shared_pool->transactions[shared_pool->count];
    tx->id = id;
    tx->reward = reward;
    tx->value = value;
    tx->timestamp = timestamp;
    tx->age = 0;
    strncpy(tx->data, data, sizeof(tx->data) - 1);
    tx->data[sizeof(tx->data) - 1] = '\0';

    shared_pool->count++;
    int block_ready = shared_pool->count / TRANSACTIONS_PER_BLOCK;

    if (block_ready > last_signaled_blocks) {
        struct sembuf op = {0, 1, 0};
        if (semop(sem_tx_avail_id, &op, 1) == -1) {
            perror("[TxPool] semop signal failed");
        } else {
            log_message("[TxPool] Signaled miner (tx=%d, blocks=%d)", shared_pool->count, block_ready);
            last_signaled_blocks = block_ready;
        }
    }

    unlock_transaction_pool();
    return 1;
}

void remove_transactions(int num) {
    lock_transaction_pool();

    if (num > shared_pool->count) {
        log_message("[TxPool] Removing %d txs but only %d exist — adjusting.", num, shared_pool->count);
        num = shared_pool->count;
    }

    for (int i = 0; i < shared_pool->count - num; i++) {
        shared_pool->transactions[i] = shared_pool->transactions[i + num];
    }
    shared_pool->count -= num;

    log_message("[TxPool] Removed %d transactions. Pool now at %d", num, shared_pool->count);

    unlock_transaction_pool();
}

Transaction *fetch_transactions(int *num) {
    if (!shared_pool) {
        log_message("[TxPool] fetch_transactions called but shared_pool is NULL");
        *num = 0;
        return NULL;
    }

    lock_transaction_pool();

    int count = shared_pool->count;
    if (count < TRANSACTIONS_PER_BLOCK) {
        *num = count;
    } else {
        *num = TRANSACTIONS_PER_BLOCK;
    }

    Transaction *txs = shared_pool->transactions;

    unlock_transaction_pool();
    return txs;
}

void wait_for_transactions() {
    if (sem_tx_avail_id < 0) {
        log_message("[TxPool] Cannot wait: sem_tx_avail_id invalid");
        return;
    }

    struct sembuf op = {0, -1, 0};
    if (semop(sem_tx_avail_id, &op, 1) == -1) {
        if (errno == EIDRM) {
            log_message("[TxPool] Semaphore removed during wait.");
        } else {
            perror("[TxPool] wait_for_transactions semop");
        }
    }
}

void attach_transaction_pool() {
    size_t size = sizeof(SharedTransactionPool);
    shm_id = shmget(SHM_KEY, size, 0666);
    if (shm_id < 0) {
        perror("[TxPool] shmget attach");
        exit(EXIT_FAILURE);
    }

    void *mem = shmat(shm_id, NULL, 0);
    if (mem == (void *)-1) {
        perror("[TxPool] shmat attach");
        exit(EXIT_FAILURE);
    }

    shared_pool = (SharedTransactionPool *)mem;

    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id < 0) {
        perror("[TxPool] semget mutex attach");
        exit(EXIT_FAILURE);
    }

    sem_tx_avail_id = semget(SEM_KEY_TX_AVAIL, 1, 0666);
    if (sem_tx_avail_id < 0) {
        perror("[TxPool] semget TX_AVAIL attach");
        exit(EXIT_FAILURE);
    }

    log_message("[TxPool] Attached to existing pool.");
}

void detach_transaction_pool() {
    if (shared_pool != NULL) {
        shmdt((void *)shared_pool);
        shared_pool = NULL;
        log_message("[TxPool] Detached from pool.");
    }
}

void cleanup_transaction_pool() {
    detach_transaction_pool();
    if (shm_id >= 0) shmctl(shm_id, IPC_RMID, NULL);
    if (sem_id >= 0) semctl(sem_id, 0, IPC_RMID);
    if (sem_tx_avail_id >= 0) semctl(sem_tx_avail_id, 0, IPC_RMID);
    log_message("[TxPool] Cleaned up shared resources.");
}

SharedTransactionPool *get_transaction_pool(void) {
    return shared_pool;
}

