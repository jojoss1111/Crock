#include "crock_interno.h"

struct block {
    size_t size;
    int free;
    unsigned int magia;
    struct block *next;
    struct block *prev;
    struct block *free_next;
    struct block *free_prev;
    struct arena *dono;
};

#define CROCK_BLOCO_MAGIA_OK    0xC20C0001u
#define CROCK_BLOCO_MAGIA_LIVRE 0xDEADC0DEu

#define CROCK_SMALL_BIN_LIMIT 1024
#define CROCK_SMALL_BINS (CROCK_SMALL_BIN_LIMIT / 8)
#define CROCK_CLASSES 192

struct arena {
    char *base;
    size_t tamanho;
};

static struct block *free_bins[CROCK_CLASSES];

static int free_bin(size_t tam) {
    if (tam <= CROCK_SMALL_BIN_LIMIT) return (int)((tam + 7) / 8) - 1;

#if defined(__GNUC__) || defined(__clang__)
    int expoente = (int)(sizeof(size_t) * 8 - 1 - __builtin_clzl((unsigned long)tam));
#else
    int expoente = 0;
    while (tam > 1) { tam >>= 1; expoente++; }
#endif
    int bin = CROCK_SMALL_BINS + expoente - 10;
    if (bin >= CROCK_CLASSES) return CROCK_CLASSES - 1;
    if (bin < CROCK_SMALL_BINS) return CROCK_SMALL_BINS;
    return bin;
}

static void free_list_add(struct block *b) {
    int bin = free_bin(b->size);
    b->free_prev = NULL;
    b->free_next = free_bins[bin];
    if (free_bins[bin] != NULL) free_bins[bin]->free_prev = b;
    free_bins[bin] = b;
}

static void free_list_remove(struct block *b) {
    int bin = free_bin(b->size);
    if (b->free_prev != NULL) b->free_prev->free_next = b->free_next;
    else free_bins[bin] = b->free_next;
    if (b->free_next != NULL) b->free_next->free_prev = b->free_prev;
    b->free_next = NULL;
    b->free_prev = NULL;
}

static int heap_initialized = 0;

static struct arena *nova_arena(size_t tam_min) {
    size_t overhead = sizeof(struct block) + sizeof(struct arena);
    if (tam_min > (size_t)-1 - overhead) return NULL;
    size_t necessario = tam_min + overhead;

    size_t tam = HEAP_SIZE;
    while (tam < necessario) {
        if (tam > (size_t)-1 / 2) return NULL;
        tam *= 2;
    }

    void *mem = crock_plat_memoria_reservar(tam);
    if (mem == NULL) return NULL;

    struct arena *a = (struct arena *)mem;
    a->base = (char *)mem;
    a->tamanho = tam;

    struct block *b = (struct block *)((char *)mem + sizeof(struct arena));
    b->size = tam - sizeof(struct arena) - sizeof(struct block);
    b->free = 1;
    b->magia = CROCK_BLOCO_MAGIA_LIVRE;
    b->next = NULL;
    b->prev = NULL;
    b->dono = a;
    b->free_next = NULL;
    b->free_prev = NULL;

    free_list_add(b);
    return a;
}

void init_heap(void) {
    nova_arena(0);
    heap_initialized = 1;
}

void *memoria_zerar(void *destino, size_t tam) {
    __builtin_memset(destino, 0, tam);
    return destino;
}

void *memoria_copia(void *destino, const void *origem, size_t tam) {
    __builtin_memcpy(destino, origem, tam);
    return destino;
}

void memoria_mover(void *destino, const void *origem, size_t tam) {
    __builtin_memmove(destino, origem, tam);
}

static struct block *malloc_de_bloco(struct arena *a, struct block *atual, size_t tam) {
    free_list_remove(atual);
    if (atual->size >= tam + sizeof(struct block) + 8) {
        struct block *novo = (struct block *)((char *)atual + sizeof(struct block) + tam);
        novo->size = atual->size - tam - sizeof(struct block);
        novo->free = 1;
        novo->magia = CROCK_BLOCO_MAGIA_LIVRE;
        novo->next = atual->next;
        novo->prev = atual;
        novo->dono = a;
        if (atual->next != NULL) atual->next->prev = novo;

        atual->size = tam;
        atual->next = novo;
        free_list_add(novo);
    }
    atual->free = 0;
    atual->magia = CROCK_BLOCO_MAGIA_OK;
    return atual;
}

