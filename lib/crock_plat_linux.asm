bits 64
default rel

section .text

global crock_plat_escrever
crock_plat_escrever:
    mov rax, 1
    syscall
    ret

global crock_plat_ler
crock_plat_ler:
    xor rax, rax
    syscall
    ret

global crock_plat_abrir_leitura
crock_plat_abrir_leitura:
    xor rsi, rsi
    xor rdx, rdx
    mov rax, 2
    syscall
    ret

global crock_plat_abrir_escrita
crock_plat_abrir_escrita:
    mov rsi, 0x241
    mov rdx, 0644o
    mov rax, 2
    syscall
    ret

global crock_plat_fechar
crock_plat_fechar:
    mov rax, 3
    syscall
    ret

global crock_plat_memoria_reservar
crock_plat_memoria_reservar:
    mov rsi, rdi
    xor rdi, rdi
    mov rdx, 3
    mov r10, 0x22
    mov r8, -1
    xor r9, r9
    mov rax, 9
    syscall
    cmp rax, -4095
    jae .mmap_erro
    ret
.mmap_erro:
    xor rax, rax
    ret

global crock_plat_memoria_decommit
crock_plat_memoria_decommit:
    mov rdx, 4
    mov rax, 28
    syscall
    ret

global crock_plat_memoria_liberar
crock_plat_memoria_liberar:
    mov rax, 11
    syscall
    ret

global crock_plat_relogio_ns
crock_plat_relogio_ns:
    sub rsp, 24
    mov rdi, 1
    mov rsi, rsp
    mov rax, 228
    syscall
    test rax, rax
    jnz .clock_erro
    mov rax, [rsp]
    imul rax, rax, 1000000000
    add rax, [rsp+8]
    add rsp, 24
    ret
.clock_erro:
    xor rax, rax
    add rsp, 24
    ret

global crock_plat_dormir_ns
crock_plat_dormir_ns:
    test rdi, rdi
    jle .sleep_fim
    sub rsp, 40
    mov rax, rdi
    mov rcx, 1000000000
    xor rdx, rdx
    div rcx
    mov [rsp], rax
    mov [rsp+8], rdx
.sleep_loop:
    mov rdi, rsp
    lea rsi, [rsp+16]
    mov rax, 35
    syscall
    test rax, rax
    jz .sleep_ok
    mov rax, [rsp+16]
    mov [rsp], rax
    mov rax, [rsp+24]
    mov [rsp+8], rax
    jmp .sleep_loop
.sleep_ok:
    add rsp, 40
.sleep_fim:
    ret
;Parabéns, pode ter lido isso aqui kkk
section .note.GNU-stack noalloc noexec nowrite progbits
