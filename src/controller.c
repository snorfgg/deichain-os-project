/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include "transaction_pool.h"
#include "blockchain_ledger.h"
#include "logger.h"
#include "transaction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_VALIDATORS 3

char fifo_names[MAX_VALIDATORS][64];
int fifo_fds[MAX_VALIDATORS] = {-1};

pid_t miner_pid = -1;
pid_t statistics_pid = -1;
pid_t validators[MAX_VALIDATORS] = {-1};

int sem_id = -1;
int sem_tx_avail_id = -1;
int msg_id = -1;
int validator_lock_id = -1;

void create_and_hold_fifos() {
    for (int i = 0; i < MAX_VALIDATORS; i++) {
        snprintf(fifo_names[i], sizeof(fifo_names[i]), "/tmp/VALIDATOR_INPUT_%d", i);
        unlink(fifo_names[i]);
        if (mkfifo(fifo_names[i], 0666) == -1 && errno != EEXIST) {
            perror("[Controller] mkfifo");
            exit(EXIT_FAILURE);
        }
        fifo_fds[i] = open(fifo_names[i], O_RDWR);
        if (fifo_fds[i] == -1) {
            perror("[Controller] open dummy fifo");
            exit(EXIT_FAILURE);
        }
    }
}

void handle_sigusr1(int sig) {
    log_message("[Controller] SIGUSR1 received — dumping ledger");

    SharedLedger *ledger = get_ledger();
    if (!ledger) {
        log_message("[Controller] Ledger not available.");
        return;
    }

    FILE *f = fopen("DEIChain_log.log", "a");
    if (!f) {
        perror("[Controller] Failed to open log file for ledger dump");
        return;
    }

    fprintf(f, "=================== Start Ledger ===================\n");
    printf("=================== Start Ledger ===================\n");

    for (int i = 0; i < ledger->block_count; i++) {
        Block *b = &ledger->blocks[i];

        char time_str[26];
        ctime_r(&b->timestamp, time_str);
        time_str[strcspn(time_str, "\n")] = 0;

        fprintf(f, "||----  Block %03d --\n", i);
        printf("||----  Block %03d --\n", i);

        fprintf(f, "Block ID: %d\nPrevious Hash:\n%s\nBlock Timestamp: %s\nNonce: %d\nHash: %s\nReward Sum: %d\nTransactions:\n",
                b->id, b->previous_hash, time_str, b->nonce, b->hash, b->reward_sum);

        printf("Block ID: %d\nPrevious Hash:\n%s\nBlock Timestamp: %s\nNonce: %d\nHash: %s\nReward Sum: %d\nTransactions:\n",
               b->id, b->previous_hash, time_str, b->nonce, b->hash, b->reward_sum);

        for (int j = 0; j < TRANSACTIONS_PER_BLOCK; j++) {
            Transaction *tx = &b->transactions[j];
            char tx_time_str[26];
            ctime_r(&tx->timestamp, tx_time_str);
            tx_time_str[strcspn(tx_time_str, "\n")] = 0;

            fprintf(f, "  [%d] TX ID: %d | Reward: %d | Value: %.2f | Age: %d | Time: %s\n",
                    j, tx->id, tx->reward, tx->value, tx->age, tx_time_str);

            printf("  [%d] TX ID: %d | Reward: %d | Value: %.2f | Age: %d | Time: %s\n",
                   j, tx->id, tx->reward, tx->value, tx->age, tx_time_str);
        }

        fprintf(f, "||------------------------------\n");
        printf("||------------------------------\n");
    }

    fprintf(f, "=================== End   Ledger ===================\n");
    printf("=================== End   Ledger ===================\n");

    fclose(f);
}

