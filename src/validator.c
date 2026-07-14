/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include "logger.h"
#include "transaction_pool.h"
#include "blockchain_ledger.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>

#define VALIDATOR_SEM_KEY (SEM_KEY + 2)

struct msgbuf {
    long mtype;
    struct {
        int miner_id;
        int credits;
        int valid;
    } data;
};

static int msg_id = -1;
static int fifo_fd = -1;
static int validator_sem_id = -1;
static char fifo_name[128] = {0};

void handle_sigint(int sig) {
    log_message("[Validator] SIGINT received. Cleaning up.");
    if (fifo_fd != -1) close(fifo_fd);
    detach_transaction_pool();
    close_logger();
    exit(0);
}

int validate_pow(const char *hash) {
    for (int i = 0; i < DIFFICULTY; i++) {
        if (hash[i] != '0') return 0;
    }
    return 1;
}

void init_validator_semaphore() {
    validator_sem_id = semget(VALIDATOR_SEM_KEY, 1, IPC_CREAT | 0666);
    if (validator_sem_id < 0) {
        perror("[Validator] Failed to get validator mutex");
        exit(EXIT_FAILURE);
    }
}

void lock_validator() {
    struct sembuf op = {0, -1, 0};
    if (semop(validator_sem_id, &op, 1) == -1) {
        perror("[Validator] lock failed");
        exit(EXIT_FAILURE);
    }
}

void unlock_validator() {
    struct sembuf op = {0, 1, 0};
    if (semop(validator_sem_id, &op, 1) == -1) {
        perror("[Validator] unlock failed");
        exit(EXIT_FAILURE);
    }
}

void age_transactions() {
    lock_transaction_pool();
    SharedTransactionPool *pool = get_transaction_pool();
    for (int i = 0; i < pool->count; i++) {
        pool->transactions[i].age++;
        if (pool->transactions[i].age % 50 == 0) {
            pool->transactions[i].reward++;
        }
    }
    unlock_transaction_pool();
}

void remove_block_transactions(const Transaction *block_tx) {
    lock_transaction_pool();
    SharedTransactionPool *pool = get_transaction_pool();

    for (int i = 0; i < TRANSACTIONS_PER_BLOCK; i++) {
        int found = 0;
        for (int j = 0; j < pool->count; j++) {
            if (pool->transactions[j].id == block_tx[i].id) {
                for (int k = j; k < pool->count - 1; k++) {
                    pool->transactions[k] = pool->transactions[k + 1];
                }
                pool->count--;
                found = 1;
                break;
            }
        }
        if (!found) {
            log_message("[Validator]  TX ID %d not found in pool", block_tx[i].id);
        }
    }

    log_message("[Validator]  Pool cleaned, %d transactions remain", pool->count);
    unlock_transaction_pool();
}

int open_fifo_for_reading() {
    int fd;
    while ((fd = open(fifo_name, O_RDONLY)) == -1) {
        log_message("[Validator] Waiting for FIFO '%s'...", fifo_name);
        sleep(1);
    }
    return fd;
}

int read_block_serialized(int fd, Block *block) {
    size_t total_read = 0;
    char *buf = (char *)block;
    size_t target_size = sizeof(Block);

    log_message("[Validator] Expecting to read %zu bytes for a full Block", target_size);

    while (total_read < target_size) {
        ssize_t n = read(fd, buf + total_read, target_size - total_read);

        if (n > 0) {
            total_read += n;
            log_message("[Validator] Read %zd bytes (total: %zu/%zu)", n, total_read, target_size);
        } else if (n == 0) {
            log_message("[Validator] FIFO writer closed unexpectedly. Reopening FIFO...");
            close(fd);
            fifo_fd = open_fifo_for_reading();
            total_read = 0;
            buf = (char *)block;  // Reset buffer position
        } else {
            if (errno == EINTR) {
                log_message("[Validator] Read interrupted by signal. Retrying...");
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_message("[Validator] FIFO would block. Waiting...");
                usleep(10000);
                continue;
            }
            perror("[Validator] read_block_serialized failed");
            return 0;
        }
    }

    log_message("[Validator] Successfully read full Block from FIFO");
    return 1;
}


int validate_block(Block *block, int *total_credits) {
    age_transactions();

    int current_count = get_block_count();
    if (current_count > 0) {
        const char *last_hash = get_ledger()->blocks[current_count - 1].hash;
        if (strncmp(block->previous_hash, last_hash, 64) != 0) {
            log_message("[Validator]  Block rejected: bad previous hash");
            return 0;
        }
    } else {
        if (strcmp(block->previous_hash, "INITIAL_HASH") != 0) {
            log_message("[Validator]  Block rejected: bad genesis hash");
            return 0;
        }
    }

    *total_credits = 0;
    for (int i = 0; i < TRANSACTIONS_PER_BLOCK; i++) {
        *total_credits += block->transactions[i].reward;
    }

    if (!validate_pow(block->hash)) {
        log_message("[Validator]  Block rejected: PoW failed");
        return 0;
    }

    remove_block_transactions(block->transactions);
    add_block(block);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "[Validator] Usage: %s <fifo_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    strncpy(fifo_name, argv[1], sizeof(fifo_name) - 1);
    fifo_name[sizeof(fifo_name) - 1] = '\0';

    signal(SIGINT, handle_sigint);

    if (init_logger() != 0) {
        fprintf(stderr, "[Validator] Logger failed.\n");
        return EXIT_FAILURE;
    }

    log_message("[Validator]  Startup complete. FIFO: %s", fifo_name);

    attach_transaction_pool();
    init_ledger();
    init_validator_semaphore();

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    if (msg_id < 0) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    fifo_fd = open_fifo_for_reading();

    while (1) {
        Block block;
        memset(&block, 0, sizeof(Block));

        if (!read_block_serialized(fifo_fd, &block)) {
            log_message("[Validator]  Block read failed, skipping...");
            continue;
        }

        log_message("[Validator]  Validating block from Miner %d...", block.miner_id);

        int total_credits = 0;
        int valid = 0;

        lock_validator();
        valid = validate_block(&block, &total_credits);
        unlock_validator();

        struct msgbuf msg = {
            .mtype = 1,
            .data = {
                .miner_id = block.miner_id,
                .credits = total_credits,
                .valid = valid
            }
        };

        if (msgsnd(msg_id, &msg, sizeof(msg.data), 0) < 0) {
            perror("msgsnd");
        }

        log_message("[Validator]  Sent result to miner %d — %s", block.miner_id, valid ? "VALID" : "INVALID");
    }

    return 0;
}

