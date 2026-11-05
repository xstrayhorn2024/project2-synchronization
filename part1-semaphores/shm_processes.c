#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/* Both the account and the semaphore must be shared across fork(). */
typedef struct {
    int BankAccount;
    sem_t mutex;
} Shared;
static volatile sig_atomic_t running = 1;
static void stop(int sig) { (void)sig; running = 0; }

/* role 0 is Dad, role 1 is Mom, and role 2 is a student. */
static void simulate(Shared *shared, int role)
{
    int localBalance = 0;
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    while (running) {
        sleep((unsigned)(rand() % (role == 1 ? 11 : 6)));
        if (!running) break;
        if (role == 0) puts("Dear Old Dad: Attempting to Check Balance");
        else if (role == 1) puts("Loveable Mom: Attempting to Check Balance");
        else puts("Poor Student: Attempting to Check Balance");

        /* Keep the entire read/modify/write transaction in one critical section. */
        while (sem_wait(&shared->mutex) == -1) {
            if (errno != EINTR) { perror("sem_wait"); return; }
            if (!running) return;
        }
        if (role == 0) {
            if (rand() % 2 == 0) {
                if (localBalance < 100) {
                    localBalance = shared->BankAccount;
                    int amount = rand() % 101;
                    if (amount % 2 == 0) {
                        localBalance += amount;
                        printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                        shared->BankAccount = localBalance;
                    } else puts("Dear old Dad: Doesn't have any money to give");
                } else printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            } else printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        } else if (role == 1) {
            if (localBalance <= 100) {
                localBalance = shared->BankAccount;
                int amount = rand() % 126;
                localBalance += amount;
                printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
                shared->BankAccount = localBalance;
            }
        } else {
            if (rand() % 2 == 0) {
                localBalance = shared->BankAccount;
                int need = rand() % 51;
                printf("Poor Student needs $%d\n", need);
                if (need <= localBalance) {
                    localBalance -= need;
                    printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
                } else printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
                shared->BankAccount = localBalance;
            } else printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }
        if (sem_post(&shared->mutex) == -1) { perror("sem_post"); return; }
    }
}

int main(int argc, char *argv[])
{
    int parents = 1, students = 1;
    if (argc != 1 && argc != 3) {
        fprintf(stderr, "Usage: %s [1|2 number_of_students]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 3) {
        char *end;
        long p = strtol(argv[1], &end, 10);
        if (!*argv[1] || *end || p < 1 || p > 2) return EXIT_FAILURE;
        long n = strtol(argv[2], &end, 10);
        if (!*argv[2] || *end || n < 1 || n > 1000) return EXIT_FAILURE;
        parents = (int)p; students = (int)n;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    struct sigaction action = {0};
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    Shared *shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }
    shared->BankAccount = 0;
    if (sem_init(&shared->mutex, 1, 1) == -1) {
        perror("sem_init"); munmap(shared, sizeof(*shared)); return EXIT_FAILURE;
    }
    int count = students + parents - 1;
    pid_t *children = calloc((size_t)count, sizeof(*children));
    if (!children) { perror("calloc"); sem_destroy(&shared->mutex); munmap(shared, sizeof(*shared)); return EXIT_FAILURE; }
    int created = 0, failed = 0;
    /* Fork Mom separately, leaving the original parent as Dad. */
    if (parents == 2) {
        pid_t mom = fork();
        if (mom < 0) { perror("fork Mom"); failed = 1; running = 0; }
        else if (mom == 0) {
            simulate(shared, 1);
            munmap(shared, sizeof(*shared));
            free(children);
            _exit(EXIT_SUCCESS);
        } else children[created++] = mom;
    }
    /* Fork each student with its own local balance. */
    for (int i = 0; i < students && !failed; ++i) {
        pid_t student = fork();
        if (student < 0) { perror("fork Student"); failed = 1; running = 0; break; }
        if (student == 0) {
            printf("Poor Student #%d: PID %ld%c", i + 1, (long)getpid(), 10);
            simulate(shared, 2);
            munmap(shared, sizeof(*shared));
            free(children);
            _exit(EXIT_SUCCESS);
        }
        children[created++] = student;
    }
    if (!failed) simulate(shared, 0);
    for (int i = 0; i < created; ++i) kill(children[i], SIGTERM);
    for (int i = 0; i < created; ++i)
        while (waitpid(children[i], NULL, 0) == -1 && errno == EINTR) {}
    free(children);
    sem_destroy(&shared->mutex);
    munmap(shared, sizeof(*shared));
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
