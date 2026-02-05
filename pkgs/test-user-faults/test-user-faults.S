.macro ufret
     .long 0xfa010ff3
.endm

.section .note.GNU-stack, "", @progbits

.text
.globl asm_ufault_handler
asm_ufault_handler:
    /* Save all caller-saved registers before calling into the C code. The C
       code is compiled to only use general purpose registers (e.g. SSE
       registers don't need to be saved). */
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    /* Before jumping to the handler, the CPU aligned the stack to 16-bytes and
       then pushed 5 quadwords. We just pushed 9 quadwords so at this point the
       stack is 16-bytes aligned again. This ABI requires the stack to be
       16-bytes aligned before calling a function. */

    /* The ABI requires the direction flag to be cleared before calling a
       function. */
    cld

    leaq 88(%rsp), %rdi
    movq 80(%rsp), %rsi
    movq 72(%rsp), %rdx
    call ufault_handler

    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    /* The UFRET instruction only pops the user interrupt frame. Before it, we
       need to pop the fault code and the fault address. */
    addq $16, %rsp

    ufret
