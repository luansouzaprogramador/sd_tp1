#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define TAM_MSG 20

static int eh_primo(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (int i = 3; (long) i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

static void produtor(int fd_escrita, int quantidade) {
    char buf[TAM_MSG];
    int N = 1;

    for (int i = 0; i < quantidade; i++) {
        int delta = (rand() % 100) + 1;
        N = N + delta;

        // Formata o numero em string de tamanho fixo
        memset(buf, 0, TAM_MSG);
        snprintf(buf, TAM_MSG, "%d", N);

        ssize_t escrito = write(fd_escrita, buf, TAM_MSG);
        if (escrito != TAM_MSG) {
            perror("write");
            break;
        }
    }

    // Envia o valor 0 para sinalizar fim
    memset(buf, 0, TAM_MSG);
    snprintf(buf, TAM_MSG, "%d", 0);
    if (write(fd_escrita, buf, TAM_MSG) != TAM_MSG) {
        perror("write");
    }

    close(fd_escrita);
}

static void consumidor(int fd_leitura) {
    char buf[TAM_MSG];
    ssize_t lido;

    while (1) {
        // Zera o buffer antes de ler
        memset(buf, 0, TAM_MSG);

        lido = read(fd_leitura, buf, TAM_MSG);
        if (lido < 0) {
            perror("read");
            break;
        }
        if (lido == 0) {
            // EOF: todas as pontas de escrita foram fechadas
            break;
        }
        if (lido != TAM_MSG) {
            // Leitura parcial (nao esperado neste caso)
            fprintf(stderr, "Aviso: leitura parcial (%zd bytes)\n", lido);
            break;
        }

        int valor = atoi(buf);

        if (valor == 0) {
            // Sinal de fim enviado pelo produtor
            break;
        }

        if (eh_primo(valor)) {
            printf("%d e primo\n", valor);
        } else {
            printf("%d nao e primo\n", valor);
        }
    }

    close(fd_leitura);
}

//Uso: ./produtor_consumidor <quantidade>
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <quantidade>\n", argv[0]);
        return 1;
    }

    int quantidade = atoi(argv[1]);
    if (quantidade < 0) {
        fprintf(stderr, "Erro: quantidade deve ser >= 0\n");
        return 1;
    }

    // Inicializa gerador aleatorio
    srand((unsigned) time(NULL));

    // Cria o pipe
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    // Cria o processo filho
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    }

    if (pid == 0) {
        // ---------- FILHO: CONSUMIDOR ----------
        close(fd[1]);             // fecha ponta de escrita
        consumidor(fd[0]);        // le da ponta de leitura
        _exit(0);
    } else {
        // ---------- PAI: PRODUTOR ----------
        close(fd[0]);             // fecha ponta de leitura
        produtor(fd[1], quantidade);
        wait(NULL);               // espera o filho terminar
    }

    return 0;
}