#ifndef LIB_H
#define LIB_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef __INT8_TYPE__    int8_f;
typedef __INT16_TYPE__   int16_f;
typedef __INT32_TYPE__   int32_f;
typedef __INT64_TYPE__   int64_f;

typedef __UINT8_TYPE__   uint8_f;
typedef __UINT16_TYPE__  uint16_f;
typedef __UINT32_TYPE__  uint32_f;
typedef __UINT64_TYPE__  uint64_f;

typedef unsigned long size_t;

#define HEAP_SIZE 1048576

// códigos de erro da lib
typedef enum {
    CROCK_OK = 0,
    CROCK_ERRO_NULO,
    CROCK_ERRO_INDICE,
    CROCK_ERRO_MEMORIA,
    CROCK_ERRO_TAM_INVALIDO,
    CROCK_ERRO_LIMITE,
    CROCK_ERRO_USE_AFTER_FREE,
    CROCK_ERRO_ARQUIVO,
    CROCK_ERRO_FORMATO,
} CrockErro;

// retorna o último erro registrado
CrockErro crock_erro(void);
// retorna uma descrição em texto do erro
const char *crock_erro_texto(CrockErro erro);
// limpa o último erro registrado
void crock_erro_limpar(void);

// zera um bloco de memória
void *memoria_zerar(void *destino, size_t tam);
// copia um bloco de memória
void *memoria_copia(void *destino, const void *origem, size_t tam);

// inicializa o heap interno da lib
void  init_heap(void);
// aloca memória
void *memoria_malloc(size_t tam);
// aloca memória zerada
void *memoria_calloc(size_t qtd, size_t tam_item);
// libera memória alocada
void  memoria_free(void *ptr);
// realoca um bloco de memória
void *memoria_realloc(void *ptr, size_t novo_tam);
#define memoria_tam(x) sizeof(x)

// vetor dinâmico genérico
typedef struct {
    void *dados;
    int32_f tamanho;
    int32_f capacidade;
    int32_f limite;
    size_t tam_elemento;
} Vetor;

// cria um vetor novo
Vetor vetor_criar(size_t tam_elemento, int32_f capacidade_inicial);
// define um limite máximo de elementos
int   vetor_definir_limite(Vetor *v, int32_f limite);
// retorna o limite máximo configurado
int32_f vetor_limite(Vetor *v);
// diz se o vetor atingiu o limite
int   vetor_cheio(Vetor *v);

// adiciona um elemento no final
int   vetor_add(Vetor *v, void *valor);
// adiciona uma string no final
int   vetor_add_str(Vetor *v, const char *texto);
// insere um elemento numa posição específica
int   vetor_inserir(Vetor *v, int32_f indice, void *valor);
// retorna o ponteiro pro elemento num índice
void *vetor_get(Vetor *v, int32_f indice);
// altera o elemento num índice
int   vetor_set(Vetor *v, int32_f indice, void *valor);
// remove um elemento num índice
int   vetor_remover(Vetor *v, int32_f indice);
// retorna a quantidade de elementos
int32_f vetor_tam(Vetor *v);
// esvazia o vetor sem liberar a memória
void  vetor_limpar(Vetor *v);
// libera o vetor
void  vetor_liberar(Vetor *v);

extern long write(int fd, const void *buf, unsigned long contagem);

#define SAIDA_STDOUT 1
#define SAIDA_STDERR 2

// força a escrita do buffer de saída de um fd
void saida_flush(int fd);
// força a escrita de todos os buffers de saída
void saida_flush_tudo(void);
// escreve um caractere num fd
void saida_char_fd(int fd, char c);
// escreve uma string num fd
void saida_str_fd(int fd, const char *s);
// escreve um inteiro sem sinal num fd, numa base qualquer
void saida_uint_fd(int fd, uint64_f valor, int base, int maiusculo);
// escreve um inteiro com sinal num fd
void saida_int_fd(int fd, int64_f valor);
// escreve um float num fd, com um número fixo de casas
void saida_float_fd(int fd, double valor, int casas);
// escreve um texto formatado num fd
void saida_fmt_fd(int fd, const char *formato, ...);

#define saida_txt(...)    do { saida_fmt_fd(SAIDA_STDOUT, __VA_ARGS__); saida_flush(SAIDA_STDOUT); } while (0)
#define saida_txt_ln(...) do { saida_fmt_fd(SAIDA_STDOUT, __VA_ARGS__); saida_char_fd(SAIDA_STDOUT, '\n'); } while (0)
#define saida_erro(...)   do { saida_fmt_fd(SAIDA_STDERR, __VA_ARGS__); saida_flush(SAIDA_STDERR); } while (0)

