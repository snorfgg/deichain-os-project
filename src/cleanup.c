/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include "config.h" 

void cleanup_shared_memory(int shm_id) {
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
    }
}

void cleanup_message_queue(int msg_id) {
    if (msgctl(msg_id, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID");
    }
}

void cleanup_fifo(const char* fifo_path) {
    if (unlink(fifo_path) == -1) {
        perror("unlink FIFO");
    }
}
