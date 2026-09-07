#ifndef CROCK_INTERNO_H
#define CROCK_INTERNO_H

#include "crock.h"

/* ── va_list ao estilo do projeto ─────────────────────────────────────── */
typedef __builtin_va_list va_lista;
#define va_inicio(v, ultimo) __builtin_va_start(v, ultimo)
#define va_fim(v)            __builtin_va_end(v)
#define va_prox(v, tipo)     __builtin_va_arg(v, tipo)
#define va_dup(d, s)         __builtin_va_copy(d, s)

/* ── camada de plataforma (implementada em crock_plat_linux.asm /
   crock_plat_win32.c) ──────────────────────────────────────────────────── */
extern void    *crock_plat_memoria_reservar(size_t tam);
extern int64_f  crock_plat_escrever(int fd, const void *buf, size_t tam);
extern int64_f  crock_plat_ler(int fd, void *buf, size_t tam);
extern int      crock_plat_abrir_leitura(const char *caminho);
extern int      crock_plat_abrir_escrita(const char *caminho);
extern void     crock_plat_fechar(int fd);
extern int64_f  crock_plat_relogio_ns(void);
extern void     crock_plat_dormir_ns(int64_f ns);

/* ── erro global ──────────────────────────────────────────────────────── */
int   crock_falha(CrockErro e);
void *crock_falha_ptr(CrockErro e);

#define CROCK_EXIGIR(cond, erro, retorno) \
    do { if (!(cond)) { crock_falha(erro); return (retorno); } } while (0)

/* ── memória: usado fora de crock_memoria.c (vetor, saida) ───────────── */
void memoria_mover(void *destino, const void *origem, size_t tam);

/* ── saída: usado pela camada formatada sem puxar o allocator ─────────── */
void crock_saida_n_fd(int fd, const char *s, size_t n);

/* ── formatador genérico, compartilhado por crock_saida.c e crock_txt.c ─ */
typedef struct {
    void *ctx;
    void (*emite)(void *ctx, char c);
    void (*emite_n)(void *ctx, const char *s, size_t n);
} CrockEscritor;

void escritor_char(CrockEscritor *e, char c);
void escritor_n(CrockEscritor *e, const char *s, size_t n);
void escritor_str(CrockEscritor *e, const char *s);
void escritor_uint(CrockEscritor *e, uint64_f valor, int base, int maiusculo);
void escritor_int(CrockEscritor *e, int64_f valor);
void escritor_float(CrockEscritor *e, double valor, int casas);
void escritor_vfmt(CrockEscritor *e, const char *formato, va_lista args);

#endif
