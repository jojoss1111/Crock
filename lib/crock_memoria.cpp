#define CROCK_MEMORIA_IMPLEMENTACAO
#include "crock_interno.h"
#include "crock.hpp" // crock::LockGuard -- única mudança de comportamento deste arquivo

struct block {
    size_t size;
    size_t solicitado;
    unsigned int magia;
    struct block *next;
    struct block *prev;
    struct block *free_next;
    struct block *free_prev;
    struct arena *dono;
#ifdef CROCK_RASTREAR_ALOCACOES
    const char *rastreio_arquivo;
    int rastreio_linha;
#endif
};

#define CROCK_BLOCO_MAGIA_OK      0xC20C0001u
#define CROCK_BLOCO_MAGIA_LIVRE   0xDEADC0DEu
#define CROCK_BLOCO_MAGIA_TCACHE  0xCACE0002u

static unsigned int crock_canario_semente = 0;

static unsigned int crock_canario(struct block *b, unsigned int base) {
    return base ^ (unsigned int)(size_t)b ^ crock_canario_semente;
}

#define CROCK_SMALL_BIN_LIMIT 1024
#define CROCK_SMALL_BINS (CROCK_SMALL_BIN_LIMIT / 8)
#define CROCK_CLASSES 192

#define CROCK_MMAP_DIRETO_LIMITE (256 * 1024)
#define CROCK_PAGINA_TAM 4096

#define CROCK_REDZONE_FRENTE 8
#define CROCK_REDZONE_TRAS   8
#define CROCK_REDZONE_BYTE_FRENTE 0xAB
#define CROCK_REDZONE_BYTE_TRAS   0xAA

#define CROCK_REDZONE_PADRAO_FRENTE ((uint64_f)0xABABABABABABABABull)
#define CROCK_REDZONE_PADRAO_TRAS   ((uint64_f)0xAAAAAAAAAAAAAAAAull)

#if defined(_MSC_VER)
#define CROCK_THREAD_LOCAL __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define CROCK_THREAD_LOCAL _Thread_local
#else
#define CROCK_THREAD_LOCAL __thread
#endif

#define CROCK_TCACHE_MAX_POR_BIN 64

typedef struct {
    struct block *topo;
    int qtd;
} crock_tcache_bin;

static CROCK_THREAD_LOCAL crock_tcache_bin tcache_bins[CROCK_CLASSES];

struct arena {
    char *base;
    size_t tamanho;
    struct arena *next;
};

static struct arena *arena_primeira = NULL;

typedef struct {
    size_t inicio;
    size_t fim;
    struct arena *arena;
} ArenaFaixa;

static ArenaFaixa *arena_faixas = NULL;
static int arena_faixas_qtd = 0;
static int arena_faixas_cap = 0;

static int arena_faixas_add(struct arena *a) {
    if (arena_faixas_qtd == arena_faixas_cap) {
        int nova_cap = arena_faixas_cap == 0 ? 16 : arena_faixas_cap * 2;
        size_t tam_bytes = (size_t)nova_cap * sizeof(ArenaFaixa);
        void *novo = crock_plat_memoria_reservar(tam_bytes);
        if (novo == NULL) return 0;
        if (arena_faixas != NULL) {
            memoria_copia(novo, arena_faixas, (size_t)arena_faixas_qtd * sizeof(ArenaFaixa));
            crock_plat_memoria_liberar(arena_faixas, (size_t)arena_faixas_cap * sizeof(ArenaFaixa));
        }
        arena_faixas = (ArenaFaixa *)novo;
        arena_faixas_cap = nova_cap;
    }

    size_t inicio = (size_t)a->base + sizeof(struct arena) + sizeof(struct block) + CROCK_REDZONE_FRENTE;
    size_t fim = (size_t)a->base + a->tamanho;

    int i = arena_faixas_qtd;
    while (i > 0 && arena_faixas[i - 1].inicio > inicio) {
        arena_faixas[i] = arena_faixas[i - 1];
        i--;
    }
    arena_faixas[i].inicio = inicio;
    arena_faixas[i].fim = fim;
    arena_faixas[i].arena = a;
    arena_faixas_qtd++;
    return 1;
}

