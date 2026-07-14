/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef TRANSACTION_GENERATOR_H
#define TRANSACTION_GENERATOR_H

#include "transaction.h"  

// Creates a Transaction struct with unique ID and populated metadata.
Transaction create_transaction(float value, const char *data);

// Creates and directly attempts to add a transaction to the pool.
// Returns 1 on success, 0 if the pool is full or duplicate.
int generate_and_add_transaction(float value, const char *data);

#endif // TRANSACTION_GENERATOR_H
