# DEIChain: Concurrent Blockchain Simulator

Academic project developed for the **Operating Systems** course of the **Bachelor's Degree in Computer Engineering** at the **University of Coimbra**.

## Overview

DEIChain is a concurrent blockchain simulation implemented in C.

The project focuses on operating-system concepts such as process creation, multithreading, inter-process communication, synchronization, shared resources and signal handling.

The system simulates the generation, mining, validation and storage of transactions and blocks.

## Main Components

- **Controller** – Initializes the system, creates IPC resources and manages child processes.
- **Transaction Generator** – Generates transactions and inserts them into the shared transaction pool.
- **Miner** – Uses multiple threads to create blocks and perform Proof-of-Work.
- **Validator** – Validates mined blocks and updates the blockchain ledger.
- **Statistics** – Collects information about valid and invalid blocks and miner credits.
- **Logger** – Provides synchronized logging to the terminal and log file.

## Operating Systems Concepts

- Processes created with `fork()` and `exec()`
- POSIX threads
- System V shared memory
- System V semaphores
- System V message queues
- Named pipes (FIFOs)
- Mutex synchronization
- Signal handling
- Graceful resource cleanup

## Blockchain Features

- Shared transaction pool
- Shared blockchain ledger
- SHA-256 hashing
- Simplified Proof-of-Work
- Transaction aging and reward adjustment
- Dynamic scaling of validator processes
- Miner statistics and credits

## Architecture

A PDF version of the architecture is available in [`docs/architecture.pdf`](docs/architecture.pdf).

## Project Structure

```text
.
├── include/        Header files
├── src/            C source files
├── docs/           Architecture and project report
├── Makefile
├── config.cfg
└── README.md
