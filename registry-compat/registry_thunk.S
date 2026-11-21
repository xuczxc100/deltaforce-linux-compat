/* Windows x64 registry-name normalization thunk. SPDX-License-Identifier: MIT */
    .text
    .globl registry_thunk
    .globl registry_thunk_end
    .globl registry_import_operand
    .seh_proc registry_thunk
registry_thunk:
    subq $0x48, %rsp
    .seh_stackalloc 0x48
    .seh_endprologue
    testq %rdx, %rdx
    jz .Lcall
    movzwl (%rdx), %eax
    testb $1, %al
    jnz .Lcall
    cmp $0x7ffe, %eax
    ja .Lcall
    movq (%rdx), %r10
    movq %r10, 0x30(%rsp)
    movq 8(%rdx), %r10
    movq %r10, 0x38(%rsp)
.Ltrim:
    testl %eax, %eax
    jz .Lname
    cmpw $0, -2(%r10,%rax)
    jne .Lname
    subl $2, %eax
    jmp .Ltrim
.Lname:
    movw %ax, 0x30(%rsp)
    leaq 0x30(%rsp), %rdx
.Lcall:
    movq 0x70(%rsp), %rax
    movq %rax, 0x20(%rsp)
    movq 0x78(%rsp), %rax
    movq %rax, 0x28(%rsp)
    .byte 0xff, 0x15
registry_import_operand:
    .long 0
    addq $0x48, %rsp
    ret
registry_thunk_end:
    .seh_endproc
