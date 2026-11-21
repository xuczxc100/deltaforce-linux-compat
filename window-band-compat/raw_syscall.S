.text
.globl raw_band_syscall
.def raw_band_syscall; .scl 2; .type 32; .endef
.seh_proc raw_band_syscall
raw_band_syscall:
    .seh_endprologue
    movq %rcx, %r10
    movl %r8d, %eax
    syscall
    ret
.seh_endproc
