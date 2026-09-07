#include "crock_interno.h"

void escritor_char(CrockEscritor *e, char c) { e->emite(e->ctx, c); }

void escritor_n(CrockEscritor *e, const char *s, size_t n) {
    if (n == 0) return;
    if (e->emite_n != NULL) e->emite_n(e->ctx, s, n);
    else for (size_t i = 0; i < n; i++) e->emite(e->ctx, s[i]);
}

void escritor_str(CrockEscritor *e, const char *s) {
    if (!s) return;
    const char *inicio = s;
    while (*s) s++;
    escritor_n(e, inicio, (size_t)(s - inicio));
}

void escritor_uint(CrockEscritor *e, uint64_f valor, int base, int maiusculo) {
    char buffer[70];
    int i = 0;
    const char *digitos = maiusculo ? "0123456789ABCDEF" : "0123456789abcdef";
    if (base < 2 || base > 16) base = 10;

    if (valor == 0) {
        buffer[i++] = '0';
    } else {
        while (valor > 0) {
            buffer[i++] = digitos[valor % (uint64_f)base];
            valor /= (uint64_f)base;
        }
    }
    while (i > 0) escritor_char(e, buffer[--i]);
}

void escritor_int(CrockEscritor *e, int64_f valor) {
    if (valor < 0) {
        escritor_char(e, '-');
        uint64_f abs_valor = (uint64_f)(~((uint64_f)valor)) + 1ULL;
        escritor_uint(e, abs_valor, 10, 0);
    } else {
        escritor_uint(e, (uint64_f)valor, 10, 0);
    }
}

void escritor_float(CrockEscritor *e, double valor, int casas) {
    if (casas < 0) casas = 6;
    if (valor < 0) {
        escritor_char(e, '-');
        valor = -valor;
    }

    double ajuste = 0.5;
    for (int i = 0; i < casas; i++) ajuste /= 10.0;
    valor += ajuste;

    long parte_inteira = (long)valor;
    double resto = valor - (double)parte_inteira;
    escritor_int(e, parte_inteira);

    if (casas > 0) {
        escritor_char(e, '.');
        for (int i = 0; i < casas; i++) {
            resto *= 10;
            int digito = (int)resto;
            if (digito > 9) digito = 9;
            if (digito < 0) digito = 0;
            escritor_char(e, (char)('0' + digito));
            resto -= digito;
        }
    }
}

void escritor_vfmt(CrockEscritor *e, const char *formato, va_lista args) {
    for (int i = 0; formato[i] != '\0'; i++) {
        if (formato[i] != '%') {
            int inicio = i;
            while (formato[i] != '\0' && formato[i] != '%') i++;
            escritor_n(e, formato + inicio, (size_t)(i - inicio));
            i--;
            continue;
        }
        i++;
        int e_longo = 0;
        while (formato[i] == 'l') { e_longo++; i++; }
        switch (formato[i]) {
            case 'd': case 'i':
                if (e_longo) escritor_int(e, va_prox(args, int64_f));
                else         escritor_int(e, va_prox(args, int));
                break;
            case 'u':
                if (e_longo) escritor_uint(e, va_prox(args, uint64_f), 10, 0);
                else         escritor_uint(e, va_prox(args, unsigned int), 10, 0);
                break;
            case 'x':
                if (e_longo) escritor_uint(e, va_prox(args, uint64_f), 16, 0);
                else         escritor_uint(e, va_prox(args, unsigned int), 16, 0);
                break;
            case 'X':
                if (e_longo) escritor_uint(e, va_prox(args, uint64_f), 16, 1);
                else         escritor_uint(e, va_prox(args, unsigned int), 16, 1);
                break;
            case 's': escritor_str(e, va_prox(args, const char *)); break;
            case 'c': escritor_char(e, (char)va_prox(args, int)); break;
            case 'f': escritor_float(e, va_prox(args, double), 6); break;
            case 'p':
                escritor_str(e, "0x");
                escritor_uint(e, (unsigned long)va_prox(args, void *), 16, 0);
                break;
            case '%': escritor_char(e, '%'); break;
            case '\0': i--; break;
            default:
                escritor_char(e, '%');
                if (e_longo) for (int k = 0; k < e_longo; k++) escritor_char(e, 'l');
                escritor_char(e, formato[i]);
                break;
        }
    }
}
