/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include "transaction_generator.h"
#include "transaction_pool.h"     
#include "logger.h"               
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static int global_transaction_id = 0;  // Thread-safe atomic counter

Transaction create_transaction(float value, const char *data) {
    Transaction tx;
    tx.id = __sync_fetch_and_add(&global_transaction_id, 1);  // atomic
    tx.reward = 1;
    tx.age = 0;
    tx.value = value;
    tx.timestamp = time(NULL);
    strncpy(tx.data, data, sizeof(tx.data) - 1);
    tx.data[sizeof(tx.data) - 1] = '\0';
    return tx;
}

int generate_and_add_transaction(float value, const char *data) {
    Transaction tx = create_transaction(value, data);
    return add_transaction_full(tx.id, tx.reward, tx.value, tx.timestamp, tx.data);
}
