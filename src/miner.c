/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include "logger.h"
#include "blockchain_ledger.h"
#include "transaction_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#define FIFO_NAME_TEMPLATE "/tmp/VALIDATOR_INPUT_%d"
#define MAX_VALIDATORS 3
#define MSG_TIMEOUT_SEC 5

struct msgbuf {
    long mtype;
    struct {
        int miner_id;
        int credits;
        int valid;
    } data;
};

static pthread_t *threads = NULL;
static char fifo_names[MAX_VALIDATORS][128];
static volatile sig_atomic_t stop_flag = 0;
static int msg_id = -1;

void setup_fifo_names() {
    for (int i = 0; i < MAX_VALIDATORS; i++) {
        snprintf(fifo_names[i], sizeof(fifo_names[i]), FIFO_NAME_TEMPLATE, i);
    }
}

void sha256(const char *str, char output[65]) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)str, strlen(str), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

int check_difficulty(const char *hash) {
    for (int i = 0; i < DIFFICULTY; i++) {
        if (hash[i] != '0') return 0;
    }
    return 1;
}

void handle_sigint(int sig) {
    stop_flag = 1;
    log_message("[Miner] SIGINT received. Cleaning up.");
}

void broadcast_block(Block *block) {
    for (int i = 0; i < MAX_VALIDATORS; i++) {
        int fd = open(fifo_names[i], O_WRONLY);
        if (fd == -1) continue;
        if (flock(fd, LOCK_EX) == -1) { close(fd); continue; }

        write(fd, block, sizeof(Block));
        flock(fd, LOCK_UN);
        close(fd);
    }
}

void *mine_block(void *arg) {
    int thread_id = *((int *)arg);
    free(arg);

    while (!stop_flag) {
        wait_for_transactions();
        if (stop_flag) break;

        int tx_count = 0;
        Transaction *txs = fetch_transactions(&tx_count);
        if (tx_count < TRANSACTIONS_PER_BLOCK) continue;

        Block block = {0};
        block.id = getpid() * 100 + thread_id;
        block.miner_id = thread_id;
        block.timestamp = time(NULL);
        block.nonce = 0;
        block.reward_sum = 0;
        memcpy(block.transactions, txs, sizeof(Transaction) * TRANSACTIONS_PER_BLOCK);

        SharedLedger *ledger = get_ledger();
        int current_blocks = get_block_count();
        if (current_blocks == 0) {
            strcpy(block.previous_hash, "INITIAL_HASH");
        } else {
            strncpy(block.previous_hash, ledger->blocks[current_blocks - 1].hash, sizeof(block.previous_hash) - 1);
            block.previous_hash[sizeof(block.previous_hash) - 1] = '\0';
        }

        char data[4096] = {0};
        for (int i = 0; i < TRANSACTIONS_PER_BLOCK; i++) {
            snprintf(data + strlen(data), sizeof(data) - strlen(data), "%d:%s|", block.transactions[i].id, block.transactions[i].data);
            block.reward_sum += block.transactions[i].reward;
        }

        char hash[65];
        int nonce = 0;
        do {
            char input[8192];
            snprintf(input, sizeof(input), "%s%d%s%ld", data, nonce, block.previous_hash, block.timestamp);
            sha256(input, hash);
            nonce++;
        } while (!check_difficulty(hash) && !stop_flag);

        if (stop_flag) break;

        block.nonce = nonce;
        strncpy(block.hash, hash, sizeof(block.hash));

        broadcast_block(&block);
	log_message("MINER %d: Built block. Nonce=%d, Hash=%s, TotalReward=%d",
            thread_id, block.nonce, block.hash, block.reward_sum);

        struct msgbuf response;
        if (msgrcv(msg_id, &response, sizeof(response.data), 1, 0) >= 0) {
            log_message("[Miner %d] Got result — Valid: %d, Credits: %d", thread_id, response.data.valid, response.data.credits);
        }
    }

    return NULL;
}

int main() {
    read_config("config.cfg");
    if (init_logger() != 0) return EXIT_FAILURE;

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    attach_transaction_pool();
    init_ledger();
    setup_fifo_names();

    msg_id = msgget(MSG_QUEUE_KEY, 0666);
    if (msg_id < 0) return EXIT_FAILURE;

    threads = malloc(sizeof(pthread_t) * NUM_MINERS);
    if (!threads) return EXIT_FAILURE;

    for (int i = 0; i < NUM_MINERS; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, mine_block, id);
    }

    for (int i = 0; i < NUM_MINERS; i++) {
        pthread_join(threads[i], NULL);
    }

    detach_transaction_pool();
    close_logger();
    free(threads);
    return 0;
}

