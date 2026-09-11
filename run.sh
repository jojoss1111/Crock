#!/usr/bin/env bash
set -euo pipefail

CC=gcc
CXX=g++
NASM=nasm
NASMFMT=elf64
AR=ar

SAIDA_BUF_TAM=${SAIDA_BUF_TAM:-128}

LIBDIR=lib
OUTDIR=compilados
LIBNAME=libcrock.a

PREFIX=/usr/local
INCLUDEDIR="$PREFIX/include"
LIBOUTDIR="$PREFIX/lib"

CROCK_MODULOS=(crock_erro crock_vetor crock_fmt crock_saida
               crock_saida_fmt crock_txt crock_entrada
               crock_timer crock_random)

CFLAGS=(-Wall -Wextra -std=c11 -O2 -ffreestanding -ffunction-sections \
        -fdata-sections -fPIC -DSAIDA_BUF_TAM="$SAIDA_BUF_TAM")

CXXFLAGS=(-Wall -Wextra -std=c++17 -O2 -ffreestanding -ffunction-sections \
          -fdata-sections -fPIC -fno-exceptions -fno-rtti \
          -DSAIDA_BUF_TAM="$SAIDA_BUF_TAM")

compilar_c() {
    mkdir -p "$OUTDIR"
    for modulo in "${CROCK_MODULOS[@]}"; do
        echo "$CC ${CFLAGS[*]} -c $LIBDIR/$modulo.c -o $OUTDIR/$modulo.o"
        "$CC" "${CFLAGS[@]}" -c "$LIBDIR/$modulo.c" -o "$OUTDIR/$modulo.o"
    done

    echo "$NASM -f $NASMFMT $LIBDIR/crock_plat_linux.asm -o $OUTDIR/crock_plat_linux.o"
    "$NASM" -f "$NASMFMT" "$LIBDIR/crock_plat_linux.asm" -o "$OUTDIR/crock_plat_linux.o"
}

compilar_cpp() {
    mkdir -p "$OUTDIR"
    local modulo
    for modulo in crock_memoria crock_arquivo; do
        echo "$CXX ${CXXFLAGS[*]} -c $LIBDIR/$modulo.cpp -o $OUTDIR/$modulo.o"
        "$CXX" "${CXXFLAGS[@]}" -c "$LIBDIR/$modulo.cpp" -o "$OUTDIR/$modulo.o"
    done
}

empacotar_lib() {
    echo "$AR rcs $LIBNAME $OUTDIR/*.o"
    "$AR" rcs "$LIBNAME" "$OUTDIR"/*.o
}

cmd_build() {
    compilar_c
    compilar_cpp
    empacotar_lib
    echo "libcrock.a pronto (nesta pasta)."
}

cmd_install() {
    cmd_build

    install -d "$INCLUDEDIR" "$LIBOUTDIR"
    install -m 644 "$LIBDIR/crock.h" "$INCLUDEDIR/crock.h"
    if [[ -f "$LIBDIR/crock.hpp" ]]; then
        install -m 644 "$LIBDIR/crock.hpp" "$INCLUDEDIR/crock.hpp"
    fi
    install -m 644 "$LIBNAME" "$LIBOUTDIR/$LIBNAME"

    echo "Instalado. Use: gcc -Os arquivo.c -lcrock -Wl,--gc-sections -s -o programa"
}

cmd_clean() {
    rm -rf "$OUTDIR" "$LIBNAME"
    rm -f "$INCLUDEDIR/crock.h" "$INCLUDEDIR/crock.hpp" "$LIBOUTDIR/$LIBNAME"
    echo "crock removida do sistema e build local limpo."
}

case "${1:-}" in
    build)   cmd_build ;;
    install) cmd_install ;;
    clean)   cmd_clean ;;
    *)
        echo "Uso: $0 {build|install|clean}" >&2
        exit 1
        ;;
esac