void cleanup(int sig) {
    log_message("[Controller] SIGINT received. Cleaning up...");

    cleanup_transaction_pool();
    cleanup_ledger();

    if (sem_id >= 0) semctl(sem_id, 0, IPC_RMID);
    if (sem_tx_avail_id >= 0) semctl(sem_tx_avail_id, 0, IPC_RMID);
    if (validator_lock_id >= 0) semctl(validator_lock_id, 0, IPC_RMID);
    if (msg_id >= 0) msgctl(msg_id, IPC_RMID, NULL);

    for (int i = 0; i < MAX_VALIDATORS; i++) {
        if (fifo_fds[i] != -1) close(fifo_fds[i]);
        unlink(fifo_names[i]);
        if (validators[i] > 0) kill(validators[i], SIGINT);
    }

    if (miner_pid > 0) kill(miner_pid, SIGINT);
    if (statistics_pid > 0) kill(statistics_pid, SIGINT);

    while (wait(NULL) > 0);
    close_logger();
    exit(0);
}

pid_t create_process(const char *name, const char *path, const char *arg) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("[Controller] fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        if (arg)
            execl(path, name, arg, NULL);
        else
            execl(path, name, NULL);
        perror("[Controller] execl failed");
        exit(EXIT_FAILURE);
    }
    return pid;
}

int process_alive(pid_t pid) {
    return (pid > 0 && kill(pid, 0) == 0);
}

int get_txpool_occupancy() {
    SharedTransactionPool *pool = get_transaction_pool();
    if (!pool) {
        log_message("[Controller] Failed to access transaction pool");
        return 0;
    }
    return (pool->count * 100 / TX_POOL_SIZE);
}

void adjust_validators() {
    int occupancy = get_txpool_occupancy();
    static int current = 1;

    log_message("[Controller] TX Pool occupancy: %d%%", occupancy);

    if (occupancy >= 80 && current < 3) {
        for (int i = current; i < 3; i++) {
            if (!process_alive(validators[i])) {
                validators[i] = create_process("validator", "./validator", fifo_names[i]);
                log_message("[Controller] Spawned validator %d", i + 1);
            }
        }
        current = 3;
    } else if (occupancy >= 60 && current < 2) {
        if (!process_alive(validators[1])) {
            validators[1] = create_process("validator", "./validator", fifo_names[1]);
            log_message("[Controller] Spawned validator 2");
        }
        current = 2;
    } else if (occupancy < 40 && current > 1) {
        for (int i = 1; i < current; i++) {
            if (validators[i] > 0 && process_alive(validators[i])) {
                kill(validators[i], SIGINT);
                log_message("[Controller] Terminated validator %d", i + 1);
                validators[i] = -1;
            }
        }
        current = 1;
    }
}

int main() {
    if (init_logger() != 0) {
        fprintf(stderr, "[Controller] Failed to initialize logger.\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, cleanup);
    signal(SIGUSR1, handle_sigusr1); 

    log_message("[Controller] DEI_CHAIN SIMULATOR STARTING");

    read_config("config.cfg");
    init_transaction_pool();
    init_ledger();

    create_and_hold_fifos();

    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) { perror("[Controller] semget"); exit(EXIT_FAILURE); }
    semctl(sem_id, 0, SETVAL, 1);

    sem_tx_avail_id = semget(SEM_KEY_TX_AVAIL, 1, IPC_CREAT | 0666);
    if (sem_tx_avail_id < 0) { perror("[Controller] semget TX_AVAIL"); exit(EXIT_FAILURE); }
    semctl(sem_tx_avail_id, 0, SETVAL, 0);

    validator_lock_id = semget(SEM_KEY_VALIDATOR, 1, IPC_CREAT | 0666);
    if (validator_lock_id < 0) { perror("[Controller] semget VALIDATOR LOCK"); exit(EXIT_FAILURE); }
    semctl(validator_lock_id, 0, SETVAL, 1);

    msg_id = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_id < 0) { perror("[Controller] msgget"); exit(EXIT_FAILURE); }

    log_message("[Controller] IPC resources initialized");

    miner_pid = create_process("miner", "./miner", NULL);
    log_message("[Controller] Miner started");

    validators[0] = create_process("validator", "./validator", fifo_names[0]);
    log_message("[Controller] Initial validator started");

    statistics_pid = create_process("statistics", "./statistics", NULL);
    log_message("[Controller] Statistics started");

    while (1) {
        adjust_validators();
        sleep(2);
    }

    return 0;
}