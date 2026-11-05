# Project 2: Process and Thread Synchronization

Both parts of Project 2 are included in this repository.

## Part 1: Process Synchronization Using Semaphores

Source: [part1-semaphores/shm_processes.c](part1-semaphores/shm_processes.c)

Uses shared memory and a process-shared semaphore to protect a bank account accessed by Dad, optional Mom, and student processes.

Build and run on Linux:

```sh
cd part1-semaphores
make shm_proc
./shm_proc 1 1
# Extra credit: Mom, Dad, and ten students
./shm_proc 2 10
```

Press Ctrl+C to stop the simulation and clean up its processes.

## Part 2: Thread Synchronization Using Monitors and Condition Variables

Sources: [main.c](part2-monitors/main.c), [BENSCHILLIBOWL.c](part2-monitors/BENSCHILLIBOWL.c), and [header](part2-monitors/BENSCHILLIBOWL.h).

Simulates restaurant customers and cooks using POSIX threads, a bounded FIFO order queue, a mutex, and condition variables. The default configuration processes 270 orders from 90 customers using ten cooks.

Build and run on Linux:

```sh
cd part2-monitors
make
./main
```

The program exits after all orders are handled and all threads are joined.
