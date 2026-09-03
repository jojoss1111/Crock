# crock

Biblioteca utilitária em C, sem dependências de libc para a maior parte das operações (memória própria via `mmap`, saída/formatação próprias, etc). Nasceu como suporte pra outros projetos (como o [Bird Engine](#)) e virou uma libzinha de uso geral: memória, vetor dinâmico, strings, arquivos, saída formatada, timer e números aleatórios.

> A crock está em constante evolução. Se encontrar algum erro ou tiver ideia pra um novo recurso, manda no canal.

## Recursos

- **Memória** (`memoria_*`) — allocator próprio (`malloc`/`calloc`/`realloc`/`free`) sobre `mmap`, com detecção de use-after-free.
- **Vetor** (`vetor_*`) — vetor dinâmico genérico, com limite opcional de elementos.
- **Saída** (`saida_*`) — escrita formatada em stdout/stderr, sem passar por `printf`.
- **Texto** (`txt_*`) — formatação, cópia, comparação e parsing de strings em buffers fixos.
- **String** (`string_*`) — string dinâmica (crescimento automático), com split, replace e trim.
- **Entrada** (`entrada_*`) — leitura de int/float/string do stdin.
- **Arquivo** (`arquivo_*`, `memoria_salva`/`memoria_load`) — leitura/escrita de arquivos e serialização binária tipo `printf`.
- **Timer** (`timer_*`) — medição de tempo (relógio monotônico) e sleep, em ns/us/ms/s.
- **Random** (`random_*`) — geração de números aleatórios (int, float, double, bool, char) e listas prontas em `Vetor`.

Todo erro reportável passa por `crock_erro()` / `crock_erro_texto()`.

## Instalação (Linux)

Clone o repositório e, dentro da pasta:

```bash
sudo make install
```

Isso compila a lib e instala o header (`crock.h`) e a biblioteca estática/compartilhada nos includes/libs do seu sistema, prontos pra usar em qualquer projeto.

Para desinstalar / limpar o que foi instalado:

```bash
sudo make clean
```

## Usando em um projeto

Depois de instalada, basta incluir o header e linkar com `-lcrock` na hora de compilar:

```c
// main.c
#include <crock.h>

int main(void) {
    init_heap();

    Timer t = timer_iniciar();
    saida_txt_ln("aleatorio: %d", (int)random_int(1, 100));
    saida_txt_ln("levou %lld ns", (long long)timer_ns(&t));

    return 0;
}
```

Compilando:

```bash
gcc main.c -lcrock -o main
```

## Exemplos rápidos

**Vetor dinâmico:**

```c
Vetor v = vetor_criar(sizeof(int32_f), 4);
int32_f x = 10;
vetor_add(&v, &x);
saida_txt_ln("tamanho: %d", (int)vetor_tam(&v));
vetor_liberar(&v);
```

**String dinâmica:**

```c
string s = s("ola");
string_add(&s, " mundo");
saida_txt_ln("%s", p(s));
string_liberar(&s);
```

**Timer:**

```c
Timer t = timer_iniciar();
timer_dormir_ms(50);
saida_txt_ln("passou %lld ms", (long long)timer_ms(&t));
```

**Random:**

```c
random_seed((uint64_f)timer_relogio_ns()); // opcional, sem chamar já auto-semeia
int32_f n = random_int(10, 30);
Vetor lista = random_lista_int(5, 0, 9);
vetor_liberar(&lista);
```

## Tratamento de erro

Funções que podem falhar setam um código em `crock_erro()`:

```c
void *p = memoria_malloc(0);
if (p == NULL) {
    saida_erro("erro: %s\n", crock_erro_texto(crock_erro()));
}
```

## Contribuindo

Achou um bug ou tem ideia pra um recurso novo? Manda no canal — a crock só cresce com feedback de quem usa.
