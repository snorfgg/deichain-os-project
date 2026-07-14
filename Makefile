all: controller miner validator txgen statistics

controller: controller.c config.c logger.c transaction_pool.c blockchain_ledger.c
	gcc -Wall -pthread -o controller controller.c config.c logger.c transaction_pool.c blockchain_ledger.c -lcrypto

miner: miner.c config.c logger.c transaction_pool.c blockchain_ledger.c
	gcc -Wall -pthread -o miner miner.c config.c logger.c transaction_pool.c blockchain_ledger.c -lcrypto

validator: validator.c config.c logger.c transaction_pool.c blockchain_ledger.c
	gcc -Wall -pthread -o validator validator.c config.c logger.c transaction_pool.c blockchain_ledger.c -lcrypto

txgen: txgen.c config.c logger.c transaction_pool.c transaction_generator.c
	gcc -Wall -pthread -o txgen txgen.c config.c logger.c transaction_pool.c transaction_generator.c -lcrypto

statistics: statistics.c config.c logger.c
	gcc -Wall -pthread -o statistics statistics.c config.c logger.c -lcrypto

clean:
	rm -f controller miner validator txgen statistics
