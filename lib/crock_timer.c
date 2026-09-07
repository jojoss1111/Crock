#include "crock_interno.h"

int64_f timer_relogio_ns(void) {
    return crock_plat_relogio_ns();
}

Timer timer_iniciar(void) {
    Timer t;
    t.inicio_ns = timer_relogio_ns();
    return t;
}

void timer_resetar(Timer *t) {
    if (!t) return;
    t->inicio_ns = timer_relogio_ns();
}

int64_f timer_ns(Timer *t) {
    if (!t) return 0;
    return timer_relogio_ns() - t->inicio_ns;
}

int64_f timer_us(Timer *t) {
    return timer_ns(t) / 1000LL;
}

int64_f timer_ms(Timer *t) {
    return timer_ns(t) / 1000000LL;
}

double timer_s(Timer *t) {
    return (double)timer_ns(t) / 1000000000.0;
}

void timer_dormir_ns(int64_f ns) {
    crock_plat_dormir_ns(ns);
}

void timer_dormir_ms(int64_f ms) {
    timer_dormir_ns(ms * 1000000LL);
}
