#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "crock.h"

typedef __INT64_TYPE__ int64_f;

#define CROCK_WIN_ARQUIVOS_MAX 64

static HANDLE crock_win_arquivos[CROCK_WIN_ARQUIVOS_MAX];

static HANDLE crock_win_handle_de_fd(int fd) {
    if (fd == 0) return GetStdHandle(STD_INPUT_HANDLE);
    if (fd == 1) return GetStdHandle(STD_OUTPUT_HANDLE);
    if (fd == 2) return GetStdHandle(STD_ERROR_HANDLE);
    if (fd >= 3 && fd < CROCK_WIN_ARQUIVOS_MAX) return crock_win_arquivos[fd];
    return INVALID_HANDLE_VALUE;
}

static int crock_win_fd_de_handle(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE || h == NULL) return -1;
    for (int i = 3; i < CROCK_WIN_ARQUIVOS_MAX; i++) {
        if (crock_win_arquivos[i] == NULL) {
            crock_win_arquivos[i] = h;
            return i;
        }
    }
    CloseHandle(h);
    return -1;
}

void *crock_plat_memoria_reservar(size_t tam) {
    return VirtualAlloc(NULL, tam, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void crock_plat_memoria_liberar(void *mem, size_t tam) {
    (void)tam;
    VirtualFree(mem, 0, MEM_RELEASE);
}

void crock_plat_memoria_decommit(void *mem, size_t tam) {
    VirtualAlloc(mem, tam, MEM_RESET, PAGE_READWRITE);
}

int64_f crock_plat_escrever(int fd, const void *buf, size_t tam) {
    DWORD escritos = 0;
    HANDLE h = crock_win_handle_de_fd(fd);
    DWORD parte = tam > (size_t)0xFFFFFFFFu ? 0xFFFFFFFFu : (DWORD)tam;
    if (!WriteFile(h, buf, parte, &escritos, NULL)) return -1;
    return (int64_f)escritos;
}

int64_f crock_plat_ler(int fd, void *buf, size_t tam) {
    DWORD lidos = 0;
    HANDLE h = crock_win_handle_de_fd(fd);
    DWORD parte = tam > (size_t)0xFFFFFFFFu ? 0xFFFFFFFFu : (DWORD)tam;
    if (!ReadFile(h, buf, parte, &lidos, NULL)) return -1;
    return (int64_f)lidos;
}

int crock_plat_abrir_leitura(const char *caminho) {
    HANDLE h = CreateFileA(caminho, GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return crock_win_fd_de_handle(h);
}

int crock_plat_abrir_escrita(const char *caminho) {
    HANDLE h = CreateFileA(caminho, GENERIC_WRITE, 0,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return crock_win_fd_de_handle(h);
}

void crock_plat_fechar(int fd) {
    if (fd >= 3 && fd < CROCK_WIN_ARQUIVOS_MAX && crock_win_arquivos[fd] != NULL) {
        CloseHandle(crock_win_arquivos[fd]);
        crock_win_arquivos[fd] = NULL;
    }
}

int64_f crock_plat_relogio_ns(void) {
    static LARGE_INTEGER freq = {0};
    static int freq_inicializada = 0;
    LARGE_INTEGER cnt;
    if (!freq_inicializada) {
        if (!QueryPerformanceFrequency(&freq)) return 0;
        freq_inicializada = 1;
    }
    if (!QueryPerformanceCounter(&cnt)) return 0;
    return (int64_f)(
        (cnt.QuadPart / freq.QuadPart) * 1000000000LL
        + (cnt.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart
    );
}

void crock_plat_dormir_ns(int64_f ns) {
    if (ns <= 0) return;
    Sleep((DWORD)((ns + 999999LL) / 1000000LL));
}
