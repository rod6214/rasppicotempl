    #include "pico.h"
    #include "hardware/regs/m0plus.h"
    #include "hardware/regs/addressmap.h"

    .global isr_svcall
isr_svcall:
    b isr_svcall

    .global start_vectors
    .type start_vectors,%function
    .thumb
start_vectors:
    nop
    b start_vectors


    