/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <time.h>  

typedef struct {
    int id;                 // is unico
    int reward;             // reward varia com ciclos
    int age;                // ciclos
    float value;            // valor
    time_t timestamp;       // cria diferentes
    char data[256];         // +info
} Transaction;

#endif

