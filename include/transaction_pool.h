/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef TRANSACTION_POOL_H
#define TRANSACTION_POOL_H

#include "transaction.h"


#define TX_POOL_CAPACITY 100

typedef struct {
    int count;
    Transaction transactions[TX_POOL_CAPACITY];
} SharedTransactionPool;

void init_transaction_pool();
void attach_transaction_pool();
void detach_transaction_pool();
void cleanup_transaction_pool();
int add_transaction_struct(Transaction *tx);
int add_transaction_full(int id, int reward, float value, time_t timestamp, const char *data);
void remove_transactions(int num);
Transaction *fetch_transactions(int *num);
void wait_for_transactions();

SharedTransactionPool *get_transaction_pool(void);

void lock_transaction_pool();
void unlock_transaction_pool();

#endif // TRANSACTION_POOL_H

