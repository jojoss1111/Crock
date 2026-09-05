CC     := gcc
CFLAGS := -Wall -Wextra -std=c11

LIBDIR := lib
LIBNAME := libcrock.a

PREFIX     := /usr/local
INCLUDEDIR := $(PREFIX)/include
LIBOUTDIR  := $(PREFIX)/lib

SONAME := libcrock.so

.PHONY: install clean so lua-install lua-clean

install: $(LIBDIR)/crock.c $(LIBDIR)/crock.h
	$(CC) $(CFLAGS) -c $(LIBDIR)/crock.c -o stdcrock.o
	ar rcs $(LIBNAME) stdcrock.o
	rm -f stdcrock.o
	install -d $(INCLUDEDIR) $(LIBOUTDIR)
	install -m 644 $(LIBDIR)/crock.h $(INCLUDEDIR)/crock.h
	install -m 644 $(LIBNAME) $(LIBOUTDIR)/$(LIBNAME)
	rm -f $(LIBNAME)
	@echo "Instalado. Agora e' so: gcc arquivo.c -lcrock -o programa"


clean:
	rm -f $(INCLUDEDIR)/crock.h $(LIBOUTDIR)/$(LIBNAME) stdcrock.o $(LIBNAME) $(SONAME)
	@echo "Lib removida do sistema."