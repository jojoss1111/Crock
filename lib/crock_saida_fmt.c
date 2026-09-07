#include "crock_interno.h"

static void emite_fd(void *ctx, char c) {
    saida_char_fd(*(int *)ctx, c);
}

static void emite_fd_n(void *ctx, const char *s, size_t n) {
    crock_saida_n_fd(*(int *)ctx, s, n);
}

void saida_uint_fd(int fd, uint64_f valor, int base, int maiusculo) {
    CrockEscritor e = { &fd, emite_fd, emite_fd_n };
    escritor_uint(&e, valor, base, maiusculo);
}

void saida_int_fd(int fd, int64_f valor) {
    CrockEscritor e = { &fd, emite_fd, emite_fd_n };
    escritor_int(&e, valor);
}

void saida_float_fd(int fd, double valor, int casas) {
    CrockEscritor e = { &fd, emite_fd, emite_fd_n };
    escritor_float(&e, valor, casas);
}

static void saida_vfmt_fd(int fd, const char *formato, va_lista args) {
    CrockEscritor e = { &fd, emite_fd, emite_fd_n };
    escritor_vfmt(&e, formato, args);
}

void saida_fmt_fd(int fd, const char *formato, ...) {
    va_lista args;
    va_inicio(args, formato);
    saida_vfmt_fd(fd, formato, args);
    va_fim(args);
}

void crock_saida_txt_fmt(const char *formato, ...) {
    va_lista args;
    va_inicio(args, formato);
    saida_vfmt_fd(SAIDA_STDOUT, formato, args);
    va_fim(args);
    saida_flush(SAIDA_STDOUT);
}

void crock_saida_txt_ln_fmt(const char *formato, ...) {
    va_lista args;
    va_inicio(args, formato);
    saida_vfmt_fd(SAIDA_STDOUT, formato, args);
    va_fim(args);
    saida_char_fd(SAIDA_STDOUT, '\n');
    saida_flush(SAIDA_STDOUT);
}

void crock_saida_erro_fmt(const char *formato, ...) {
    va_lista args;
    va_inicio(args, formato);
    saida_vfmt_fd(SAIDA_STDERR, formato, args);
    va_fim(args);
    saida_flush(SAIDA_STDERR);
}
