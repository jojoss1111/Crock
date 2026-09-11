#include "crock_interno.h"

namespace {

class FdRAII {
public:
    explicit FdRAII(int fd) : fd_(fd) {}
    ~FdRAII() { if (fd_ >= 0) crock_plat_fechar(fd_); }

    FdRAII(const FdRAII &) = delete;
    FdRAII &operator=(const FdRAII &) = delete;

    int get() const { return fd_; }
    bool valido() const { return fd_ >= 0; }

private:
    int fd_;
};

class BufferRAII {
public:
    explicit BufferRAII(char *ptr) : ptr_(ptr) {}
    ~BufferRAII() { if (ptr_ != NULL) memoria_free(ptr_); }

    BufferRAII(const BufferRAII &) = delete;
    BufferRAII &operator=(const BufferRAII &) = delete;

    char *get() const { return ptr_; }
    void reset(char *novo) { ptr_ = novo; }
    char *solta() { char *p = ptr_; ptr_ = NULL; return p; }

private:
    char *ptr_;
};

} // namespace

extern "C" {

char *arquivo_ler_tudo_tam(const char *caminho, size_t *tam_out) {
    if (caminho == NULL) { crock_falha(CROCK_ERRO_NULO); return NULL; }

    size_t capacidade = 4096;
    size_t tamanho = 0;

    BufferRAII buffer((char *)memoria_malloc(capacidade));
    if (buffer.get() == NULL) return NULL;

    FdRAII arq(crock_plat_abrir_leitura(caminho));
    if (!arq.valido()) { crock_falha(CROCK_ERRO_ARQUIVO); return NULL; }

    char buf[65536];
    int64_f n;
    while ((n = crock_plat_ler(arq.get(), buf, sizeof(buf))) > 0) {
        size_t lido = (size_t)n;
        if (lido > (size_t)-1 - tamanho - 1) {
            crock_falha(CROCK_ERRO_TAM_INVALIDO);
            return NULL;
        }
        size_t necessario = tamanho + lido + 1;
        if (necessario > capacidade) {
            size_t nova = capacidade;
            while (nova < necessario) {
                if (nova > (size_t)-1 / 2) { nova = necessario; break; }
                nova *= 2;
            }
            char *novo = (char *)memoria_realloc(buffer.get(), nova);
            if (novo == NULL) return NULL;
            buffer.reset(novo);
            capacidade = nova;
        }
        memoria_copia(buffer.get() + tamanho, buf, lido);
        tamanho += lido;
    }

    if (n < 0) { crock_falha(CROCK_ERRO_ARQUIVO); return NULL; }

    buffer.get()[tamanho] = '\0';
    if (tam_out) *tam_out = tamanho;
    return buffer.solta();
}

char *arquivo_ler_tudo(const char *caminho) {
    return arquivo_ler_tudo_tam(caminho, NULL);
}

int arquivo_escrever_tudo(const char *caminho, const char *conteudo) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(conteudo != NULL, CROCK_ERRO_NULO, -1);

    FdRAII arq(crock_plat_abrir_escrita(caminho));
    if (!arq.valido()) return crock_falha(CROCK_ERRO_ARQUIVO);

    size_t tam = txt_tam(conteudo);
    size_t escrito = 0;
    while (escrito < tam) {
        int64_f n = crock_plat_escrever(arq.get(), conteudo + escrito, tam - escrito);
        if (n <= 0) return crock_falha(CROCK_ERRO_ARQUIVO);
        escrito += (size_t)n;
    }

    return 0;
}

int arquivo_existe(const char *caminho) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, 0);
    FdRAII arq(crock_plat_abrir_leitura(caminho));
    return arq.valido() ? 1 : 0;
}

} // extern "C"

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
        int64_f n = crock_plat_escrever(fd, p + escrito, tam - escrito);
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
            int64_f n = crock_plat_ler(fd, p + lido, tam - lido);
            if (n <= 0) return 0;
            lido += (size_t)n;
        }
        return 1;
    }
    char *p = (char *)dest;
    while (tam > 0) {
        if (memoria_reader.pos == memoria_reader.tamanho) {
            int64_f n = crock_plat_ler(fd, memoria_reader.dados, MEMORIA_IO_BUF_TAM);
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

extern "C" int memoria_salva(const char *caminho, const char *formato, ...) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(formato != NULL, CROCK_ERRO_NULO, -1);
    int fd = crock_plat_abrir_escrita(caminho);
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
    crock_plat_fechar(fd);
    return ok ? 0 : -1;
}

extern "C" int memoria_load(const char *caminho, const char *formato, ...) {
    CROCK_EXIGIR(caminho != NULL, CROCK_ERRO_NULO, -1);
    CROCK_EXIGIR(formato != NULL, CROCK_ERRO_NULO, -1);
    int fd = crock_plat_abrir_leitura(caminho);
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
        crock_plat_fechar(fd);
        return -1;
    }

    uint32_f tam_formato_salvo = 0;
    if (!memoria_le_bin(fd, &tam_formato_salvo, sizeof(tam_formato_salvo))) { memoria_reader_ativo = 0; crock_plat_fechar(fd); return -1; }

    char formato_salvo[64];
    if (tam_formato_salvo >= sizeof(formato_salvo)) { memoria_reader_ativo = 0; crock_plat_fechar(fd); return -1; }
    if (!memoria_le_bin(fd, formato_salvo, tam_formato_salvo)) { memoria_reader_ativo = 0; crock_plat_fechar(fd); return -1; }
    formato_salvo[tam_formato_salvo] = '\0';

    if (txt_comp(formato_salvo, formato) != 0) { memoria_reader_ativo = 0; crock_plat_fechar(fd); return crock_falha(CROCK_ERRO_FORMATO); }

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
    crock_plat_fechar(fd);
    return ok ? 0 : -1;
}