static void arena_faixas_remover(struct arena *a) {
    for (int i = 0; i < arena_faixas_qtd; i++) {
        if (arena_faixas[i].arena == a) {
            for (int j = i; j < arena_faixas_qtd - 1; j++) arena_faixas[j] = arena_faixas[j + 1];
            arena_faixas_qtd--;
            return;
        }
    }
}

static int arena_faixas_contem(size_t alvo) {
    int lo = 0, hi = arena_faixas_qtd - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (alvo < arena_faixas[mid].inicio) hi = mid - 1;
        else if (alvo >= arena_faixas[mid].fim) lo = mid + 1;
        else return 1;
    }
    return 0;
}

static void *bloco_para_ptr(struct block *b);

struct direto_no {
    struct block *bloco;
    size_t reservado;
    struct direto_no *next;
};
static struct direto_no *direto_lista = NULL;

static void direto_registrar(struct block *b, size_t reservado) {
    struct direto_no *no = (struct direto_no *)crock_plat_memoria_reservar(sizeof(struct direto_no));
    if (no == NULL) return;
    no->bloco = b;
    no->reservado = reservado;
    no->next = direto_lista;
    direto_lista = no;
}

static void direto_remover(struct block *b) {
    struct direto_no *ant = NULL, *at = direto_lista;
    while (at != NULL) {
        if (at->bloco == b) {
            if (ant != NULL) ant->next = at->next; else direto_lista = at->next;
            crock_plat_memoria_liberar(at, sizeof(struct direto_no));
            return;
        }
        ant = at;
        at = at->next;
    }
}

static int validar_ponteiro(void *ptr) {
    if (ptr == NULL) return 0;

    size_t alvo = (size_t)ptr;

    if (arena_faixas_contem(alvo)) return 1;

    for (struct direto_no *no = direto_lista; no != NULL; no = no->next) {
        if (bloco_para_ptr(no->bloco) == ptr) return 1;
    }

    return 0;
}

static struct block *free_bins[CROCK_CLASSES];

#define CROCK_BITMAP_PALAVRAS ((CROCK_CLASSES + 63) / 64)
static uint64_f free_bitmap[CROCK_BITMAP_PALAVRAS];

static void bitmap_marcar(int bin) {
    free_bitmap[bin / 64] |= (uint64_f)1 << (bin % 64);
}

static void bitmap_desmarcar(int bin) {
    free_bitmap[bin / 64] &= ~((uint64_f)1 << (bin % 64));
}

static int bitmap_proximo(int inicio) {
    int palavra = inicio / 64;
    if (palavra >= CROCK_BITMAP_PALAVRAS) return -1;

    uint64_f mascara = free_bitmap[palavra] & (~(uint64_f)0 << (inicio % 64));
    for (;;) {
        if (mascara != 0) {
            int bin = palavra * 64 + __builtin_ctzll((unsigned long long)mascara);
            return bin < CROCK_CLASSES ? bin : -1;
        }
        palavra++;
        if (palavra >= CROCK_BITMAP_PALAVRAS) return -1;
        mascara = free_bitmap[palavra];
    }
}

static volatile int crock_heap_lock = 0;

static inline void crock_cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#else
    (void)0;
#endif
}

void crock_lock(void) {
    if (!__atomic_test_and_set(&crock_heap_lock, __ATOMIC_ACQUIRE)) return;

    unsigned int giros = 0;
    for (;;) {
        while (__atomic_load_n(&crock_heap_lock, __ATOMIC_RELAXED)) {
            crock_cpu_relax();
            if (++giros >= 4000) {
                crock_plat_dormir_ns(1);
                giros = 0;
            }
        }
        if (!__atomic_test_and_set(&crock_heap_lock, __ATOMIC_ACQUIRE)) return;
    }
}

void crock_unlock(void) {
    __atomic_clear(&crock_heap_lock, __ATOMIC_RELEASE);
}

static size_t bloco_reservado_calc(size_t tam_alinhado) {
    size_t total = sizeof(struct block) + tam_alinhado;
    if (total < tam_alinhado) { crock_falha(CROCK_ERRO_TAM_INVALIDO); return 0; }
    return (total + CROCK_PAGINA_TAM - 1) & ~(size_t)(CROCK_PAGINA_TAM - 1);
}

