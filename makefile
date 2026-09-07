CC      := gcc
SAIDA_BUF_TAM ?= 1024
CFLAGS  := -Wall -Wextra -std=c11 -O2 -ffreestanding -ffunction-sections -fdata-sections -DSAIDA_BUF_TAM=$(SAIDA_BUF_TAM)
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

PLAT_OBJ     := crock_plat_linux.o
CORE_OBJS    := $(addsuffix .o,$(CROCK_MODULOS))
CORE_OBJS_PIC := $(addsuffix _pic.o,$(CROCK_MODULOS))
CROCK_HDRS   := $(LIBDIR)/crock.h $(LIBDIR)/crock_interno.h

all: $(LIBNAME)

$(LIBNAME): $(CORE_OBJS) $(PLAT_OBJ)
	$(AR) rcs $@ $^

%.o: $(LIBDIR)/%.c $(CROCK_HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_linux.asm
	$(NASM) -f $(NASMFMT) $< -o $@

so: $(CORE_OBJS_PIC) $(PLAT_OBJ)
	$(CC) -shared $^ -o $(SONAME)

%_pic.o: $(LIBDIR)/%.c $(CROCK_HDRS)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

install: $(LIBNAME)
	install -d $(INCLUDEDIR) $(LIBOUTDIR)
	install -m 644 $(LIBDIR)/crock.h $(INCLUDEDIR)/crock.h
	install -m 644 $(LIBNAME) $(LIBOUTDIR)/$(LIBNAME)
	@echo "Instalado. Use: gcc -Os arquivo.c -lcrock -Wl,--gc-sections -s -o programa"
	@echo "Buffer de saída padrão da lib: SAIDA_BUF_TAM=$(SAIDA_BUF_TAM). Para performance: make SAIDA_BUF_TAM=16384"

clean:
	rm -f $(LIBNAME) $(SONAME) $(CORE_OBJS) $(CORE_OBJS_PIC) $(PLAT_OBJ) crock_plat_win32.o
	@echo "Artefatos locais removidos."

uninstall:
	rm -f $(INCLUDEDIR)/crock.h $(LIBOUTDIR)/$(LIBNAME) $(LIBOUTDIR)/$(SONAME)
	@echo "Lib removida do sistema."

else ifeq ($(PLAT),windows)

CC       := $(WINCC)
WINFLAGS := -Wall -Wextra -std=c11 -O2 -ffreestanding -ffunction-sections -fdata-sections -DSAIDA_BUF_TAM=$(SAIDA_BUF_TAM)

PLAT_OBJ   := crock_plat_win32.o
CORE_OBJS  := $(addsuffix .o,$(CROCK_MODULOS))
CROCK_HDRS := $(LIBDIR)/crock.h $(LIBDIR)/crock_interno.h
CROCK_SRCS := $(addprefix $(LIBDIR)/,$(addsuffix .c,$(CROCK_MODULOS)))

all: $(LIBNAME)

$(LIBNAME): $(CORE_OBJS) $(PLAT_OBJ)
	$(AR) rcs $@ $^

%.o: $(LIBDIR)/%.c $(CROCK_HDRS)
	$(CC) $(WINFLAGS) -c $< -o $@

$(PLAT_OBJ): $(LIBDIR)/crock_plat_win32.c $(LIBDIR)/crock.h
	$(CC) $(WINFLAGS) -c $< -o $@

so: $(CROCK_SRCS) $(LIBDIR)/crock_plat_win32.c
	$(CC) $(WINFLAGS) -shared $(CROCK_SRCS) $(LIBDIR)/crock_plat_win32.c \
	      -lkernel32 -o $(DLLNAME)

install: $(LIBNAME)
	@echo "Windows: copie libcrock.a e crock.h manualmente pro seu projeto."

clean:
	rm -f $(LIBNAME) $(DLLNAME) $(CORE_OBJS) $(PLAT_OBJ) crock_plat_linux.o
	@echo "Artefatos locais removidos."

uninstall:
	@echo "Windows: remova $(LIBNAME) e crock.h manualmente do projeto."

endif
