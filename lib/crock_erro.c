#include "crock_interno.h"

static CrockErro crock_ultimo_erro = CROCK_OK;

int crock_falha(CrockErro e) {
    crock_ultimo_erro = e;
    return -1;
}

void *crock_falha_ptr(CrockErro e) {
    crock_ultimo_erro = e;
    return NULL;
}

CrockErro crock_erro(void) { return crock_ultimo_erro; }

void crock_erro_limpar(void) { crock_ultimo_erro = CROCK_OK; }

const char *crock_erro_texto(CrockErro erro) {
    switch (erro) {
        case CROCK_OK:                  return "sem erro";
        case CROCK_ERRO_NULO:           return "ponteiro nulo passado onde era obrigatorio";
        case CROCK_ERRO_INDICE:         return "indice fora do intervalo valido";
        case CROCK_ERRO_MEMORIA:        return "falha ao alocar memoria";
        case CROCK_ERRO_TAM_INVALIDO:   return "tamanho de elemento ou capacidade invalido";
        case CROCK_ERRO_LIMITE:         return "operacao excederia o limite configurado";
        case CROCK_ERRO_USE_AFTER_FREE: return "uso de ponteiro ja liberado ou invalido";
        case CROCK_ERRO_ARQUIVO:        return "falha de entrada/saida em arquivo";
        case CROCK_ERRO_FORMATO:        return "formato invalido ou incompativel";
        default:                        return "erro desconhecido";
    }
}