static void memoria_decommitar_intervalo(void *inicio, void *fim) {
    size_t ini = ((size_t)inicio + CROCK_PAGINA_TAM - 1) & ~(size_t)(CROCK_PAGINA_TAM - 1);
    size_t fv  = (size_t)fim & ~(size_t)(CROCK_PAGINA_TAM - 1);
    if (fv > ini) crock_plat_memoria_decommit((void *)ini, fv - ini);
}

#define CROCK_DECOMMIT_LIMITE_MINIMO (64 * 1024)

static int free_bin(size_t tam) {
    if (tam <= CROCK_SMALL_BIN_LIMIT) return (int)((tam + 7) / 8) - 1;

#if defined(__GNUC__) || defined(__clang__)
    int expoente = (int)(sizeof(size_t) * 8 - 1 - __builtin_clzll((unsigned long long)tam));
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

    if (bin < CROCK_SMALL_BINS) {
        b->free_prev = NULL;
        b->free_next = free_bins[bin];
        if (free_bins[bin] != NULL) free_bins[bin]->free_prev = b;
        free_bins[bin] = b;
    } else {
        struct block *atual = free_bins[bin];
        struct block *anterior = NULL;
        while (atual != NULL && atual->size > b->size) {
            anterior = atual;
            atual = atual->free_next;
        }
        b->free_prev = anterior;
        b->free_next = atual;
        if (atual != NULL) atual->free_prev = b;
        if (anterior != NULL) anterior->free_next = b;
        else free_bins[bin] = b;
    }

    bitmap_marcar(bin);
}

static void free_list_remove(struct block *b) {
    int bin = free_bin(b->size);
    if (b->free_prev != NULL) b->free_prev->free_next = b->free_next;
    else free_bins[bin] = b->free_next;
    if (b->free_next != NULL) b->free_next->free_prev = b->free_prev;
    b->free_next = NULL;
    b->free_prev = NULL;
    if (free_bins[bin] == NULL) bitmap_desmarcar(bin);
}

static int heap_initialized = 0;

static void crock_canario_semear_se_necessario(void) {
    if (crock_canario_semente != 0) return;
    uint64_f agora = (uint64_f)crock_plat_relogio_ns();
    crock_canario_semente = (unsigned int)(agora ^ (agora >> 32));
    if (crock_canario_semente == 0) crock_canario_semente = 0x9E3779B9u;
}

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
    a->next = NULL;

    struct block *b = (struct block *)((char *)mem + sizeof(struct arena));
    b->size = tam - sizeof(struct arena) - sizeof(struct block);
    b->magia = crock_canario(b, CROCK_BLOCO_MAGIA_LIVRE);
    b->next = NULL;
    b->prev = NULL;
    b->dono = a;
    b->free_next = NULL;
    b->free_prev = NULL;

    free_list_add(b);

    if (arena_primeira == NULL) {
        arena_primeira = a;
    } else {
        a->next = arena_primeira;
        arena_primeira = a;
    }

    arena_faixas_add(a);

    return a;
}

static void heap_inicializar_se_necessario(void) {
    if (heap_initialized) return;
    crock_canario_semear_se_necessario();
    nova_arena(0);
    heap_initialized = 1;
}