static struct block *malloc_na_arena(size_t tam) {
    for (int bin = free_bin(tam); bin < CROCK_CLASSES; bin++) {
        struct block *atual = free_bins[bin];
        while (atual != NULL) {
            struct block *proximo = atual->free_next;
            if (atual->size >= tam) return malloc_de_bloco(atual->dono, atual, tam);
            atual = proximo;
        }
    }
    return NULL;
}

void *memoria_malloc(size_t tam) {
    if (!heap_initialized) init_heap();
    if (tam == 0) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);
    if (tam > (size_t)-1 - 7) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    tam = (tam + 7) & ~(size_t)7;

    struct block *b = malloc_na_arena(tam);
    if (b == NULL) {
        struct arena *nova = nova_arena(tam);
        if (nova == NULL) return crock_falha_ptr(CROCK_ERRO_MEMORIA);
        b = malloc_na_arena(tam);
    }
    if (b == NULL) return crock_falha_ptr(CROCK_ERRO_MEMORIA);
    return (void *)((char *)b + sizeof(struct block));
}

void *memoria_calloc(size_t qtd, size_t tam_item) {
    if (qtd == 0 || tam_item == 0) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    size_t total = qtd * tam_item;
    if (total / qtd != tam_item) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    void *p = memoria_malloc(total);
    if (p != NULL) memoria_zerar(p, total);
    return p;
}

void memoria_free(void *ptr) {
    if (ptr == NULL) return;

    struct block *bloco = (struct block *)((char *)ptr - sizeof(struct block));

    if (bloco->magia == CROCK_BLOCO_MAGIA_LIVRE) {
        crock_falha(CROCK_ERRO_USE_AFTER_FREE);
        return;
    }
    if (bloco->magia != CROCK_BLOCO_MAGIA_OK) {
        crock_falha(CROCK_ERRO_USE_AFTER_FREE);
        return;
    }

#ifdef CROCK_DEBUG
    unsigned char *lixo = (unsigned char *)ptr;
    static const unsigned char padrao[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    for (size_t i = 0; i < bloco->size; i++) lixo[i] = padrao[i % 4];
#endif

    bloco->free = 1;
    bloco->magia = CROCK_BLOCO_MAGIA_LIVRE;

    if (bloco->next != NULL && bloco->next->free) {
        free_list_remove(bloco->next);
        bloco->size += sizeof(struct block) + bloco->next->size;
        bloco->next = bloco->next->next;
        if (bloco->next != NULL) bloco->next->prev = bloco;
    }

    if (bloco->prev != NULL && bloco->prev->free) {
        free_list_remove(bloco->prev);
        bloco->prev->size += sizeof(struct block) + bloco->size;
        bloco->prev->next = bloco->next;
        if (bloco->next != NULL) bloco->next->prev = bloco->prev;
        free_list_add(bloco->prev);
    } else {
        free_list_add(bloco);
    }
}

void *memoria_realloc(void *ptr, size_t novo_tam) {
    if (ptr == NULL) return memoria_malloc(novo_tam);
    if (novo_tam == 0) { memoria_free(ptr); return NULL; }

    struct block *bloco = (struct block *)((char *)ptr - sizeof(struct block));
    if (bloco->magia != CROCK_BLOCO_MAGIA_OK) {
        return crock_falha_ptr(CROCK_ERRO_USE_AFTER_FREE);
    }
    size_t alinhado = (novo_tam + 7) & ~(size_t)7;
    if (bloco->size >= alinhado) return ptr;

    struct arena *a = bloco->dono;
    if (bloco->next != NULL && bloco->next->free &&
        bloco->size + sizeof(struct block) + bloco->next->size >= alinhado) {
        struct block *prox = bloco->next;
        free_list_remove(prox);
        size_t total = bloco->size + sizeof(struct block) + prox->size;
        bloco->next = prox->next;
        if (bloco->next != NULL) bloco->next->prev = bloco;

        if (total >= alinhado + sizeof(struct block) + 8) {
            struct block *sobra = (struct block *)((char *)bloco + sizeof(struct block) + alinhado);
            sobra->size = total - alinhado - sizeof(struct block);
            sobra->free = 1;
            sobra->magia = CROCK_BLOCO_MAGIA_LIVRE;
            sobra->next = bloco->next;
            sobra->prev = bloco;
            sobra->dono = a;
            if (bloco->next != NULL) bloco->next->prev = sobra;
            bloco->next = sobra;
            free_list_add(sobra);
            bloco->size = alinhado;
        } else {
            bloco->size = total;
        }
        return ptr;
    }

    void *novo = memoria_malloc(novo_tam);
    if (novo == NULL) return NULL;
    memoria_copia(novo, ptr, bloco->size);
    memoria_free(ptr);
    return novo;
}
