CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -ffreestanding
NASM    := nasm
NASMFMT := elf64
AR      := ar
WINCC   ?= x86_64-w64-mingw32-gcc

LIBDIR  := lib
LIBNAME := libcrock.a
SONAME  := libcrock.so
DLLNAME := libcrock.dll

PREFIX     := /usr/local
INCLUDEDIR := $(PREFIX)/include
LIBOUTDIR  := $(PREFIX)/lib

# Detect the host OS automatically, but allow cross-compilation with PLAT=.
ifndef PLAT
ifeq ($(OS),Windows_NT)
PLAT := windows
else
PLAT := linux
endif
endif

.PHONY: all install uninstall clean so

# ─── Linux ────────────────────────────────────────────────────────────────────
ifeq ($(PLAT),linux)

PLAT_OBJ := crock_plat_linux.o

all: $(LIBNAME)

$(LIBNAME): crock_core_linux.o $(PLAT_OBJ)
	$(AR) rcs $@ $^

crock_core_linux.o: $(LIBDIR)/crock.c $(LIBDIR)/crock.h
	$(CC) $(CFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_linux.asm
	$(NASM) -f $(NASMFMT) $< -o $@

so: crock_core_linux_pic.o $(PLAT_OBJ)
	$(CC) -shared $^ -o $(SONAME)

crock_core_linux_pic.o: $(LIBDIR)/crock.c $(LIBDIR)/crock.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

install: $(LIBNAME)
	install -d $(INCLUDEDIR) $(LIBOUTDIR)
	install -m 644 $(LIBDIR)/crock.h $(INCLUDEDIR)/crock.h
	install -m 644 $(LIBNAME) $(LIBOUTDIR)/$(LIBNAME)
	@echo "Instalado. Use: gcc arquivo.c -lcrock -o programa"

clean:
	rm -f $(LIBNAME) $(SONAME) crock_core_linux.o crock_core_linux_pic.o crock_core_win.o $(PLAT_OBJ) crock_plat_win32.o
	@echo "Artefatos locais removidos."

uninstall:
	rm -f $(INCLUDEDIR)/crock.h $(LIBOUTDIR)/$(LIBNAME) $(LIBOUTDIR)/$(SONAME)
	@echo "Lib removida do sistema."

# ─── Windows (MinGW cross ou nativo) ──────────────────────────────────────────
else ifeq ($(PLAT),windows)

CC     := $(WINCC)
WINFLAGS := -Wall -Wextra -std=c11 -O2 -ffreestanding

all: $(LIBNAME)

PLAT_OBJ := crock_plat_win32.o

$(LIBNAME): crock_core_win.o $(PLAT_OBJ)
	$(AR) rcs $@ $^

crock_core_win.o: $(LIBDIR)/crock.c $(LIBDIR)/crock.h
	$(CC) $(WINFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_win32.c $(LIBDIR)/crock.h
	$(CC) $(WINFLAGS) -c $< -o $@

so: $(LIBDIR)/crock.c $(LIBDIR)/crock_plat_win32.c
	$(CC) $(WINFLAGS) -shared $(LIBDIR)/crock.c $(LIBDIR)/crock_plat_win32.c \
	      -lkernel32 -o $(DLLNAME)

install: $(LIBNAME)
	@echo "Windows: copie libcrock.a e crock.h manualmente pro seu projeto."

clean:
	rm -f $(LIBNAME) $(DLLNAME) crock_core_win.o $(PLAT_OBJ) crock_plat_linux.o
	@echo "Artefatos locais removidos."

uninstall:
	@echo "Windows: remova $(LIBNAME) e crock.h manualmente do projeto."

endif
