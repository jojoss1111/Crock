#include "crock_interno.h"

Vetor vetor_criar(size_t tam_elemento, int32_f capacidade_inicial) {
    Vetor v;
    v.limite = 0;
    if (tam_elemento == 0) {
        crock_falha(CROCK_ERRO_TAM_INVALIDO);
        v.dados = NULL;
        v.tamanho = 0;
        v.capacidade = 0;
        v.tam_elemento = 0;
        return v;
    }
    if (capacidade_inicial <= 0) capacidade_inicial = 4;
    v.tamanho = 0;
    v.capacidade = capacidade_inicial;
    v.tam_elemento = tam_elemento;
    size_t total = (size_t)capacidade_inicial * tam_elemento;
    if (total / tam_elemento != (size_t)capacidade_inicial) {
        crock_falha(CROCK_ERRO_TAM_INVALIDO);
        v.dados = NULL;
    } else {
        v.dados = memoria_malloc(total);
    }
    if (v.dados == NULL) v.capacidade = 0;
    return v;
}

int vetor_definir_limite(Vetor *v, int32_f limite) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(limite >= 0, CROCK_ERRO_TAM_INVALIDO, -1);
    CROCK_EXIGIR(limite == 0 || limite >= v->tamanho, CROCK_ERRO_LIMITE, -1);
    v->limite = limite;
    return 0;
}

int32_f vetor_limite(Vetor *v) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, 0);
    return v->limite;
}

int vetor_cheio(Vetor *v) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, 1);
    return v->limite > 0 && v->tamanho >= v->limite;
}

static int vetor_crescer(Vetor *v) {
    if (v->limite > 0 && v->capacidade >= v->limite) {
        return 0;
    }
    int32_f nova_cap = (v->capacidade > 0) ? v->capacidade * 2 : 4;
    if (nova_cap <= v->capacidade) return 0;
    if (v->limite > 0 && nova_cap > v->limite) nova_cap = v->limite;
    size_t novo_total = (size_t)nova_cap * v->tam_elemento;
    if (novo_total / v->tam_elemento != (size_t)nova_cap) { crock_falha(CROCK_ERRO_TAM_INVALIDO); return 0; }
    void *novo = memoria_realloc(v->dados, novo_total);
    if (novo == NULL) { crock_falha(CROCK_ERRO_MEMORIA); return 0; }
    v->dados = novo;
    v->capacidade = nova_cap;
    return 1;
}

static int vetor_garantir_espaco(Vetor *v) {
    if (v->tamanho < v->capacidade) return 0;
    if (v->limite > 0 && v->tamanho >= v->limite) return crock_falha(CROCK_ERRO_LIMITE);
    return vetor_crescer(v) ? 0 : -1;
}

int vetor_add(Vetor *v, void *valor) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(valor != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(v->tam_elemento != 0, CROCK_ERRO_TAM_INVALIDO, -1);
    if (vetor_garantir_espaco(v) != 0) return -1;
    char *destino = (char *)v->dados + ((size_t)v->tamanho * v->tam_elemento);
    memoria_copia(destino, valor, v->tam_elemento);
    v->tamanho++;
    return 0;
}

int vetor_add_str(Vetor *v, const char *texto) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(texto != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(v->tam_elemento != 0, CROCK_ERRO_TAM_INVALIDO, -1);
    if (vetor_garantir_espaco(v) != 0) return -1;
    char *destino = (char *)v->dados + ((size_t)v->tamanho * v->tam_elemento);
    memoria_zerar(destino, v->tam_elemento);
    txt_copia(destino, texto, v->tam_elemento - 1);
    destino[v->tam_elemento - 1] = '\0';
    v->tamanho++;
    return 0;
}

int vetor_inserir(Vetor *v, int32_f indice, void *valor) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(valor != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(v->tam_elemento != 0, CROCK_ERRO_TAM_INVALIDO, -1);
    CROCK_EXIGIR(indice >= 0 && indice <= v->tamanho, CROCK_ERRO_INDICE, -1);
    if (vetor_garantir_espaco(v) != 0) return -1;

    char *base = (char *)v->dados;
    char *origem = base + ((size_t)indice * v->tam_elemento);
    char *destino = base + ((size_t)(indice + 1) * v->tam_elemento);
    size_t bytes = (size_t)(v->tamanho - indice) * v->tam_elemento;

    if (bytes > 0) memoria_mover(destino, origem, bytes);
    memoria_copia(origem, valor, v->tam_elemento);
    v->tamanho++;
    return 0;
}

void *vetor_get(Vetor *v, int32_f indice) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, NULL);
    CROCK_EXIGIR(indice >= 0 && indice < v->tamanho, CROCK_ERRO_INDICE, NULL);
    return (char *)v->dados + ((size_t)indice * v->tam_elemento);
}

int vetor_set(Vetor *v, int32_f indice, void *valor) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(valor != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(indice >= 0 && indice < v->tamanho, CROCK_ERRO_INDICE, -1);
    memoria_copia((char *)v->dados + ((size_t)indice * v->tam_elemento), valor, v->tam_elemento);
    return 0;
}

int vetor_remover(Vetor *v, int32_f indice) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(indice >= 0 && indice < v->tamanho, CROCK_ERRO_INDICE, -1);
    char *base = (char *)v->dados;
    char *alvo = base + ((size_t)indice * v->tam_elemento);
    char *proximo = base + ((size_t)(indice + 1) * v->tam_elemento);
    size_t bytes = (size_t)(v->tamanho - indice - 1) * v->tam_elemento;

    if (bytes > 0) memoria_mover(alvo, proximo, bytes);
    v->tamanho--;
    return 0;
}

int32_f vetor_tam(Vetor *v) {
    CROCK_EXIGIR(v != NULL, CROCK_ERRO_NULO, 0);
    return v->tamanho;
}

void vetor_limpar(Vetor *v) {
    if (v == NULL) { crock_falha(CROCK_ERRO_NULO); return; }
    v->tamanho = 0;
}

void vetor_liberar(Vetor *v) {
    if (v == NULL) { crock_falha(CROCK_ERRO_NULO); return; }
    memoria_free(v->dados);
    v->dados = NULL;
    v->tamanho = 0;
    v->capacidade = 0;
    v->limite = 0;
}
