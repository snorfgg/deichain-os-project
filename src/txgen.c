/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include "logger.h"
#include "transaction_pool.h"
#include "transaction_generator.h"  //preciso?

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

static int reward = 0;
static int sleep_time = 0;
static int counter = 0;
static int max_transactions = 0;

void cleanup_txgen() {
    log_message("[TxGen] Cleaning up resources...");
    detach_transaction_pool();
    close_logger();
}

void handle_sigint(int sig) {
    log_message("[TxGen] SIGINT received. Initiating cleanup.");
    cleanup_txgen();
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <reward> <sleep_time_ms>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    reward = atoi(argv[1]);
    sleep_time = atoi(argv[2]);

    srand(time(NULL) ^ getpid());

    if (init_logger() != 0) {
        fprintf(stderr, "[TxGen] Failed to initialize logger.\n");
        exit(EXIT_FAILURE);
    }

    log_message("[TxGen] Parsed input: reward=%d, sleep_time=%dms", reward, sleep_time);

    read_config("config.cfg");
    max_transactions = TX_POOL_SIZE;

    signal(SIGINT, handle_sigint);
    atexit(cleanup_txgen);

    log_message("[TxGen] Attaching to transaction pool...");
    attach_transaction_pool();
    log_message("[TxGen] Attached. Starting generation loop...");

    int retry_count = 0;

    while (counter < max_transactions) {
        char tx_data[256];
        snprintf(tx_data, sizeof(tx_data), "Transaction #%d from PID %d", counter, getpid());

        float value = (rand() % 10000) / 100.0f;

        // Create and add transaction using the new method
        Transaction tx = create_transaction(value, tx_data);
        tx.reward = reward;  // Inject your little parameterized bribe

        log_message("[TxGen] Attempting to add TX ID %d", tx.id);
        int success = add_transaction_struct(&tx);

        if (success) {
            log_message("[TxGen]  Added TX ID %d (counter=%d)", tx.id, counter + 1);
            counter++;
            retry_count = 0;
        } else {
            SharedTransactionPool *pool = get_transaction_pool();
            int pool_count = pool ? pool->count : -1;

            log_message("[TxGen]  TX %d failed (pool full? count=%d). Retrying...", tx.id, pool_count);
            retry_count++;
            if (retry_count % 10 == 0) {
                log_message("[TxGen] Retry count = %d. Still failing at TX %d", retry_count, counter);
            }

            usleep(50000);
        }

        usleep(sleep_time * 1000);
    }

    log_message("[TxGen] Reached max of %d transactions. Shutting down.", max_transactions);
    return 0;
}

