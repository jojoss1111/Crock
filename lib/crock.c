#include "crock.h"

typedef __builtin_va_list va_lista;
#define va_inicio(v, ultimo) __builtin_va_start(v, ultimo)
#define va_fim(v)            __builtin_va_end(v)
#define va_prox(v, tipo)     __builtin_va_arg(v, tipo)
#define va_dup(d, s)         __builtin_va_copy(d, s)

#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define MAP_PRIVATE    0x02
#define MAP_ANONYMOUS  0x20
#define MMAP_FALHOU    ((void *)-1)

extern void *mmap(void *addr, size_t tam, int prot, int flags, int fd, long offset);

static CrockErro crock_ultimo_erro = CROCK_OK;

static int crock_falha(CrockErro e) {
    crock_ultimo_erro = e;
    return -1;
}

static void *crock_falha_ptr(CrockErro e) {
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

#define CROCK_EXIGIR(cond, erro, retorno) \
    do { if (!(cond)) { crock_ultimo_erro = (erro); return (retorno); } } while (0)

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

        void *mem = mmap(NULL, tam, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MMAP_FALHOU || mem == NULL) return NULL;

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

static void memoria_mover(void *destino, const void *origem, size_t tam) {
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

#define SAIDA_BUF_TAM 16384

typedef struct {
    int fd;
    unsigned long usado;
    char dados[SAIDA_BUF_TAM];
} SaidaBuffer;

static SaidaBuffer saida_buf_stdout = { SAIDA_STDOUT, 0, {0} };
static SaidaBuffer saida_buf_stderr = { SAIDA_STDERR, 0, {0} };

static SaidaBuffer *saida_buffer_de(int fd) {
    return (fd == SAIDA_STDERR) ? &saida_buf_stderr : &saida_buf_stdout;
}

void saida_flush(int fd) {
    SaidaBuffer *b = saida_buffer_de(fd);
    if (b->usado > 0) {
        unsigned long escrito = 0;
        while (escrito < b->usado) {
            long n = write(b->fd, b->dados + escrito, b->usado - escrito);
            if (n <= 0) break;
            escrito += (unsigned long)n;
        }
        if (escrito < b->usado) {
            memoria_mover(b->dados, b->dados + escrito, b->usado - escrito);
        }
        b->usado -= escrito;
    }
}

void saida_flush_tudo(void) {
    saida_flush(SAIDA_STDOUT);
    saida_flush(SAIDA_STDERR);
}

void saida_char_fd(int fd, char c) {
    SaidaBuffer *b = saida_buffer_de(fd);
    if (b->usado >= SAIDA_BUF_TAM) saida_flush(fd);
    b->dados[b->usado++] = c;
}

void saida_str_fd(int fd, const char *s) {
    if (!s) return;
    SaidaBuffer *b = saida_buffer_de(fd);
    while (*s) {
        unsigned long livre = SAIDA_BUF_TAM - b->usado;
        if (livre == 0) { saida_flush(fd); continue; }
        unsigned long n = 0;
        while (n < livre && s[n] != '\0') n++;
        memoria_copia(b->dados + b->usado, s, n);
        b->usado += n;
        s += n;
    }
}

typedef struct {
    void *ctx;
    void (*emite)(void *ctx, char c);
    void (*emite_n)(void *ctx, const char *s, size_t n);
} CrockEscritor;

static void escritor_char(CrockEscritor *e, char c) { e->emite(e->ctx, c); }

static void escritor_n(CrockEscritor *e, const char *s, size_t n) {
    if (n == 0) return;
    if (e->emite_n != NULL) e->emite_n(e->ctx, s, n);
    else for (size_t i = 0; i < n; i++) e->emite(e->ctx, s[i]);
}

static void escritor_str(CrockEscritor *e, const char *s) {
    if (!s) return;
    const char *inicio = s;
    while (*s) s++;
    escritor_n(e, inicio, (size_t)(s - inicio));
}

static void escritor_uint(CrockEscritor *e, uint64_f valor, int base, int maiusculo) {
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

static void escritor_int(CrockEscritor *e, int64_f valor) {
    if (valor < 0) {
        escritor_char(e, '-');
        uint64_f abs_valor = (uint64_f)(~((uint64_f)valor)) + 1ULL;
        escritor_uint(e, abs_valor, 10, 0);
    } else {
        escritor_uint(e, (uint64_f)valor, 10, 0);
    }
}

static void escritor_float(CrockEscritor *e, double valor, int casas) {
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

static void escritor_vfmt(CrockEscritor *e, const char *formato, va_lista args) {
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

static void emite_fd(void *ctx, char c) { saida_char_fd(*(int *)ctx, c); }
static void emite_fd_n(void *ctx, const char *s, size_t n) {
    int fd = *(int *)ctx;
    SaidaBuffer *b = saida_buffer_de(fd);
    while (n > 0) {
        unsigned long livre = SAIDA_BUF_TAM - b->usado;
        if (livre == 0) { saida_flush(fd); continue; }
        size_t parte = n < livre ? n : livre;
        memoria_copia(b->dados + b->usado, s, parte);
        b->usado += parte;
        s += parte;
        n -= parte;
    }
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
    write(1, escape, txt_tam(escape));
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

extern long read(int fd, void *buf, unsigned long contagem);

#define ENTRADA_BUF_TAM 256

static char entrada_buf[ENTRADA_BUF_TAM];
static unsigned long entrada_buf_pos = 0;
static unsigned long entrada_buf_len = 0;

static int entrada_getchar(void) {
    if (entrada_buf_pos >= entrada_buf_len) {
        long n = read(0, entrada_buf, ENTRADA_BUF_TAM);
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

extern int  open(const char *caminho, int flags, ...);
extern int  close(int fd);

#define ARQ_O_RDONLY 0
#define ARQ_O_WRONLY 1
#define ARQ_O_CREAT  0100
#define ARQ_O_TRUNC  01000

char *arquivo_ler_tudo(const char *caminho) {
    if (caminho == NULL) { crock_falha(CROCK_ERRO_NULO); return NULL; }
    size_t capacidade = 4096;
    size_t tamanho = 0;
    char *conteudo = (char *)memoria_malloc(capacidade);
    if (conteudo == NULL) return NULL;

    int fd = open(caminho, ARQ_O_RDONLY);
    if (fd < 0) { memoria_free(conteudo); crock_falha(CROCK_ERRO_ARQUIVO); return NULL; }

    char buf[4096];
    long n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        size_t lido = (size_t)n;
        if (lido > (size_t)-1 - tamanho - 1) {
            close(fd); memoria_free(conteudo); crock_falha(CROCK_ERRO_TAM_INVALIDO); return NULL;
        }
        size_t necessario = tamanho + lido + 1;
        if (necessario > capacidade) {
            size_t nova = capacidade;
            while (nova < necessario) {
                if (nova > (size_t)-1 / 2) { nova = necessario; break; }
                nova *= 2;
            }
            char *novo = (char *)memoria_realloc(conteudo, nova);
            if (novo == NULL) { close(fd); memoria_free(conteudo); return NULL; }
            conteudo = novo;
            capacidade = nova;
        }
        memoria_copia(conteudo + tamanho, buf, lido);
        tamanho += lido;
    }

    close(fd);
    if (n < 0) { memoria_free(conteudo); crock_falha(CROCK_ERRO_ARQUIVO); return NULL; }
    conteudo[tamanho] = '\0';
    return conteudo;
}

int arquivo_escrever_tudo(const char *caminho, const char *conteudo) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(conteudo != NULL, CROCK_ERRO_NULO, -1);
    int fd = open(caminho, ARQ_O_WRONLY | ARQ_O_CREAT | ARQ_O_TRUNC, 0644);
    if (fd < 0) return crock_falha(CROCK_ERRO_ARQUIVO);

    size_t tam = txt_tam(conteudo);
    size_t escrito = 0;
    while (escrito < tam) {
        long n = write(fd, conteudo + escrito, tam - escrito);
        if (n <= 0) { close(fd); return crock_falha(CROCK_ERRO_ARQUIVO); }
            escrito += (size_t)n;
    }

    close(fd);
    return 0;
}

int arquivo_existe(const char *caminho) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, 0);
    int fd = open(caminho, ARQ_O_RDONLY);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

static const char MEMORIA_SALVA_MAGIA[4] = { 'C', 'R', 'S', 'V' };
#define MEMORIA_IO_BUF_TAM 16384

typedef struct {
    int fd;
    size_t usado;
    char dados[MEMORIA_IO_BUF_TAM];
} MemoriaWriter;

typedef struct {
    int fd;
    size_t pos;
    size_t tamanho;
    char dados[MEMORIA_IO_BUF_TAM];
} MemoriaReader;

static MemoriaWriter memoria_writer;
static MemoriaReader memoria_reader;
static int memoria_writer_ativo = 0;
static int memoria_reader_ativo = 0;

static int memoria_escreve_bruto(int fd, const void *dados, size_t tam) {
    const char *p = (const char *)dados;
    size_t escrito = 0;
    while (escrito < tam) {
        long n = write(fd, p + escrito, tam - escrito);
        if (n <= 0) return 0;
        escrito += (size_t)n;
    }
    return 1;
}

static int memoria_flush_writer(void) {
    if (!memoria_writer_ativo || memoria_writer.usado == 0) return 1;
    if (!memoria_escreve_bruto(memoria_writer.fd, memoria_writer.dados, memoria_writer.usado)) return 0;
    memoria_writer.usado = 0;
    return 1;
}

static int memoria_escreve_bin(int fd, const void *dados, size_t tam) {
    if (!memoria_writer_ativo || memoria_writer.fd != fd) return memoria_escreve_bruto(fd, dados, tam);
    const char *p = (const char *)dados;
    while (tam > 0) {
        size_t livre = MEMORIA_IO_BUF_TAM - memoria_writer.usado;
        if (livre == 0 && !memoria_flush_writer()) return 0;
        livre = MEMORIA_IO_BUF_TAM - memoria_writer.usado;
        if (tam >= MEMORIA_IO_BUF_TAM && memoria_writer.usado == 0) {
            return memoria_escreve_bruto(fd, p, tam);
        }
        size_t parte = tam < livre ? tam : livre;
        memoria_copia(memoria_writer.dados + memoria_writer.usado, p, parte);
        memoria_writer.usado += parte;
        p += parte;
        tam -= parte;
    }
    return 1;
}

static int memoria_le_bin(int fd, void *dest, size_t tam) {
    if (!memoria_reader_ativo || memoria_reader.fd != fd) {
        char *p = (char *)dest;
        size_t lido = 0;
        while (lido < tam) {
            long n = read(fd, p + lido, tam - lido);
            if (n <= 0) return 0;
            lido += (size_t)n;
        }
        return 1;
    }
    char *p = (char *)dest;
    while (tam > 0) {
        if (memoria_reader.pos == memoria_reader.tamanho) {
            long n = read(fd, memoria_reader.dados, MEMORIA_IO_BUF_TAM);
            if (n <= 0) return 0;
            memoria_reader.pos = 0;
            memoria_reader.tamanho = (size_t)n;
        }
        size_t disponivel = memoria_reader.tamanho - memoria_reader.pos;
        size_t parte = tam < disponivel ? tam : disponivel;
        memoria_copia(p, memoria_reader.dados + memoria_reader.pos, parte);
        memoria_reader.pos += parte;
        p += parte;
        tam -= parte;
    }
    return 1;
}

int memoria_salva(const char *caminho, const char *formato, ...) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(formato != NULL, CROCK_ERRO_NULO, -1);
    int fd = open(caminho, ARQ_O_WRONLY | ARQ_O_CREAT | ARQ_O_TRUNC, 0644);
    if (fd < 0) return crock_falha(CROCK_ERRO_ARQUIVO);
    memoria_writer.fd = fd;
    memoria_writer.usado = 0;
    memoria_writer_ativo = 1;

    int ok = memoria_escreve_bin(fd, MEMORIA_SALVA_MAGIA, sizeof(MEMORIA_SALVA_MAGIA));

    uint32_f tam_formato = (uint32_f)txt_tam(formato);
    ok = ok && memoria_escreve_bin(fd, &tam_formato, sizeof(tam_formato));
    ok = ok && memoria_escreve_bin(fd, formato, tam_formato);

    va_lista args;
    va_inicio(args, formato);

    for (const char *f = formato; ok && *f != '\0'; f++) {
        switch (*f) {
            case 'i': {
                int v = va_prox(args, int);
                ok = memoria_escreve_bin(fd, &v, sizeof(v));
                break;
            }
            case 'l': {
                long v = va_prox(args, long);
                ok = memoria_escreve_bin(fd, &v, sizeof(v));
                break;
            }
            case 'c': {
                char v = (char)va_prox(args, int);
                ok = memoria_escreve_bin(fd, &v, sizeof(v));
                break;
            }
            case 'f': {
                float v = (float)va_prox(args, double);
                ok = memoria_escreve_bin(fd, &v, sizeof(v));
                break;
            }
            case 'd': {
                double v = va_prox(args, double);
                ok = memoria_escreve_bin(fd, &v, sizeof(v));
                break;
            }
            case 's': {
                const char *texto = va_prox(args, const char *);
                uint32_f tam = (uint32_f)txt_tam(texto);
                ok = memoria_escreve_bin(fd, &tam, sizeof(tam))
                && memoria_escreve_bin(fd, texto, tam);
                break;
            }
            case 'b': {
                const void *dados = va_prox(args, const void *);
                size_t tam = va_prox(args, size_t);
                uint32_f tam32 = (uint32_f)tam;
                ok = memoria_escreve_bin(fd, &tam32, sizeof(tam32))
                && memoria_escreve_bin(fd, dados, tam);
                break;
            }
            default:
                ok = 0;
                break;
        }
    }

    va_fim(args);
    ok = ok && memoria_flush_writer();
    memoria_writer_ativo = 0;
    close(fd);
    return ok ? 0 : -1;
}

int memoria_load(const char *caminho, const char *formato, ...) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(formato != NULL, CROCK_ERRO_NULO, -1);
    int fd = open(caminho, ARQ_O_RDONLY);
    if (fd < 0) return crock_falha(CROCK_ERRO_ARQUIVO);
    memoria_reader.fd = fd;
    memoria_reader.pos = 0;
    memoria_reader.tamanho = 0;
    memoria_reader_ativo = 1;

    char magia[4];
    if (!memoria_le_bin(fd, magia, sizeof(magia)) ||
        magia[0] != MEMORIA_SALVA_MAGIA[0] || magia[1] != MEMORIA_SALVA_MAGIA[1] ||
        magia[2] != MEMORIA_SALVA_MAGIA[2] || magia[3] != MEMORIA_SALVA_MAGIA[3]) {
        memoria_reader_ativo = 0;
        close(fd);
    return -1;
        }

        uint32_f tam_formato_salvo = 0;
        if (!memoria_le_bin(fd, &tam_formato_salvo, sizeof(tam_formato_salvo))) { memoria_reader_ativo = 0; close(fd); return -1; }

        char formato_salvo[64];
        if (tam_formato_salvo >= sizeof(formato_salvo)) { memoria_reader_ativo = 0; close(fd); return -1; }
        if (!memoria_le_bin(fd, formato_salvo, tam_formato_salvo)) { memoria_reader_ativo = 0; close(fd); return -1; }
        formato_salvo[tam_formato_salvo] = '\0';

        if (txt_comp(formato_salvo, formato) != 0) { memoria_reader_ativo = 0; close(fd); return crock_falha(CROCK_ERRO_FORMATO); }

            va_lista args;
            va_inicio(args, formato);
            int ok = 1;

            for (const char *f = formato; ok && *f != '\0'; f++) {
                switch (*f) {
                    case 'i': {
                        int *destino = va_prox(args, int *);
                        ok = memoria_le_bin(fd, destino, sizeof(*destino));
                        break;
                    }
                    case 'l': {
                        long *destino = va_prox(args, long *);
                        ok = memoria_le_bin(fd, destino, sizeof(*destino));
                        break;
                    }
                    case 'c': {
                        char *destino = va_prox(args, char *);
                        ok = memoria_le_bin(fd, destino, sizeof(*destino));
                        break;
                    }
                    case 'f': {
                        float *destino = va_prox(args, float *);
                        ok = memoria_le_bin(fd, destino, sizeof(*destino));
                        break;
                    }
                    case 'd': {
                        double *destino = va_prox(args, double *);
                        ok = memoria_le_bin(fd, destino, sizeof(*destino));
                        break;
                    }
                    case 's': {
                        char *destino = va_prox(args, char *);
                        size_t tam_buffer = va_prox(args, size_t);
                        uint32_f tam_salvo = 0;

                        if (tam_buffer == 0 || !memoria_le_bin(fd, &tam_salvo, sizeof(tam_salvo))) { ok = 0; break; }

                        size_t tam_copiar = ((size_t)tam_salvo < tam_buffer - 1) ? (size_t)tam_salvo : tam_buffer - 1;
                        if (!memoria_le_bin(fd, destino, tam_copiar)) { ok = 0; break; }
                        destino[tam_copiar] = '\0';

                        size_t sobra = (size_t)tam_salvo - tam_copiar;
                        char lixo[256];
                        while (sobra > 0) {
                            size_t pedaco = (sobra < sizeof(lixo)) ? sobra : sizeof(lixo);
                            if (!memoria_le_bin(fd, lixo, pedaco)) { ok = 0; break; }
                            sobra -= pedaco;
                        }
                        break;
                    }
                    case 'b': {
                        void *destino = va_prox(args, void *);
                        size_t tam_buffer = va_prox(args, size_t);
                        uint32_f tam_salvo = 0;

                        if (!memoria_le_bin(fd, &tam_salvo, sizeof(tam_salvo))) { ok = 0; break; }
                        if ((size_t)tam_salvo > tam_buffer) { ok = 0; break; }
                            ok = memoria_le_bin(fd, destino, (size_t)tam_salvo);
                            break;
                    }
                    default:
                        ok = 0;
                        break;
                }
            }

            va_fim(args);
            memoria_reader_ativo = 0;
            close(fd);
            return ok ? 0 : -1;
}

struct crock_timespec {
    int64_f tv_sec;
    int64_f tv_nsec;
};

#define CROCK_CLOCK_MONOTONO 1

extern int clock_gettime(int clk_id, struct crock_timespec *tp);
extern int nanosleep(const struct crock_timespec *pedido, struct crock_timespec *restante);

int64_f timer_relogio_ns(void) {
    struct crock_timespec ts;
    if (clock_gettime(CROCK_CLOCK_MONOTONO, &ts) != 0) return 0;
    return (int64_f)ts.tv_sec * 1000000000LL + (int64_f)ts.tv_nsec;
}

Timer timer_iniciar(void) {
    Timer t;
    t.inicio_ns = timer_relogio_ns();
    return t;
}

void timer_resetar(Timer *t) {
    if (!t) return;
    t->inicio_ns = timer_relogio_ns();
}

int64_f timer_ns(Timer *t) {
    if (!t) return 0;
    return timer_relogio_ns() - t->inicio_ns;
}

int64_f timer_us(Timer *t) {
    return timer_ns(t) / 1000LL;
}

int64_f timer_ms(Timer *t) {
    return timer_ns(t) / 1000000LL;
}

double timer_s(Timer *t) {
    return (double)timer_ns(t) / 1000000000.0;
}

void timer_dormir_ns(int64_f ns) {
    if (ns <= 0) return;
    struct crock_timespec pedido;
    pedido.tv_sec  = ns / 1000000000LL;
    pedido.tv_nsec = ns % 1000000000LL;
    struct crock_timespec restante;
    while (nanosleep(&pedido, &restante) != 0) {
        pedido = restante;
    }
}

void timer_dormir_ms(int64_f ms) {
    timer_dormir_ns(ms * 1000000LL);
}

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

void temporizador_iniciar(Temporizador *t, float tempo_max_s) {
    t->tempo_max_s = tempo_max_s;
    t->inicio_ns   = timer_relogio_ns();
}

void temporizador_resetar(Temporizador *t) {
    t->inicio_ns = timer_relogio_ns();
}

float temporizador_decorrido(const Temporizador *t) {
    int64_f agora = timer_relogio_ns();
    return (float)(agora - t->inicio_ns) / 1e9f;
}

bool temporizador_passou(Temporizador *t) {
    return temporizador_decorrido(t) >= t->tempo_max_s;
}

bool temporizador_nao_passou(Temporizador *t) {
    return !temporizador_passou(t);
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
