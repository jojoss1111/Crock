#ifndef CROCK_HPP
#define CROCK_HPP

#include "crock.h"
#include "crock_interno.h"

namespace crock {

class LockGuard {
public:
    LockGuard() noexcept { crock_lock(); }

    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;
    LockGuard(LockGuard &&) = delete;
    LockGuard &operator=(LockGuard &&) = delete;

    ~LockGuard() { crock_unlock(); }
};

template <typename T>
class Alocacao {
public:
    Alocacao() noexcept : ptr_(nullptr) {}
    explicit Alocacao(T *ptr) noexcept : ptr_(ptr) {}
    Alocacao(const Alocacao &) = delete;
    Alocacao &operator=(const Alocacao &) = delete;

    Alocacao(Alocacao &&outro) noexcept : ptr_(outro.ptr_) {
        outro.ptr_ = nullptr;
    }

    Alocacao &operator=(Alocacao &&outro) noexcept {
        if (this != &outro) {
            liberar();
            ptr_ = outro.ptr_;
            outro.ptr_ = nullptr;
        }
        return *this;
    }

    ~Alocacao() { liberar(); }

    T *get() const noexcept { return ptr_; }
    T *operator->() const noexcept { return ptr_; }
    T &operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T *liberar_posse() noexcept {
        T *p = ptr_;
        ptr_ = nullptr;
        return p;
    }

private:
    void liberar() noexcept {
        if (ptr_ != nullptr) {
            memoria_free(ptr_);
            ptr_ = nullptr;
        }
    }

    T *ptr_;
};

template <typename T>
inline Alocacao<T> alocar(size_t qtd = 1) {
    return Alocacao<T>(static_cast<T *>(memoria_malloc(sizeof(T) * qtd)));
}

template <typename T>
inline Alocacao<T> alocar_zerado(size_t qtd = 1) {
    return Alocacao<T>(static_cast<T *>(memoria_calloc(qtd, sizeof(T))));
}

class VetorRAII {
public:
    explicit VetorRAII(size_t tam_elemento, int32_f capacidade_inicial = 0) noexcept
        : v_(vetor_criar(tam_elemento, capacidade_inicial)) {}

    VetorRAII(const VetorRAII &) = delete;
    VetorRAII &operator=(const VetorRAII &) = delete;

    VetorRAII(VetorRAII &&outro) noexcept : v_(outro.v_) {
        outro.v_.dados = nullptr;
        outro.v_.tamanho = 0;
        outro.v_.capacidade = 0;
    }

    VetorRAII &operator=(VetorRAII &&outro) noexcept {
        if (this != &outro) {
            vetor_liberar(&v_);
            v_ = outro.v_;
            outro.v_.dados = nullptr;
            outro.v_.tamanho = 0;
            outro.v_.capacidade = 0;
        }
        return *this;
    }

    ~VetorRAII() { vetor_liberar(&v_); }

    Vetor *bruto() noexcept { return &v_; }
    const Vetor *bruto() const noexcept { return &v_; }

    int   add(void *valor) noexcept { return vetor_add(&v_, valor); }
    int   add_str(const char *texto) noexcept { return vetor_add_str(&v_, texto); }
    int   inserir(int32_f indice, void *valor) noexcept { return vetor_inserir(&v_, indice, valor); }
    void *get(int32_f indice) noexcept { return vetor_get(&v_, indice); }
    int   set(int32_f indice, void *valor) noexcept { return vetor_set(&v_, indice, valor); }
    int   remover(int32_f indice) noexcept { return vetor_remover(&v_, indice); }
    int32_f tam() const noexcept { return v_.tamanho; }
    void  limpar() noexcept { vetor_limpar(&v_); }
    int   definir_limite(int32_f limite) noexcept { return vetor_definir_limite(&v_, limite); }

private:
    Vetor v_;
};

class EscopoTimer {
public:
    explicit EscopoTimer(const char *rotulo, int fd = SAIDA_STDERR) noexcept
        : rotulo_(rotulo), fd_(fd), t_(timer_iniciar()) {}

    EscopoTimer(const EscopoTimer &) = delete;
    EscopoTimer &operator=(const EscopoTimer &) = delete;

    ~EscopoTimer() {
        int64_f us = timer_us(&t_);
        saida_fmt_fd(fd_, "[timer] %s: ", rotulo_);
        saida_uint_fd(fd_, (uint64_f)us, 10, 0);
        saida_txt_fd(fd_, " us\n");
    }

private:
    const char *rotulo_;
    int fd_;
    Timer t_;
};

}

#endif // CROCK_HPP