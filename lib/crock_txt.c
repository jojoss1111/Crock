#include "crock_interno.h"

typedef struct {
    char *dest;
    unsigned long capacidade;
    unsigned long usado;
    unsigned long total;
} TxtBufEscrita;

static void txt_buf_char(TxtBufEscrita *b, char c) {
    if (b->dest != NULL && b->usado + 1 < b->capacidade) {
        b->dest[b->usado++] = c;
    }
    b->total++;
}

static void emite_buf(void *ctx, char c) { txt_buf_char((TxtBufEscrita *)ctx, c); }
static void emite_buf_n(void *ctx, const char *s, size_t n) {
    TxtBufEscrita *b = (TxtBufEscrita *)ctx;
    unsigned long disponivel = (b->dest != NULL && b->capacidade > b->usado)
        ? b->capacidade - b->usado - 1 : 0;
    unsigned long copiar = (n < disponivel) ? (unsigned long)n : disponivel;
    if (copiar > 0) memoria_copia(b->dest + b->usado, s, copiar);
    b->usado += copiar;
    b->total += n;
}

static int txt_vfmt_buf(char *dest, unsigned long tam, const char *formato, va_lista args) {
    TxtBufEscrita b = { dest, tam, 0, 0 };
    CrockEscritor e = { &b, emite_buf, emite_buf_n };
    escritor_vfmt(&e, formato, args);

    if (dest != NULL && tam > 0) {
        unsigned long pos = (b.usado < tam - 1) ? b.usado : tam - 1;
        dest[pos] = '\0';
    }
    return (int)b.total;
}

int txt_fmt(char *dest, unsigned long tam, const char *formato, ...) {
    va_lista args;
    va_inicio(args, formato);
    int total = txt_vfmt_buf(dest, tam, formato, args);
    va_fim(args);
    return total;
}

char *txt_copia(char *dest, const char *src, unsigned long tam) {
    unsigned long i = 0;
    while (i < tam && src[i] != '\0') { dest[i] = src[i]; i++; }
    while (i < tam) { dest[i] = '\0'; i++; }
    return dest;
}

char *txt_cpy(char *dest, const char *src) {
    size_t tam = (size_t)txt_tam(src) + 1;
    memoria_copia(dest, src, tam);
    return dest;
}

char *txt_junta(char *dest, const char *src, unsigned long tam) {
    unsigned long tam_dest = 0;
    while (dest[tam_dest] != '\0') tam_dest++;

    unsigned long i = 0;
    while (i < tam && src[i] != '\0') { dest[tam_dest + i] = src[i]; i++; }
    dest[tam_dest + i] = '\0';
    return dest;
}

int txt_comp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

unsigned long txt_tam(const char *str) {
    unsigned long tam = 0;
    while (str[tam] != '\0') tam++;
    return tam;
}

int64_f txt_p_int(const char *str) {
    int i = 0, negativo = 0;
    uint64_f resultado = 0;
    const uint64_f maximo = (uint64_f)(((uint64_f)-1) >> 1);
    const uint64_f limite = maximo + 1;

    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') i++;
    if (str[i] == '-') { negativo = 1; i++; }
    else if (str[i] == '+') i++;

    while (str[i] >= '0' && str[i] <= '9') {
        uint64_f digito = (uint64_f)(str[i] - '0');
        uint64_f teto = negativo ? limite : maximo;
        if (resultado > (teto - digito) / 10) return negativo ? (int64_f)(-limite) : (int64_f)maximo;
        resultado = resultado * 10 + digito;
        i++;
    }
    if (negativo) {
        if (resultado == limite) return (int64_f)(-limite);
        return -(int64_f)resultado;
    }
    return (int64_f)resultado;
}

double txt_p_flt(const char *str) {
    int i = 0;
    double sinal = 1.0, resultado = 0.0;

    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') i++;
    if (str[i] == '-') { sinal = -1.0; i++; }
    else if (str[i] == '+') { i++; }

    const double maximo = 1.7976931348623157e308;
    while (str[i] >= '0' && str[i] <= '9') {
        if (resultado > maximo / 10.0) return sinal < 0 ? -maximo : maximo;
        resultado = resultado * 10.0 + (str[i] - '0');
        i++;
    }
    if (str[i] == '.') {
        i++;
        double fracao = 0.1;
        while (str[i] >= '0' && str[i] <= '9') {
            if (resultado > maximo - (str[i] - '0') * fracao) return sinal < 0 ? -maximo : maximo;
            resultado += (str[i] - '0') * fracao;
            fracao *= 0.1;
            i++;
        }
    }
    return resultado * sinal;
}

void txt_limpar(void) {
    const char *escape = "\033[0m\033[H\033[2J\033[3J";
    crock_plat_escrever(SAIDA_STDOUT, escape, txt_tam(escape));
}

char *txt_string(const char *formato, ...) {
    va_lista args, args_copia;
    va_inicio(args, formato);
    va_dup(args_copia, args);

    int tam = txt_vfmt_buf(NULL, 0, formato, args_copia);
    va_fim(args_copia);
    if (tam < 0) { va_fim(args); return NULL; }

    char *buffer = (char *)memoria_malloc((unsigned long)tam + 1);
    if (!buffer) { va_fim(args); return NULL; }

    txt_vfmt_buf(buffer, (unsigned long)tam + 1, formato, args);
    va_fim(args);
    return buffer;
}
