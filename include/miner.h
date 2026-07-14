/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef MINER_H
#define MINER_H

#include "config.h"
#include "transaction_pool.h"  
#include "logger.h"           

// Estrutura que representa um bloco a ser minerado
/*typedef struct {
    int id;                                    // Identificador do bloco
    Transaction transactions[TRANSACTIONS_PER_BLOCK];  // Transações que compõem o bloco
    int nonce;                                 // Valor utilizado no Proof-of-Work
    char hash[65];                             // Hash resultante da mineração (SHA256)
    int reward_sum;                            // Soma das recompensas das transações incluídas
} Block;*/

// Protótipo da função executada por cada thread de mineração
void *mine_block(void *arg);

#endif // MINER_H