void init_heap(void) {
    crock::LockGuard guarda;
    heap_inicializar_se_necessario();
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

static void *bloco_para_ptr(struct block *b) {
    return (void *)((char *)b + sizeof(struct block) + CROCK_REDZONE_FRENTE);
}

static struct block *ptr_para_bloco(void *ptr) {
    return (struct block *)((char *)ptr - CROCK_REDZONE_FRENTE - sizeof(struct block));
}

static void redzone_escrever(struct block *b) {
    uint64_f padrao;

    padrao = CROCK_REDZONE_PADRAO_FRENTE;
    __builtin_memcpy((char *)b + sizeof(struct block), &padrao, sizeof(padrao));

    padrao = CROCK_REDZONE_PADRAO_TRAS;
    __builtin_memcpy((char *)bloco_para_ptr(b) + b->solicitado, &padrao, sizeof(padrao));
}

static int redzone_checar(struct block *b) {
    uint64_f valor;

    __builtin_memcpy(&valor, (char *)b + sizeof(struct block), sizeof(valor));
    if (valor != CROCK_REDZONE_PADRAO_FRENTE) return 0;

    __builtin_memcpy(&valor, (char *)bloco_para_ptr(b) + b->solicitado, sizeof(valor));
    if (valor != CROCK_REDZONE_PADRAO_TRAS) return 0;

    return 1;
}

static struct block *malloc_de_bloco(struct arena *a, struct block *atual, size_t tam) {
    free_list_remove(atual);
    if (atual->size >= tam + sizeof(struct block) + 8) {
        struct block *novo = (struct block *)((char *)atual + sizeof(struct block) + tam);
        novo->size = atual->size - tam - sizeof(struct block);
        novo->magia = crock_canario(novo, CROCK_BLOCO_MAGIA_LIVRE);
        novo->next = atual->next;
        novo->prev = atual;
        novo->dono = a;
        if (atual->next != NULL) atual->next->prev = novo;

        atual->size = tam;
        atual->next = novo;
        free_list_add(novo);
    }
    atual->magia = crock_canario(atual, CROCK_BLOCO_MAGIA_OK);
#ifdef CROCK_RASTREAR_ALOCACOES
    atual->rastreio_arquivo = NULL;
    atual->rastreio_linha = 0;
#endif
    return atual;
}

#ifdef CROCK_STATS
static uint64_f crock_stat_malloc_chamadas = 0;
static uint64_f crock_stat_bins_visitados  = 0;
static uint64_f crock_stat_nos_visitados   = 0;
static uint64_f crock_stat_nova_arena      = 0;
#define CROCK_STAT_INC(x)      ((x)++)
#define CROCK_STAT_ADD(x, n)   ((x) += (n))
#else
#define CROCK_STAT_INC(x)      ((void)0)
#define CROCK_STAT_ADD(x, n)   ((void)0)
#endif

void crock_memoria_stats(uint64_f *chamadas, uint64_f *bins_visitados,
                          uint64_f *nos_visitados, uint64_f *novas_arenas) {
#ifdef CROCK_STATS
    if (chamadas)      *chamadas      = crock_stat_malloc_chamadas;
    if (bins_visitados) *bins_visitados = crock_stat_bins_visitados;
    if (nos_visitados)  *nos_visitados  = crock_stat_nos_visitados;
    if (novas_arenas)   *novas_arenas   = crock_stat_nova_arena;
#else
    if (chamadas)       *chamadas       = 0;
    if (bins_visitados) *bins_visitados = 0;
    if (nos_visitados)  *nos_visitados  = 0;
    if (novas_arenas)   *novas_arenas   = 0;
#endif
}

static struct block *malloc_na_arena(size_t tam) {
    CROCK_STAT_INC(crock_stat_malloc_chamadas);

    int bin = free_bin(tam);
    for (;;) {
        bin = bitmap_proximo(bin);
        if (bin < 0) return NULL;

        CROCK_STAT_INC(crock_stat_bins_visitados);

        struct block *atual = free_bins[bin];
        if (atual == NULL || atual->size < tam) {
            bin++;
            continue;
        }

        while (atual != NULL) {
            CROCK_STAT_INC(crock_stat_nos_visitados);
            struct block *proximo = atual->free_next;
            if (atual->size >= tam) return malloc_de_bloco(atual->dono, atual, tam);
            atual = proximo;
        }
        bin++;
    }
}

static void *memoria_malloc_direto(size_t tam_alinhado, size_t tam_usuario) {
    size_t total = sizeof(struct block) + tam_alinhado;
    if (total < tam_alinhado) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);
    size_t reservar = (total + CROCK_PAGINA_TAM - 1) & ~(size_t)(CROCK_PAGINA_TAM - 1);

    void *mem = crock_plat_memoria_reservar(reservar);
    if (mem == NULL) return crock_falha_ptr(CROCK_ERRO_MEMORIA);

    struct block *b = (struct block *)mem;
    b->size = tam_alinhado;
    b->solicitado = tam_usuario;
    b->magia = crock_canario(b, CROCK_BLOCO_MAGIA_OK);
    b->next = NULL;
    b->prev = NULL;
    b->free_next = NULL;
    b->free_prev = NULL;
    b->dono = NULL;
#ifdef CROCK_RASTREAR_ALOCACOES
    b->rastreio_arquivo = NULL;
    b->rastreio_linha = 0;
#endif
    redzone_escrever(b);
    direto_registrar(b, reservar);
    return bloco_para_ptr(b);
}

