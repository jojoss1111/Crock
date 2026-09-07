#include "crock_interno.h"

static uint64_f crock_rand_estado = 0;

static uint64_f crock_rand_proximo(void) {
    if (crock_rand_estado == 0) {
        uint64_f semente = (uint64_f)timer_relogio_ns();
        crock_rand_estado = semente ? semente : 0x9E3779B97F4A7C15ULL;
    }
    uint64_f x = crock_rand_estado;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    crock_rand_estado = x;
    return x * 0x2545F4914F6CDD1DULL;
}

void random_seed(uint64_f semente) {
    crock_rand_estado = semente ? semente : 0x9E3779B97F4A7C15ULL;
}

uint32_f random_uint(void) {
    return (uint32_f)(crock_rand_proximo() >> 32);
}

int32_f random_int(int32_f min, int32_f max) {
    if (min > max) { int32_f tmp = min; min = max; max = tmp; }
    uint32_f faixa = (uint32_f)((int64_f)max - (int64_f)min) + 1u;
    if (faixa == 0) return (int32_f)random_uint();
    return min + (int32_f)(random_uint() % faixa);
}

float random_float(float min, float max) {
    if (min > max) { float tmp = min; min = max; max = tmp; }
    float frac = (float)random_uint() / 4294967296.0f;
    return min + frac * (max - min);
}

double random_double(double min, double max) {
    if (min > max) { double tmp = min; min = max; max = tmp; }
    double frac = (double)crock_rand_proximo() / 18446744073709551616.0;
    return min + frac * (max - min);
}

bool random_bool(void) {
    return (bool)(random_uint() & 1u);
}

char random_char(const char *conjunto) {
    static const char padrao[] =
        " !\"#$%&'()*+,-./0123456789:;<=>?@"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
        "abcdefghijklmnopqrstuvwxyz{|}~";
    const char *base = (conjunto && conjunto[0]) ? conjunto : padrao;
    unsigned long tam = txt_tam(base);
    return base[random_uint() % (uint32_f)tam];
}

Vetor random_lista_int(int32_f n, int32_f min, int32_f max) {
    Vetor v = vetor_criar(sizeof(int32_f), n > 0 ? n : 0);
    for (int32_f i = 0; i < n; i++) {
        int32_f valor = random_int(min, max);
        vetor_add(&v, &valor);
    }
    return v;
}

Vetor random_lista_float(int32_f n, float min, float max) {
    Vetor v = vetor_criar(sizeof(float), n > 0 ? n : 0);
    for (int32_f i = 0; i < n; i++) {
        float valor = random_float(min, max);
        vetor_add(&v, &valor);
    }
    return v;
}

Vetor random_lista_bool(int32_f n) {
    Vetor v = vetor_criar(sizeof(bool), n > 0 ? n : 0);
    for (int32_f i = 0; i < n; i++) {
        bool valor = random_bool();
        vetor_add(&v, &valor);
    }
    return v;
}

Vetor random_lista_char(int32_f n, const char *conjunto) {
    Vetor v = vetor_criar(sizeof(char), n > 0 ? n : 0);
    for (int32_f i = 0; i < n; i++) {
        char valor = random_char(conjunto);
        vetor_add(&v, &valor);
    }
    return v;
}
