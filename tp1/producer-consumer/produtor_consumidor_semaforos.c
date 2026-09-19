// Referência:
// https://www.geeksforgeeks.org/operating-systems/producer-consumer-problem-using-semaphores-set-1/

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define M 100000
#define MAX_LOG (M * 3)

// Parametros
static int N, Np, Nc;
static int verbose = 0;

// Buffer circular - mesma estrutura do GFG
static int* buffer;
static int in_pos = 0;   // proxima posicao de escrita  (produtor)
static int out_pos = 0;  // proxima posicao de leitura  (consumidor)
static int count = 0;    // itens atualmente no buffer

/*
 * SINCRONIZACAO
 *
 * GFG usava variaveis de condicao:
 *   pthread_cond_t not_full   <- produtor esperava quando count == N
 *   pthread_cond_t not_empty  <- consumidor esperava quando count == 0
 *
 * Aqui substituimos por semaforos contadores (conforme enunciado):
 *   sem_t empty  (init = N)  <- substitui not_full
 *   sem_t full   (init = 0)  <- substitui not_empty
 *
 * O mutex pthread permanece igual ao GFG.
 */
static sem_t empty;  // posicoes livres   - init N
static sem_t full;   // posicoes ocupadas - init 0
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Controle de termino
static atomic_int stop_flag = 0;
static int consumed = 0;
static pthread_mutex_t cnt_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t done_mtx = PTHREAD_MUTEX_INITIALIZER;

// Log de ocupacao (escrito dentro do mutex - sem lock extra)
static int* occ_log;
static int occ_sz = 0;

