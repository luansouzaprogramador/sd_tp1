#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Uso: ./emissor <PID> <SINAL>
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PID> <SINAL>\n", argv[0]);
        return 1;
    }

    // Converte argumentos
    pid_t pid = (pid_t) atoi(argv[1]);
    int sig   = atoi(argv[2]);

    // Valida se o sinal é válido (0 < sig < NSIG)
    if (sig <= 0 || sig >= _NSIG) {
        fprintf(stderr, "Erro: sinal %d invalido.\n", sig);
        return 1;
    }

    // Verifica se o processo existe usando kill(pid, 0)
    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            fprintf(stderr, "Erro: processo %d nao existe.\n", pid);
        } else if (errno == EPERM) {
            fprintf(stderr, "Erro: sem permissao para sinalizar o processo %d.\n", pid);
        } else {
            perror("kill");
        }
        return 1;
    }

    // Envia o sinal
    if (kill(pid, sig) == -1) {
        perror("kill");
        return 1;
    }

    printf("Sinal %d enviado para o processo %d.\n", sig, pid);
    return 0;
}