static void *malloc_interno(size_t tam_usuario) {
    heap_inicializar_se_necessario();

    if (tam_usuario == 0) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);
    size_t overhead_redzones = CROCK_REDZONE_FRENTE + CROCK_REDZONE_TRAS;
    if (tam_usuario > (size_t)-1 - overhead_redzones - 7) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    size_t tam = (tam_usuario + overhead_redzones + 7) & ~(size_t)7;

    if (tam >= CROCK_MMAP_DIRETO_LIMITE) return memoria_malloc_direto(tam, tam_usuario);

    struct block *b = malloc_na_arena(tam);
    if (b == NULL) {
        CROCK_STAT_INC(crock_stat_nova_arena);
        struct arena *nova = nova_arena(tam);
        if (nova == NULL) return crock_falha_ptr(CROCK_ERRO_MEMORIA);
        b = malloc_na_arena(tam);
    }
    if (b == NULL) return crock_falha_ptr(CROCK_ERRO_MEMORIA);

    b->solicitado = tam_usuario;
    redzone_escrever(b);
    return bloco_para_ptr(b);
}

static struct block *tcache_tentar_malloc(size_t tam) {
    if (tam >= CROCK_MMAP_DIRETO_LIMITE) return NULL;

    crock_tcache_bin *tb = &tcache_bins[free_bin(tam)];
    struct block *atual = tb->topo;
    if (atual == NULL || atual->size < tam) return NULL;

    tb->topo = atual->free_next;
    tb->qtd--;
    atual->magia = crock_canario(atual, CROCK_BLOCO_MAGIA_OK);
    return atual;
}

void *memoria_malloc(size_t tam_usuario) {

    if (tam_usuario != 0) {
        size_t overhead_redzones = CROCK_REDZONE_FRENTE + CROCK_REDZONE_TRAS;
        if (tam_usuario <= (size_t)-1 - overhead_redzones - 7) {
            size_t tam = (tam_usuario + overhead_redzones + 7) & ~(size_t)7;
            struct block *b = tcache_tentar_malloc(tam);
            if (b != NULL) {
                b->solicitado = tam_usuario;
                redzone_escrever(b);
                return bloco_para_ptr(b);
            }
        }
    }

    crock::LockGuard guarda;
    return malloc_interno(tam_usuario);
}

void *memoria_calloc(size_t qtd, size_t tam_item) {
    if (qtd == 0 || tam_item == 0) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    size_t total = qtd * tam_item;
    if (total / qtd != tam_item) return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);

    void *p = memoria_malloc(total);
    if (p != NULL) memoria_zerar(p, total);
    return p;
}

static int checar_bloco_interno(struct block *bloco) {
    if (bloco->magia == crock_canario(bloco, CROCK_BLOCO_MAGIA_LIVRE)) return -1;
    if (bloco->magia != crock_canario(bloco, CROCK_BLOCO_MAGIA_OK)) return -1;
    return redzone_checar(bloco) ? 1 : 0;
}

int memoria_checar(void *ptr) {
    if (ptr == NULL) return 1;

    int r;
    {
        crock::LockGuard guarda;
        if (!validar_ponteiro(ptr)) {
            crock_falha(CROCK_ERRO_INDICE);
            return -1; // guarda solta o lock sozinha, mesmo saindo aqui
        }
        struct block *bloco = ptr_para_bloco(ptr);
        r = checar_bloco_interno(bloco);
    }

    if (r < 0) { crock_falha(CROCK_ERRO_USE_AFTER_FREE); return -1; }
    if (r == 0) { crock_falha(CROCK_ERRO_MEMORIA); return 0; }
    return 1;
}

