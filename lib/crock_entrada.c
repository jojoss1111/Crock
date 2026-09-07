#include "crock_interno.h"

#define ENTRADA_BUF_TAM 256

static char entrada_buf[ENTRADA_BUF_TAM];
static unsigned long entrada_buf_pos = 0;
static unsigned long entrada_buf_len = 0;

static int entrada_getchar(void) {
    if (entrada_buf_pos >= entrada_buf_len) {
        int64_f n = crock_plat_ler(0, entrada_buf, ENTRADA_BUF_TAM);
        if (n <= 0) return -1;
        entrada_buf_len = (unsigned long)n;
        entrada_buf_pos = 0;
    }
    return (unsigned char)entrada_buf[entrada_buf_pos++];
}

char *entrada_str(int cap) {
    int capacidade = (cap > 0) ? cap : 16;
    int tamanho = 0;
    char *buffer = (char *)memoria_malloc((unsigned long)capacidade);
    if (!buffer) return NULL;

    int c;
    while ((c = entrada_getchar()) != '\n' && c != -1) {
        if (tamanho + 1 >= capacidade) {
            capacidade *= 2;
            char *novo = (char *)memoria_realloc(buffer, (unsigned long)capacidade);
            if (!novo) { memoria_free(buffer); return NULL; }
            buffer = novo;
        }
        buffer[tamanho++] = (char)c;
    }
    buffer[tamanho] = '\0';
    return buffer;
}

void entrada_str_em(char *destino, int tam) {
    char *temp = entrada_str(tam);
    if (temp == NULL) { destino[0] = '\0'; return; }
    txt_copia(destino, temp, (unsigned long)(tam - 1));
    destino[tam - 1] = '\0';
    memoria_free(temp);
}

static void entrada_ler_num(char *buffer, int cap) {
    int i = 0, c;
    while ((c = entrada_getchar()) != '\n' && c != -1) {
        if (i < cap - 1) buffer[i++] = (char)c;
    }
    buffer[i] = '\0';
}

int64_f entrada_int64(void) {
    char buffer[100];
    entrada_ler_num(buffer, (int)sizeof(buffer));
    return txt_p_int(buffer);
}

int entrada_int(void) { return (int)entrada_int64(); }

static int64_f entrada_int_limite(int64_f minimo, int64_f maximo) {
    int64_f valor = entrada_int64();
    if (valor < minimo) return minimo;
    if (valor > maximo) return maximo;
    return valor;
}

int8_f entrada_int8(void) {
    return (int8_f)entrada_int_limite(-128, 127);
}

int16_f entrada_int16(void) {
    return (int16_f)entrada_int_limite(-32768, 32767);
}

int32_f entrada_int32(void) {
    return (int32_f)entrada_int_limite(-2147483647LL - 1, 2147483647LL);
}

double entrada_float(void) {
    char buffer[100];
    entrada_ler_num(buffer, (int)sizeof(buffer));
    return txt_p_flt(buffer);
}

float entrada_float32(void) {
    double valor = entrada_float();
    const double maximo = 3.4028234663852886e38;
    if (valor > maximo) return (float)maximo;
    if (valor < -maximo) return (float)-maximo;
    return (float)valor;
}

double entrada_float64(void) {
    return entrada_float();
}
