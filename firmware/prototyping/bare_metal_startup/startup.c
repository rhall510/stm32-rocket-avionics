#include "startup.h"

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

__attribute__((noreturn)) void _reset(void) {
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    main();

    while (1);
}

__attribute__((section(".vectors"))) void (*vtable[16 + 102])(void) = {
    (void (*)(void))(&_estack),
    _reset
};
