/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "config.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#define MAX_MINERS 32

// Global stats
static int valid_blocks[MAX_MINERS] = {0};
static int invalid_blocks[MAX_MINERS] = {0};
static int credits[MAX_MINERS] = {0};
static int total_blocks = 0;
static int msg_id = -1;

struct msgbuf {
    long mtype;
    struct {
        int miner_id;
        int credits;
        int valid;
    } data;
};

void print_statistics() {
    log_message("=== Statistics Summary ===");
    for (int i = 0; i < MAX_MINERS; i++) {
        if (valid_blocks[i] > 0 || invalid_blocks[i] > 0) {
            printf("Miner %d -> Valid: %d | Invalid: %d | Credits: %d\n",
                   i, valid_blocks[i], invalid_blocks[i], credits[i]);
        }
    }
    printf("Total Blocks Processed: %d\n", total_blocks);
    fflush(stdout);
}

void handle_sigusr1(int sig) {
    print_statistics();
}

void handle_sigint(int sig) {
    log_message("[Statistics] SIGINT received. Cleaning up...");
    print_statistics();
    close_logger();
    exit(0);
}

int main() {
    if (init_logger() != 0) {
        fprintf(stderr, "[Statistics] Logger init failed.\n");
        return EXIT_FAILURE;
    }

    log_message("[Statistics] Starting up...");

    signal(SIGUSR1, handle_sigusr1);
    signal(SIGINT, handle_sigint);

    msg_id = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_id == -1) {
        perror("[Statistics] msgget failed");
        return EXIT_FAILURE;
    }

    struct msgbuf msg;
    while (1) {
        ssize_t rcv = msgrcv(msg_id, &msg, sizeof(msg.data), 0, 0);
        if (rcv > 0) {
            int id = msg.data.miner_id;
            if (id < 0 || id >= MAX_MINERS) {
                log_message("[Statistics] Warning: Invalid miner ID received (%d)", id);
                continue;
            }

            if (msg.data.valid)
                valid_blocks[id]++;
            else
                invalid_blocks[id]++;

            credits[id] += msg.data.credits;
            total_blocks++;

            log_message("[Statistics] Miner %d submitted a %s block. Credits +%d",
                        id, msg.data.valid ? "VALID" : "INVALID", msg.data.credits);
        } else {
            perror("[Statistics] msgrcv failed");
        }
    }

    return 0;
}