int memoria_checar_tudo(void) {
    int corrompidos = 0;

    {
        crock::LockGuard guarda;
        for (struct arena *a = arena_primeira; a != NULL; a = a->next) {
            struct block *b = (struct block *)((char *)a->base + sizeof(struct arena));
            while (b != NULL) {
                if (b->magia == crock_canario(b, CROCK_BLOCO_MAGIA_OK)) {
                    if (checar_bloco_interno(b) <= 0) {
                        corrompidos++;
#ifdef CROCK_DEBUG
                        saida_str_fd(SAIDA_STDERR, "[crock] bloco corrompido em 0x");
                        saida_uint_fd(SAIDA_STDERR, (uint64_f)(size_t)bloco_para_ptr(b), 16, 1);
                        saida_str_fd(SAIDA_STDERR, " tamanho ");
                        saida_uint_fd(SAIDA_STDERR, (uint64_f)b->solicitado, 10, 0);
                        saida_str_fd(SAIDA_STDERR, "\n");
#endif
                    }
                }
                b = b->next;
            }
        }
    }

    return corrompidos;
}

// libera um bloco já validado pelo chamador (magia/redzone conferidos).
// não repete a validação -- todo call site já passou por checar_bloco_interno
// (ou equivalente) antes de chegar aqui.
static void free_interno(void *ptr) {
    if (ptr == NULL) return;

    struct block *bloco = ptr_para_bloco(ptr);

    if (bloco->dono == NULL) {
        direto_remover(bloco);
        crock_plat_memoria_liberar(bloco, bloco_reservado_calc(bloco->size));
        return;
    }

#ifdef CROCK_DEBUG
    unsigned char *lixo = (unsigned char *)ptr;
    static const unsigned char padrao[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    for (size_t i = 0; i < bloco->size; i++) lixo[i] = padrao[i % 4];
#endif

    bloco->magia = crock_canario(bloco, CROCK_BLOCO_MAGIA_LIVRE);

    struct arena *a = bloco->dono;

    if (bloco->next != NULL &&
        bloco->next->magia == crock_canario(bloco->next, CROCK_BLOCO_MAGIA_LIVRE)) {
        free_list_remove(bloco->next);
        bloco->size += sizeof(struct block) + bloco->next->size;
        bloco->next = bloco->next->next;
        if (bloco->next != NULL) bloco->next->prev = bloco;
    }

    struct block *final = bloco;
    if (bloco->prev != NULL &&
        bloco->prev->magia == crock_canario(bloco->prev, CROCK_BLOCO_MAGIA_LIVRE)) {
        free_list_remove(bloco->prev);
        bloco->prev->size += sizeof(struct block) + bloco->size;
        bloco->prev->next = bloco->next;
        if (bloco->next != NULL) bloco->next->prev = bloco->prev;
        final = bloco->prev;
    }

    final->magia = crock_canario(final, CROCK_BLOCO_MAGIA_LIVRE);

    if (a != arena_primeira && final->prev == NULL && final->next == NULL &&
        final->size == a->tamanho - sizeof(struct arena) - sizeof(struct block)) {

        if (arena_primeira == a) {
            arena_primeira = a->next;
        } else {
            for (struct arena *p = arena_primeira; p != NULL; p = p->next) {
                if (p->next == a) { p->next = a->next; break; }
            }
        }
        arena_faixas_remover(a);
        crock_plat_memoria_liberar(a, a->tamanho);
        return;
    }

#ifndef CROCK_DEBUG
    if (final->size >= CROCK_DECOMMIT_LIMITE_MINIMO) {
        memoria_decommitar_intervalo((char *)final + sizeof(struct block),
                                      (char *)final + sizeof(struct block) + final->size);
    }
#endif

    free_list_add(final);
}

void memoria_free(void *ptr) {
    if (ptr == NULL) return;

    int ok;
    {
        crock::LockGuard guarda;
        ok = validar_ponteiro(ptr);
    }
    if (!ok) { crock_falha(CROCK_ERRO_INDICE); return; }

    struct block *bloco = ptr_para_bloco(ptr);

    int estado = checar_bloco_interno(bloco);
    if (estado < 0) { crock_falha(CROCK_ERRO_USE_AFTER_FREE); return; }
    if (estado == 0) { crock_falha(CROCK_ERRO_MEMORIA); return; }

    if (bloco->dono == NULL) {
        free_interno(ptr);
        return;
    }

    if (bloco->size < CROCK_MMAP_DIRETO_LIMITE) {
        int bin = free_bin(bloco->size);
        crock_tcache_bin *tb = &tcache_bins[bin];
        if (tb->qtd < CROCK_TCACHE_MAX_POR_BIN) {
            bloco->free_next = tb->topo;
            tb->topo = bloco;
            tb->qtd++;
            bloco->magia = crock_canario(bloco, CROCK_BLOCO_MAGIA_TCACHE);
            return;
        }
    }

    crock::LockGuard guarda;
    free_interno(ptr);
}

#ifdef CROCK_RASTREAR_ALOCACOES
// copia o arquivo/linha de origem pro bloco recém-realocado (usado nos 3
// caminhos de realloc que precisam mover o conteúdo pra um bloco novo)
static void realloc_copiar_rastreio(void *novo, struct block *origem) {
    struct block *b = ptr_para_bloco(novo);
    b->rastreio_arquivo = origem->rastreio_arquivo;
    b->rastreio_linha   = origem->rastreio_linha;
}
#define CROCK_REALLOC_COPIAR_RASTREIO(novo, origem) realloc_copiar_rastreio((novo), (origem))
#else
#define CROCK_REALLOC_COPIAR_RASTREIO(novo, origem) ((void)0)
#endif

static void *realloc_interno(void *ptr, size_t novo_tam) {
    if (ptr == NULL) return malloc_interno(novo_tam);
    if (!validar_ponteiro(ptr)) return crock_falha_ptr(CROCK_ERRO_INDICE);
    if (novo_tam == 0) { free_interno(ptr); return NULL; }

    struct block *bloco = ptr_para_bloco(ptr);
    int estado = checar_bloco_interno(bloco);
    if (estado < 0) return crock_falha_ptr(CROCK_ERRO_USE_AFTER_FREE);
    if (estado == 0) return crock_falha_ptr(CROCK_ERRO_MEMORIA);

    size_t overhead_redzones = CROCK_REDZONE_FRENTE + CROCK_REDZONE_TRAS;
    if (novo_tam > (size_t)-1 - overhead_redzones - 7) {
        return crock_falha_ptr(CROCK_ERRO_TAM_INVALIDO);
    }
    size_t alinhado_usuario = novo_tam;
    size_t alinhado = (novo_tam + overhead_redzones + 7) & ~(size_t)7;

    if (bloco->dono == NULL) {
        size_t reservado_atual = bloco_reservado_calc(bloco->size);
        if (sizeof(struct block) + alinhado <= reservado_atual) {
            bloco->size = alinhado;
            bloco->solicitado = alinhado_usuario;
            redzone_escrever(bloco);
            return ptr;
        }
        void *novo = malloc_interno(novo_tam);
        if (novo == NULL) return NULL;
        size_t copiar = bloco->solicitado < novo_tam ? bloco->solicitado : novo_tam;
        memoria_copia(novo, ptr, copiar);
        CROCK_REALLOC_COPIAR_RASTREIO(novo, bloco);
        direto_remover(bloco);
        crock_plat_memoria_liberar(bloco, reservado_atual);
        return novo;
    }

    if (alinhado >= CROCK_MMAP_DIRETO_LIMITE) {
        void *novo = memoria_malloc_direto(alinhado, alinhado_usuario);
        if (novo == NULL) return NULL;
        size_t copiar = bloco->solicitado < alinhado_usuario ? bloco->solicitado : alinhado_usuario;
        memoria_copia(novo, ptr, copiar);
        CROCK_REALLOC_COPIAR_RASTREIO(novo, bloco);
        free_interno(ptr);
        return novo;
    }

    if (bloco->size >= alinhado) {
        bloco->solicitado = alinhado_usuario;
        redzone_escrever(bloco);
        return ptr;
    }

    struct arena *a = bloco->dono;
    if (bloco->next != NULL &&
        bloco->next->magia == crock_canario(bloco->next, CROCK_BLOCO_MAGIA_LIVRE) &&
        bloco->size + sizeof(struct block) + bloco->next->size >= alinhado) {
        struct block *prox = bloco->next;
        free_list_remove(prox);
        size_t total = bloco->size + sizeof(struct block) + prox->size;
        bloco->next = prox->next;
        if (bloco->next != NULL) bloco->next->prev = bloco;

        if (total >= alinhado + sizeof(struct block) + 8) {
            struct block *sobra = (struct block *)((char *)bloco + sizeof(struct block) + alinhado);
            sobra->size = total - alinhado - sizeof(struct block);
            sobra->magia = crock_canario(sobra, CROCK_BLOCO_MAGIA_LIVRE);
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
        bloco->solicitado = alinhado_usuario;
        redzone_escrever(bloco);
        return ptr;
    }

    void *novo = malloc_interno(novo_tam);
    if (novo == NULL) return NULL;
    memoria_copia(novo, ptr, bloco->solicitado);
    CROCK_REALLOC_COPIAR_RASTREIO(novo, bloco);
    free_interno(ptr);
    return novo;
}

void *memoria_realloc(void *ptr, size_t novo_tam) {
    crock::LockGuard guarda;
    return realloc_interno(ptr, novo_tam);
}

#ifdef CROCK_RASTREAR_ALOCACOES
void *crock_memoria_malloc_rastreado(size_t tam, const char *arquivo, int linha) {
    void *p = memoria_malloc(tam);
    if (p != NULL) {
        struct block *b = ptr_para_bloco(p);
        b->rastreio_arquivo = arquivo;
        b->rastreio_linha = linha;
    }
    return p;
}

void *crock_memoria_calloc_rastreado(size_t qtd, size_t tam_item, const char *arquivo, int linha) {
    void *p = memoria_calloc(qtd, tam_item);
    if (p != NULL) {
        struct block *b = ptr_para_bloco(p);
        b->rastreio_arquivo = arquivo;
        b->rastreio_linha = linha;
    }
    return p;
}

int crock_memoria_relatorio_vazamentos(void) {
    int vazamentos = 0;

    {
        crock::LockGuard guarda;
        for (struct arena *a = arena_primeira; a != NULL; a = a->next) {
            struct block *b = (struct block *)((char *)a->base + sizeof(struct arena));
            while (b != NULL) {
                if (b->magia == crock_canario(b, CROCK_BLOCO_MAGIA_OK)) {
                    vazamentos++;
                    saida_str_fd(SAIDA_STDERR, "[crock] vazamento: bloco de ");
                    saida_uint_fd(SAIDA_STDERR, (uint64_f)b->solicitado, 10, 0);
                    saida_str_fd(SAIDA_STDERR, " bytes alocado em ");
                    saida_str_fd(SAIDA_STDERR, b->rastreio_arquivo ? b->rastreio_arquivo : "?");
                    saida_str_fd(SAIDA_STDERR, ":");
                    saida_int_fd(SAIDA_STDERR, (int64_f)b->rastreio_linha);
                    saida_str_fd(SAIDA_STDERR, " nunca foi liberado\n");
                }
                b = b->next;
            }
        }
        for (struct direto_no *no = direto_lista; no != NULL; no = no->next) {
            vazamentos++;
            saida_str_fd(SAIDA_STDERR, "[crock] vazamento (mmap direto): bloco de ");
            saida_uint_fd(SAIDA_STDERR, (uint64_f)no->bloco->solicitado, 10, 0);
            saida_str_fd(SAIDA_STDERR, " bytes alocado em ");
            saida_str_fd(SAIDA_STDERR, no->bloco->rastreio_arquivo ? no->bloco->rastreio_arquivo : "?");
            saida_str_fd(SAIDA_STDERR, ":");
            saida_int_fd(SAIDA_STDERR, (int64_f)no->bloco->rastreio_linha);
            saida_str_fd(SAIDA_STDERR, " nunca foi liberado\n");
        }
    }

    saida_flush(SAIDA_STDERR);
    return vazamentos;
}
#endif