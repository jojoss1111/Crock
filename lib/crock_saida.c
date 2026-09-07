#include "crock_interno.h"

#ifndef SAIDA_BUF_TAM
#define SAIDA_BUF_TAM 1024
#endif

typedef struct {
    int fd;
    unsigned long usado;
    char dados[SAIDA_BUF_TAM];
} SaidaBuffer;

static SaidaBuffer saida_buf_stdout = { SAIDA_STDOUT, 0, {0} };
static SaidaBuffer saida_buf_stderr = { SAIDA_STDERR, 0, {0} };

static SaidaBuffer *saida_buffer_de(int fd) {
    return (fd == SAIDA_STDERR) ? &saida_buf_stderr : &saida_buf_stdout;
}

static void saida_copia(char *destino, const char *origem, size_t tam) {
    for (size_t i = 0; i < tam; i++) destino[i] = origem[i];
}

static void saida_move(char *destino, const char *origem, size_t tam) {
    if (destino < origem) {
        for (size_t i = 0; i < tam; i++) destino[i] = origem[i];
    } else if (destino > origem) {
        while (tam > 0) {
            tam--;
            destino[tam] = origem[tam];
        }
    }
}

void saida_flush(int fd) {
    SaidaBuffer *b = saida_buffer_de(fd);
    if (b->usado > 0) {
        unsigned long escrito = 0;
        while (escrito < b->usado) {
            int64_f n = crock_plat_escrever(b->fd, b->dados + escrito, b->usado - escrito);
            if (n <= 0) break;
            escrito += (unsigned long)n;
        }
        if (escrito < b->usado) {
            saida_move(b->dados, b->dados + escrito, b->usado - escrito);
        }
        b->usado -= escrito;
    }
}

void saida_flush_tudo(void) {
    saida_flush(SAIDA_STDOUT);
    saida_flush(SAIDA_STDERR);
}

void saida_char_fd(int fd, char c) {
    SaidaBuffer *b = saida_buffer_de(fd);
    if (b->usado >= SAIDA_BUF_TAM) saida_flush(fd);
    b->dados[b->usado++] = c;
}

void saida_str_fd(int fd, const char *s) {
    if (!s) return;
    const char *inicio = s;
    while (*s != '\0') s++;
    crock_saida_n_fd(fd, inicio, (size_t)(s - inicio));
}

void crock_saida_n_fd(int fd, const char *s, size_t n) {
    SaidaBuffer *b = saida_buffer_de(fd);
    while (n > 0) {
        unsigned long livre = SAIDA_BUF_TAM - b->usado;
        if (livre == 0) { saida_flush(fd); continue; }
        size_t parte = n < livre ? n : livre;
        saida_copia(b->dados + b->usado, s, parte);
        b->usado += parte;
        s += parte;
        n -= parte;
    }
}

void saida_txt_fd(int fd, const char *s) {
    saida_str_fd(fd, s);
    saida_flush(fd);
}

void crock_saida_txt_1(const char *s) {
    saida_txt_fd(SAIDA_STDOUT, s);
}

void crock_saida_txt_ln_1(const char *s) {
    saida_str_fd(SAIDA_STDOUT, s);
    saida_char_fd(SAIDA_STDOUT, '\n');
    saida_flush(SAIDA_STDOUT);
}

void crock_saida_erro_1(const char *s) {
    saida_txt_fd(SAIDA_STDERR, s);
}
