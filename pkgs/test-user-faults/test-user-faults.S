.macro ufret
     .long 0xfa010ff3
.endm

.section .note.GNU-stack, "", @progbits

.text
.globl asm_ufault_handler
asm_ufault_handler:
    /* Clear the direction flag. This is required by the ABI. */
    cld

    /* To deliver a user fault, the CPU first aligns the stack to 16-bytes and
       then pushes 5 quadwords. Push an additional quadword before the next 8
       quadwords, so that the stack is 16-bytes aligned when calling into the C
       code. This is required by the ABI. */
    subq $8, %rsp

    /* Save all caller-saved registers before calling into the C code. The C
       code is compiled to only use general purpose registers (e.g. SSE
       registers don't need to be saved). */
    pushq %rax
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    leaq 88(%rsp), %rdi
    movq 80(%rsp), %rsi
    movq 72(%rsp), %rdx
    call ufault_handler

    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rax

    /* The UFRET instruction only pops the user interrupt frame. Before it, we
       need to pop the fault code and the fault address. */
    addq $24, %rsp

    ufret