static int is_prime(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (int i = 3; (long long)i * i <= n; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

/* ================================================================== */
/*  Thread PRODUTORA                                                    */
/*                                                                      */
/*  GFG (com cond var):                                                 */
/*    pthread_mutex_lock(&mutex);                                       */
/*    while (count == N) pthread_cond_wait(&not_full, &mutex);         */
/*    insere item                                                       */
/*    pthread_cond_signal(&not_empty);                                  */
/*    pthread_mutex_unlock(&mutex);                                     */
/*                                                                      */
/*  Versao com semaforos:                                               */
/*    sem_wait(&empty);          <- P(empty): bloqueia se buffer cheio  */
/*    pthread_mutex_lock(&mutex);                                       */
/*    insere item                                                       */
/*    pthread_mutex_unlock(&mutex);                                     */
/*    sem_post(&full);           <- V(full): sinaliza novo item         */
/* ================================================================== */
static void* producer(void* arg) {
    (void)arg;
    unsigned int seed = (unsigned int)((uintptr_t)pthread_self() ^
                                       (uintptr_t)time(NULL) * 2654435761UL);

    while (!atomic_load_explicit(&stop_flag, memory_order_relaxed)) {
        // Produz item aleatorio em [1, 10^7]
        int item = (int)((unsigned)rand_r(&seed) % 10000000) + 1;

        // P(empty): bloqueia se buffer cheio
        // substitui: while(count==N) pthread_cond_wait(&not_full, &m)
        sem_wait(&empty);

        if (atomic_load_explicit(&stop_flag, memory_order_relaxed)) {
            sem_post(&empty);
            break;
        }

        // Secao critica - igual ao GFG
        pthread_mutex_lock(&mutex);

        buffer[in_pos] = item;
        if (verbose) printf("[Produtor]  Produced: %d at %d\n", item, in_pos);
        in_pos = (in_pos + 1) % N;
        count++;
        if (occ_sz < MAX_LOG) occ_log[occ_sz++] = count;

        pthread_mutex_unlock(&mutex);

        // V(full): sinaliza que ha novo item disponivel
        // substitui: pthread_cond_signal(&not_empty)
        sem_post(&full);
    }

    return NULL;
}

/* ================================================================== */
/*  Thread CONSUMIDORA                                                  */
/*                                                                      */
/*  GFG (com cond var):                                                 */
/*    pthread_mutex_lock(&mutex);                                       */
/*    while (count == 0) pthread_cond_wait(&not_empty, &mutex);        */
/*    remove item                                                       */
/*    pthread_cond_signal(&not_full);                                   */
/*    pthread_mutex_unlock(&mutex);                                     */
/*                                                                      */
/*  Versao com semaforos:                                               */
/*    sem_wait(&full);           <- P(full): bloqueia se buffer vazio   */
/*    pthread_mutex_lock(&mutex);                                       */
/*    remove item                                                       */
/*    pthread_mutex_unlock(&mutex);                                     */
/*    sem_post(&empty);          <- V(empty): sinaliza posicao livre    */
/* ================================================================== */
static void* consumer(void* arg) {
    (void)arg;

    while (1) {
        // P(full): bloqueia se buffer vazio
        // substitui: while(count==0) pthread_cond_wait(&not_empty, &m)
        sem_wait(&full);

        if (atomic_load_explicit(&stop_flag, memory_order_relaxed)) {
            sem_post(&full);
            break;
        }

        // Secao critica - igual ao GFG
        pthread_mutex_lock(&mutex);

        int item = buffer[out_pos];
        if (verbose) printf("[Consumidor] Consumed: %d at %d\n", item, out_pos);
        out_pos = (out_pos + 1) % N;
        count--;
        if (occ_sz < MAX_LOG) occ_log[occ_sz++] = count;

        pthread_mutex_unlock(&mutex);

        // V(empty): sinaliza posicao livre
        // substitui: pthread_cond_signal(&not_full)
        sem_post(&empty);

        // Processamento fora da SC (como no GFG o sleep ficava fora)
        int prime = is_prime(item);
        if (verbose)
            printf("[Consumidor] %d -> %s primo\n", item,
                   prime ? "E" : "NAO E");

        // Contagem de itens consumidos
        pthread_mutex_lock(&cnt_mtx);
        consumed++;
        int done = (consumed >= M);
        pthread_mutex_unlock(&cnt_mtx);

        if (done) {
            atomic_store(&stop_flag, 1);
            pthread_mutex_lock(&done_mtx);
            pthread_cond_signal(&done_cond);
            pthread_mutex_unlock(&done_mtx);
            break;
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <N> <Np> <Nc> [-v]\n", argv[0]);
        return EXIT_FAILURE;
    }

    N = atoi(argv[1]);
    Np = atoi(argv[2]);
    Nc = atoi(argv[3]);
    if (argc > 4 && strcmp(argv[4], "-v") == 0) verbose = 1;

    if (N <= 0 || Np <= 0 || Nc <= 0) {
        fprintf(stderr, "N, Np e Nc devem ser inteiros positivos.\n");
        return EXIT_FAILURE;
    }

    buffer = malloc((size_t)N * sizeof(int));
    occ_log = malloc((size_t)MAX_LOG * sizeof(int));
    if (!buffer || !occ_log) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    /* Inicializa semaforos
     *   empty = N : todas as posicoes estao livres
     *   full  = 0 : nenhum item disponivel ainda
     */
    sem_init(&empty, 0, (unsigned int)N);
    sem_init(&full, 0, 0);

    pthread_t* prod = malloc((size_t)Np * sizeof(pthread_t));
    pthread_t* cons = malloc((size_t)Nc * sizeof(pthread_t));

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int i = 0; i < Np; i++) pthread_create(&prod[i], NULL, producer, NULL);
    for (int i = 0; i < Nc; i++) pthread_create(&cons[i], NULL, consumer, NULL);

    // Main dorme ate M itens serem consumidos
    pthread_mutex_lock(&done_mtx);
    while (!atomic_load(&stop_flag)) pthread_cond_wait(&done_cond, &done_mtx);
    pthread_mutex_unlock(&done_mtx);

    clock_gettime(CLOCK_MONOTONIC, &t_end);

    // Encerra threads bloqueadas nos semaforos
    atomic_store(&stop_flag, 1);
    for (int i = 0; i < Np; i++) sem_post(&empty);
    for (int i = 0; i < Nc; i++) sem_post(&full);

    for (int i = 0; i < Np; i++) pthread_join(prod[i], NULL);
    for (int i = 0; i < Nc; i++) pthread_join(cons[i], NULL);

    double elapsed = (double)(t_end.tv_sec - t_start.tv_sec) +
                     (double)(t_end.tv_nsec - t_start.tv_nsec) * 1e-9;
    printf("%.6f\n", elapsed);

    // Salva log de ocupacao
    char fname[128];
    snprintf(fname, sizeof(fname), "occ_N%d_Np%d_Nc%d.csv", N, Np, Nc);
    FILE* f = fopen(fname, "w");
    if (f) {
        fprintf(f, "event,occupancy\n");
        for (int i = 0; i < occ_sz; i++) fprintf(f, "%d,%d\n", i, occ_log[i]);
        fclose(f);
        fprintf(stderr, "[Info] Log salvo: %s (%d entradas)\n", fname, occ_sz);
    }

    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&cnt_mtx);
    pthread_mutex_destroy(&done_mtx);
    pthread_cond_destroy(&done_cond);
    free(buffer);
    free(occ_log);
    free(prod);
    free(cons);

    return EXIT_SUCCESS;
}