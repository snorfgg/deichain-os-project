/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#ifndef CONFIG_H
#define CONFIG_H

//definido no .c
extern int NUM_MINERS;
extern int TX_POOL_SIZE;
extern int TRANSACTIONS_PER_BLOCK;
extern int BLOCKCHAIN_BLOCKS;

// para ler config
void read_config(const char *filename);

// nunca mudam
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define MSG_QUEUE_KEY 0x91011
#define DIFFICULTY 4
#define SEM_KEY_TX_AVAIL (SEM_KEY + 1)
#define SEM_KEY_VALIDATOR (SEM_KEY + 2)
#endif // CONFIG_H

