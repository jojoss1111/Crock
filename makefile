CC      := gcc
SAIDA_BUF_TAM ?= 128
CFLAGS  := -Wall -Wextra -std=c11 -O2 -ffreestanding -ffunction-sections -fdata-sections -DSAIDA_BUF_TAM=$(SAIDA_BUF_TAM)
NASM    := nasm
NASMFMT := elf64
AR      := ar
WINCC   ?= x86_64-w64-mingw32-gcc

LIBDIR  := lib
OUTDIR  := compilados
LIBNAME := libcrock.a
SONAME  := libcrock.so
DLLNAME := libcrock.dll

PREFIX     := /usr/local
INCLUDEDIR := $(PREFIX)/include
LIBOUTDIR  := $(PREFIX)/lib

CROCK_MODULOS := crock_erro crock_memoria crock_vetor crock_fmt \
                  crock_saida crock_saida_fmt crock_txt crock_entrada crock_arquivo \
                  crock_timer crock_random

ifndef PLAT
ifeq ($(OS),Windows_NT)
PLAT := windows
else
PLAT := linux
endif
endif

.PHONY: all install uninstall clean so

ifeq ($(PLAT),linux)

PLAT_OBJ      := $(OUTDIR)/crock_plat_linux.o
CORE_OBJS     := $(addprefix $(OUTDIR)/,$(addsuffix .o,$(CROCK_MODULOS)))
CORE_OBJS_PIC := $(addprefix $(OUTDIR)/,$(addsuffix _pic.o,$(CROCK_MODULOS)))
CROCK_HDRS    := $(LIBDIR)/crock.h $(LIBDIR)/crock_interno.h

all: $(LIBNAME)

$(LIBNAME): $(CORE_OBJS) $(PLAT_OBJ)
	$(AR) rcs $@ $^

$(OUTDIR)/%.o: $(LIBDIR)/%.c $(CROCK_HDRS) | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_linux.asm | $(OUTDIR)
	$(NASM) -f $(NASMFMT) $< -o $@

so: $(CORE_OBJS_PIC) $(PLAT_OBJ)
	$(CC) -shared $^ -o $(SONAME)

$(OUTDIR)/%_pic.o: $(LIBDIR)/%.c $(CROCK_HDRS) | $(OUTDIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(OUTDIR):
	mkdir -p $(OUTDIR)

install: $(LIBNAME)
	install -d $(INCLUDEDIR) $(LIBOUTDIR)
	install -m 644 $(LIBDIR)/crock.h $(INCLUDEDIR)/crock.h
	install -m 644 $(LIBNAME) $(LIBOUTDIR)/$(LIBNAME)
	@echo "Instalado. Use: gcc -Os arquivo.c -lcrock -Wl,--gc-sections -s -o programa"
	@echo "Buffer de saída padrão da lib: SAIDA_BUF_TAM=$(SAIDA_BUF_TAM). Para zero buffer: make SAIDA_BUF_TAM=0. Para performance: make SAIDA_BUF_TAM=16384"

clean:
	rm -f $(LIBNAME) $(SONAME)
	rm -rf $(OUTDIR)
	@echo "Artefatos locais removidos."

uninstall:
	rm -f $(INCLUDEDIR)/crock.h $(LIBOUTDIR)/$(LIBNAME) $(LIBOUTDIR)/$(SONAME)
	@echo "Lib removida do sistema."

else ifeq ($(PLAT),windows)

CC       := $(WINCC)
WINFLAGS := -Wall -Wextra -std=c11 -O2 -ffreestanding -ffunction-sections -fdata-sections -DSAIDA_BUF_TAM=$(SAIDA_BUF_TAM)

PLAT_OBJ   := $(OUTDIR)/crock_plat_win32.o
CORE_OBJS  := $(addprefix $(OUTDIR)/,$(addsuffix .o,$(CROCK_MODULOS)))
CROCK_HDRS := $(LIBDIR)/crock.h $(LIBDIR)/crock_interno.h
CROCK_SRCS := $(addprefix $(LIBDIR)/,$(addsuffix .c,$(CROCK_MODULOS)))

all: $(LIBNAME)

$(LIBNAME): $(CORE_OBJS) $(PLAT_OBJ)
	$(AR) rcs $@ $^

$(OUTDIR)/%.o: $(LIBDIR)/%.c $(CROCK_HDRS) | $(OUTDIR)
	$(CC) $(WINFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_win32.c $(LIBDIR)/crock.h | $(OUTDIR)
	$(CC) $(WINFLAGS) -c $< -o $@

$(OUTDIR):
	mkdir -p $(OUTDIR)

so: $(CROCK_SRCS) $(LIBDIR)/crock_plat_win32.c
	$(CC) $(WINFLAGS) -shared $(CROCK_SRCS) $(LIBDIR)/crock_plat_win32.c \
	      -lkernel32 -o $(DLLNAME)

install: $(LIBNAME)
	@echo "Windows: copie libcrock.a e crock.h manualmente pro seu projeto."

clean:
	rm -f $(LIBNAME) $(DLLNAME)
	rm -rf $(OUTDIR)
	@echo "Artefatos locais removidos."

uninstall:
	@echo "Windows: remova $(LIBNAME) e crock.h manualmente do projeto."

endif