// formata um texto num buffer (tipo snprintf)
int    txt_fmt(char *dest, unsigned long tam, const char *formato, ...);
// copia uma string
char  *txt_copia(char *dest, const char *src, unsigned long tam);
// concatena uma string
char  *txt_junta(char *dest, const char *src, unsigned long tam);
// compara duas strings
int    txt_comp(const char *a, const char *b);
// retorna o tamanho de uma string
unsigned long txt_tam(const char *str);
// converte uma string pra inteiro
int64_f txt_p_int(const char *str);
// converte uma string pra float
double txt_p_flt(const char *str);
// limpa o buffer interno usado por txt_string
void   txt_limpar(void);
// formata e retorna uma string nova (usa buffer interno)
char  *txt_string(const char *formato, ...);

// lê um inteiro digitado pelo usuário
int    entrada_int(void);
// lê um float digitado pelo usuário
double entrada_float(void);
// lê uma string digitada pelo usuário (aloca)
char  *entrada_str(int cap);
// lê uma string digitada pelo usuário num buffer existente
void   entrada_str_em(char *destino, int tam);

#if !defined(__cplusplus) && !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
typedef char bool;
#define false 0
#define true  1
#endif

// string dinâmica
typedef struct {
    char *dados;
    size_t tamanho;
    size_t capacidade;
} string;

// cria uma string a partir de um texto
string string_criar(const char *inicial);
// cria uma string vazia com capacidade inicial
string string_criar_cap(size_t capacidade_inicial);
// cria uma string nova já formatada
string string_fmt_novo(const char *formato, ...);
// adiciona um texto no final
int    string_add(string *t, const char *s);
// adiciona um caractere no final
int    string_add_char(string *t, char c);
// adiciona um texto formatado no final
int    string_fmt(string *t, const char *formato, ...);
// esvazia a string sem liberar a memória
void   string_limpar(string *t);
// libera a string
void   string_liberar(string *t);
// retorna o conteúdo como const char*
const char *string_cstr(string *t);
// compara a string com um texto
int    string_igual(string *t, const char *s);
// retorna o tamanho da string
size_t string_tam(string *t);

// divide a string por um delimitador, retorna um vetor de strings
Vetor  string_split(string *t, const char *delimitador);
// substitui todas as ocorrências de um alvo por outro texto
int    string_replace(string *t, const char *alvo, const char *substituto);
// remove espaços do início e do fim
void   string_trim(string *t);

#define s(txt) string_criar(txt)
#define f(...) string_fmt_novo(__VA_ARGS__)
#define p(t)   string_cstr(&(t))

// lê o conteúdo inteiro de um arquivo
string arquivo_ler_tudo(const char *caminho);
// escreve um conteúdo inteiro num arquivo
int    arquivo_escrever_tudo(const char *caminho, const char *conteudo);
// diz se um arquivo existe
int    arquivo_existe(const char *caminho);
// salva valores num arquivo binário, formato tipo printf
int memoria_salva(const char *caminho, const char *formato, ...);
// carrega valores de um arquivo binário, formato tipo printf
int memoria_load(const char *caminho, const char *formato, ...);

// timer, medido a partir do relógio monotônico do sistema
typedef struct {
    int64_f inicio_ns;
} Timer;

// retorna o relógio monotônico atual, em nanossegundos
int64_f timer_relogio_ns(void);

// cria e inicia um timer
Timer   timer_iniciar(void);
// reinicia a contagem de um timer
void    timer_resetar(Timer *t);

// retorna o tempo decorrido em nanossegundos
int64_f timer_ns(Timer *t);
// retorna o tempo decorrido em microssegundos
int64_f timer_us(Timer *t);
// retorna o tempo decorrido em milissegundos
int64_f timer_ms(Timer *t);
// retorna o tempo decorrido em segundos
double  timer_s(Timer *t);

// pausa a execução por um tempo em nanossegundos
void    timer_dormir_ns(int64_f ns);
// pausa a execução por um tempo em milissegundos
void    timer_dormir_ms(int64_f ms);

// define a semente do gerador de números aleatórios
void     random_seed(uint64_f semente);
// gera um inteiro sem sinal aleatório
uint32_f random_uint(void);
// gera um inteiro aleatório entre min e max, inclusive
int32_f  random_int(int32_f min, int32_f max);
// gera um float aleatório entre min e max
float    random_float(float min, float max);
// gera um double aleatório entre min e max
double   random_double(double min, double max);
// gera um booleano aleatório
bool     random_bool(void);
// gera um caractere aleatório de um conjunto (NULL = ASCII imprimível)
char     random_char(const char *conjunto);

// gera um vetor de inteiros aleatórios
Vetor random_lista_int(int32_f n, int32_f min, int32_f max);
// gera um vetor de floats aleatórios
Vetor random_lista_float(int32_f n, float min, float max);
// gera um vetor de booleanos aleatórios
Vetor random_lista_bool(int32_f n);
// gera um vetor de caracteres aleatórios
Vetor random_lista_char(int32_f n, const char *conjunto);

#ifdef __cplusplus
}
#endif

#endif