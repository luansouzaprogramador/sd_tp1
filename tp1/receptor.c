#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t flag = 0;

static void safe_write(const char *msg, size_t len) {
    ssize_t n = write(1, msg, len);
    (void) n;
}

void handler_usr1(int sig) {
    (void) sig;
    safe_write("Recebido SIGUSR1\n", 17);
    flag = 1;
}

void handler_usr2(int sig) {
    (void) sig;
    safe_write("Recebido SIGUSR2\n", 17);
    flag = 1;
}

void handler_int(int sig) {
    (void) sig;
    safe_write("Recebido SIGINT - encerrando\n", 29);
    _exit(0);
}

int main(int argc, char *argv[]) {
    // Uso: ./receptor <busy|blocking>
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <busy|blocking>\n", argv[0]);
        return 1;
    }

    int busy_wait = (strcmp(argv[1], "busy") == 0);
    if (!busy_wait && strcmp(argv[1], "blocking") != 0) {
        fprintf(stderr, "Erro: modo deve ser 'busy' ou 'blocking'.\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = handler_usr1;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction SIGUSR1");
        return 1;
    }

    sa.sa_handler = handler_usr2;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("sigaction SIGUSR2");
        return 1;
    }

    sa.sa_handler = handler_int;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    // Informa PID e modo para o usuario
    printf("Receptor iniciado.\n");
    printf("PID: %d\n", getpid());
    printf("Modo: %s\n", busy_wait ? "busy wait" : "blocking wait");
    fflush(stdout);

    if (busy_wait) {
        // BUSY WAIT: consome CPU em loop checando a flag
        while (1) {
            if (flag) {
                flag = 0; // reseta a flag
            }
        }
    } else {
        // BLOCKING WAIT: dorme ate chegar sinal
        while (1) {
            pause(); // retorna quando um sinal e capturado
        }
    }

    return 0;
}