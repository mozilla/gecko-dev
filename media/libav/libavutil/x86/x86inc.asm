;*****************************************************************************
;* x86inc.asm: x264asm abstraction layer
;*****************************************************************************
;* Copyright (C) 2005-2013 x264 project
;*
;* Authors: Loren Merritt <lorenm@u.washington.edu>
;*          Anton Mitrofanov <BugMaster@narod.ru>
;*          Fiona Glaser <fiona@x264.com>
;*          Henrik Gramner <henrik@gramner.com>
;*
;* Permission to use, copy, modify, and/or distribute this software for any
;* purpose with or without fee is hereby granted, provided that the above
;* copyright notice and this permission notice appear in all copies.
;*
;* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
;* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
;* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
;* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
;* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
;* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
;* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
;*****************************************************************************

; This is a header file for the x264ASM assembly language, which uses
; NASM/YASM syntax combined with a large number of macros to provide easy
; abstraction between different calling conventions (x86_32, win64, linux64).
; It also has various other useful features to simplify writing the kind of
; DSP functions that are most often used in x264.

; Unlike the rest of x264, this file is available under an ISC license, as it
; has significant usefulness outside of x264 and we want it to be available
; to the largest audience possible.  Of course, if you modify it for your own
; purposes to add a new feature, we strongly encourage contributing a patch
; as this feature might be useful for others as well.  Send patches or ideas
; to x264-devel@videolan.org .

%ifndef private_prefix
    %define private_prefix x264
%endif

%ifndef public_prefix
    %define public_prefix private_prefix
%endif

%define WIN64  0
%define UNIX64 0
%if ARCH_X86_64
    %ifidn __OUTPUT_FORMAT__,win32
        %define WIN64  1
    %elifidn __OUTPUT_FORMAT__,win64
        %define WIN64  1
    %elifidn __OUTPUT_FORMAT__,x64
        %define WIN64  1
    %else
        %define UNIX64 1
    %endif
%endif

%ifdef PREFIX
    %define mangle(x) _ %+ x
%else
    %define mangle(x) x
%endif

; aout does not support align=
; NOTE: This section is out of sync with x264, in order to
; keep supporting OS/2.
%macro SECTION_RODATA 0-1 16
    %ifidn __OUTPUT_FORMAT__,aout
        section .text
    %else
        SECTION .rodata align=%1
    %endif
%endmacro

%macro SECTION_TEXT 0-1 16
    %ifidn __OUTPUT_FORMAT__,aout
        SECTION .text
    %else
        SECTION .text align=%1
    %endif
%endmacro

%if WIN64
    %define PIC
%elif ARCH_X86_64 == 0
; x86_32 doesn't require PIC.
; Some distros prefer shared objects to be PIC, but nothing breaks if
; the code contains a few textrels, so we'll skip that complexity.
    %undef PIC
%endif
%ifdef PIC
    default rel
%endif

%macro CPUNOP 1
    %if HAVE_CPUNOP
        CPU %1
    %endif
%endmacro

; Always use long nops (reduces 0x90 spam in disassembly on x86_32)
CPUNOP amdnop

; Macros to eliminate most code duplication between x86_32 and x86_64:
; Currently this works only for leaf functions which load all their arguments
; into registers at the start, and make no other use of the stack. Luckily that
; covers most of x264's asm.

; PROLOGUE:
; %1 = number of arguments. loads them from stack if needed.
; %2 = number of registers used. pushes callee-saved regs if needed.
; %3 = number of xmm registers used. pushes callee-saved xmm regs if needed.
; %4 = (optional) stack size to be allocated. If not aligned (x86-32 ICC 10.x,
;      MSVC or YMM), the stack will be manually aligned (to 16 or 32 bytes),
;      and an extra register will be allocated to hold the original stack
;      pointer (to not invalidate r0m etc.). To prevent the use of an extra
;      register as stack pointer, request a negative stack size.
; %4+/%5+ = list of names to define to registers
; PROLOGUE can also be invoked by adding the same options to cglobal

; e.g.
; cglobal foo, 2,3,0, dst, src, tmp
; declares a function (foo), taking two args (dst and src) and one local variable (tmp)

; TODO Some functions can use some args directly from the stack. If they're the
; last args then you can just not declare them, but if they're in the middle
; we need more flexible macro.

; RET:
; Pops anything that was pushed by PROLOGUE, and returns.

; REP_RET:
; Use this instead of RET if it's a branch target.

; registers:
; rN and rNq are the native-size register holding function argument N
; rNd, rNw, rNb are dword, word, and byte size
; rNh is the high 8 bits of the word size
; rNm is the original location of arg N (a register or on the stack), dword
; rNmp is native size

%macro DECLARE_REG 2-3
    %define r%1q %2
    %define r%1d %2d
    %define r%1w %2w
    %define r%1b %2b
    %define r%1h %2h
    %define %2q %2
    %if %0 == 2
        %define r%1m  %2d
        %define r%1mp %2
    %elif ARCH_X86_64 ; memory
        %define r%1m [rstk + stack_offset + %3]
        %define r%1mp qword r %+ %1 %+ m
    %else
        %define r%1m [rstk + stack_offset + %3]
        %define r%1mp dword r %+ %1 %+ m
    %endif
    %define r%1  %2
%endmacro

%macro DECLARE_REG_SIZE 3
    %define r%1q r%1
    %define e%1q r%1
    %define r%1d e%1
    %define e%1d e%1
    %define r%1w %1
    %define e%1w %1
    %define r%1h %3
    %define e%1h %3
    %define r%1b %2
    %define e%1b %2
%if ARCH_X86_64 == 0
    %define r%1  e%1
%endif
%endmacro

DECLARE_REG_SIZE ax, al, ah
DECLARE_REG_SIZE bx, bl, bh
DECLARE_REG_SIZE cx, cl, ch
DECLARE_REG_SIZE dx, dl, dh
DECLARE_REG_SIZE si, sil, null
DECLARE_REG_SIZE di, dil, null
DECLARE_REG_SIZE bp, bpl, null

; t# defines for when per-arch register allocation is more complex than just function arguments

%macro DECLARE_REG_TMP 1-*
    %assign %%i 0
    %rep %0
        CAT_XDEFINE t, %%i, r%1
        %assign %%i %%i+1
        %rotate 1
    %endrep
%endmacro

%macro DECLARE_REG_TMP_SIZE 0-*
    %rep %0
        %define t%1q t%1 %+ q
        %define t%1d t%1 %+ d
        %define t%1w t%1 %+ w
        %define t%1h t%1 %+ h
        %define t%1b t%1 %+ b
        %rotate 1
    %endrep
%endmacro

DECLARE_REG_TMP_SIZE 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14

%if ARCH_X86_64
    %define gprsize 8
%else
    %define gprsize 4
%endif

%macro PUSH 1
    push %1
    %ifidn rstk, rsp
        %assign stack_offset stack_offset+gprsize
    %endif
%endmacro

%macro POP 1
    pop %1
    %ifidn rstk, rsp
        %assign stack_offset stack_offset-gprsize
    %endif
%endmacro

%macro PUSH_IF_USED 1-*
    %rep %0
        %if %1 < regs_used
            PUSH r%1
        %endif
        %rotate 1
    %endrep
%endmacro

%macro POP_IF_USED 1-*
    %rep %0
        %if %1 < regs_used
            pop r%1
        %endif
        %rotate 1
    %endrep
%endmacro

%macro LOAD_IF_USED 1-*
    %rep %0
        %if %1 < num_args
            mov r%1, r %+ %1 %+ mp
        %endif
        %rotate 1
    %endrep
%endmacro

%macro SUB 2
    sub %1, %2
    %ifidn %1, rstk
        %assign stack_offset stack_offset+(%2)
    %endif
%endmacro

%macro ADD 2
    add %1, %2
    %ifidn %1, rstk
        %assign stack_offset stack_offset-(%2)
    %endif
%endmacro

%macro movifnidn 2
    %ifnidn %1, %2
        mov %1, %2
    %endif
%endmacro

%macro movsxdifnidn 2
    %ifnidn %1, %2
        movsxd %1, %2
    %endif
%endmacro

%macro ASSERT 1
    %if (%1) == 0
        %error assert failed
    %endif
%endmacro

%macro DEFINE_ARGS 0-*
    %ifdef n_arg_names
        %assign %%i 0
        %rep n_arg_names
            CAT_UNDEF arg_name %+ %%i, q
            CAT_UNDEF arg_name %+ %%i, d
            CAT_UNDEF arg_name %+ %%i, w
            CAT_UNDEF arg_name %+ %%i, h
            CAT_UNDEF arg_name %+ %%i, b
            CAT_UNDEF arg_name %+ %%i, m
            CAT_UNDEF arg_name %+ %%i, mp
            CAT_UNDEF arg_name, %%i
            %assign %%i %%i+1
        %endrep
    %endif

    %xdefine %%stack_offset stack_offset
    %undef stack_offset ; so that the current value of stack_offset doesn't get baked in by xdefine
    %assign %%i 0
    %rep %0
        %xdefine %1q r %+ %%i %+ q
        %xdefine %1d r %+ %%i %+ d
        %xdefine %1w r %+ %%i %+ w
        %xdefine %1h r %+ %%i %+ h
        %xdefine %1b r %+ %%i %+ b
        %xdefine %1m r %+ %%i %+ m
        %xdefine %1mp r %+ %%i %+ mp
        CAT_XDEFINE arg_name, %%i, %1
        %assign %%i %%i+1
        %rotate 1
    %endrep
    %xdefine stack_offset %%stack_offset
    %assign n_arg_names %0
%endmacro

%macro ALLOC_STACK 1-2 0 ; stack_size, n_xmm_regs (for win64 only)
    %ifnum %1
        %if %1 != 0
            %assign %%stack_alignment ((mmsize + 15) & ~15)
            %assign stack_size %1
            %if stack_size < 0
                %assign stack_size -stack_size
            %endif
            %assign stack_size_padded stack_size
            %if WIN64
                %assign stack_size_padded stack_size_padded + 32 ; reserve 32 bytes for shadow space
                %if mmsize != 8
                    %assign xmm_regs_used %2
                    %if xmm_regs_used > 8
                        %assign stack_size_padded stack_size_padded + (xmm_regs_used-8)*16
                    %endif
                %endif
            %endif
            %if mmsize <= 16 && HAVE_ALIGNED_STACK
                %assign stack_size_padded stack_size_padded + %%stack_alignment - gprsize - (stack_offset & (%%stack_alignment - 1))
                SUB rsp, stack_size_padded
            %else
                %assign %%reg_num (regs_used - 1)
                %xdefine rstk r %+ %%reg_num
                ; align stack, and save original stack location directly above
                ; it, i.e. in [rsp+stack_size_padded], so we can restore the
                ; stack in a single instruction (i.e. mov rsp, rstk or mov
                ; rsp, [rsp+stack_size_padded])
                mov  rstk, rsp
                %if %1 < 0 ; need to store rsp on stack
                    sub  rsp, gprsize+stack_size_padded
                    and  rsp, ~(%%stack_alignment-1)
                    %xdefine rstkm [rsp+stack_size_padded]
                    mov rstkm, rstk
                %else ; can keep rsp in rstk during whole function
                    sub  rsp, stack_size_padded
                    and  rsp, ~(%%stack_alignment-1)
                    %xdefine rstkm rstk
                %endif
            %endif
            WIN64_PUSH_XMM
        %endif
    %endif
%endmacro

%macro SETUP_STACK_POINTER 1
    %ifnum %1
        %if %1 != 0 && (HAVE_ALIGNED_STACK == 0 || mmsize == 32)
            %if %1 > 0
                %assign regs_used (regs_used + 1)
            %elif ARCH_X86_64 && regs_used == num_args && num_args <= 4 + UNIX64 * 2
                %warning "Stack pointer will overwrite register argument"
            %endif
        %endif
    %endif
%endmacro

%macro DEFINE_ARGS_INTERNAL 3+
    %ifnum %2
        DEFINE_ARGS %3
    %elif %1 == 4
        DEFINE_ARGS %2
    %elif %1 > 4
        DEFINE_ARGS %2, %3
    %endif
%endmacro

%if WIN64 ; Windows x64 ;=================================================

DECLARE_REG 0,  rcx
DECLARE_REG 1,  rdx
DECLARE_REG 2,  R8
DECLARE_REG 3,  R9
DECLARE_REG 4,  R10, 40
DECLARE_REG 5,  R11, 48
DECLARE_REG 6,  rax, 56
DECLARE_REG 7,  rdi, 64
DECLARE_REG 8,  rsi, 72
DECLARE_REG 9,  rbx, 80
DECLARE_REG 10, rbp, 88
DECLARE_REG 11, R12, 96
DECLARE_REG 12, R13, 104
DECLARE_REG 13, R14, 112
DECLARE_REG 14, R15, 120

%macro PROLOGUE 2-5+ 0 ; #args, #regs, #xmm_regs, [stack_size,] arg_names...
    %assign num_args %1
    %assign regs_used %2
    ASSERT regs_used >= num_args
    SETUP_STACK_POINTER %4
    ASSERT regs_used <= 15
    PUSH_IF_USED 7, 8, 9, 10, 11, 12, 13, 14
    ALLOC_STACK %4, %3
    %if mmsize != 8 && stack_size == 0
        WIN64_SPILL_XMM %3
    %endif
    LOAD_IF_USED 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
    DEFINE_ARGS_INTERNAL %0, %4, %5
%endmacro

%macro WIN64_PUSH_XMM 0
    ; Use the shadow space to store XMM6 and XMM7, the rest needs stack space allocated.
    %if xmm_regs_used > 6
        movaps [rstk + stack_offset +  8], xmm6
    %endif
    %if xmm_regs_used > 7
        movaps [rstk + stack_offset + 24], xmm7
    %endif
    %if xmm_regs_used > 8
        %assign %%i 8
        %rep xmm_regs_used-8
            movaps [rsp + (%%i-8)*16 + stack_size + 32], xmm %+ %%i
            %assign %%i %%i+1
        %endrep
    %endif
%endmacro

%macro WIN64_SPILL_XMM 1
    %assign xmm_regs_used %1
    ASSERT xmm_regs_used <= 16
    %if xmm_regs_used > 8
        %assign stack_size_padded (xmm_regs_used-8)*16 + (~stack_offset&8) + 32
        SUB rsp, stack_size_padded
    %endif
    WIN64_PUSH_XMM
%endmacro

%macro WIN64_RESTORE_XMM_INTERNAL 1
    %assign %%pad_size 0
    %if xmm_regs_used > 8
        %assign %%i xmm_regs_used
        %rep xmm_regs_used-8
            %assign %%i %%i-1
            movaps xmm %+ %%i, [%1 + (%%i-8)*16 + stack_size + 32]
        %endrep
    %endif
    %if stack_size_padded > 0
        %if stack_size > 0 && (mmsize == 32 || HAVE_ALIGNED_STACK == 0)
            mov rsp, rstkm
        %else
            add %1, stack_size_padded
            %assign %%pad_size stack_size_padded
        %endif
    %endif
    %if xmm_regs_used > 7
        movaps xmm7, [%1 + stack_offset - %%pad_size + 24]
    %endif
    %if xmm_regs_used > 6
        movaps xmm6, [%1 + stack_offset - %%pad_size +  8]
    %endif
%endmacro

%macro WIN64_RESTORE_XMM 1
    WIN64_RESTORE_XMM_INTERNAL %1
    %assign stack_offset (stack_offset-stack_size_padded)
    %assign xmm_regs_used 0
%endmacro

%define has_epilogue regs_used > 7 || xmm_regs_used > 6 || mmsize == 32 || stack_size > 0

%macro RET 0
    WIN64_RESTORE_XMM_INTERNAL rsp
    POP_IF_USED 14, 13, 12, 11, 10, 9, 8, 7
%if mmsize == 32
    vzeroupper
%endif
    AUTO_REP_RET
%endmacro

%elif ARCH_X86_64 ; *nix x64 ;=============================================

DECLARE_REG 0,  rdi
DECLARE_REG 1,  rsi
DECLARE_REG 2,  rdx
DECLARE_REG 3,  rcx
DECLARE_REG 4,  R8
DECLARE_REG 5,  R9
DECLARE_REG 6,  rax, 8
DECLARE_REG 7,  R10, 16
DECLARE_REG 8,  R11, 24
DECLARE_REG 9,  rbx, 32
DECLARE_REG 10, rbp, 40
DECLARE_REG 11, R12, 48
DECLARE_REG 12, R13, 56
DECLARE_REG 13, R14, 64
DECLARE_REG 14, R15, 72

%macro PROLOGUE 2-5+ ; #args, #regs, #xmm_regs, [stack_size,] arg_names...
    %assign num_args %1
    %assign regs_used %2
    ASSERT regs_used >= num_args
    SETUP_STACK_POINTER %4
    ASSERT regs_used <= 15
    PUSH_IF_USED 9, 10, 11, 12, 13, 14
    ALLOC_STACK %4
    LOAD_IF_USED 6, 7, 8, 9, 10, 11, 12, 13, 14
    DEFINE_ARGS_INTERNAL %0, %4, %5
%endmacro

%define has_epilogue regs_used > 9 || mmsize == 32 || stack_size > 0

%macro RET 0
%if stack_size_padded > 0
%if mmsize == 32 || HAVE_ALIGNED_STACK == 0
    mov rsp, rstkm
%else
    add rsp, stack_size_padded
%endif
%endif
    POP_IF_USED 14, 13, 12, 11, 10, 9
%if mmsize == 32
    vzeroupper
%endif
    AUTO_REP_RET
%endmacro

%else ; X86_32 ;==============================================================

DECLARE_REG 0, eax, 4
DECLARE_REG 1, ecx, 8
DECLARE_REG 2, edx, 12
DECLARE_REG 3, ebx, 16
DECLARE_REG 4, esi, 20
DECLARE_REG 5, edi, 24
DECLARE_REG 6, ebp, 28
%define rsp esp

%macro DECLARE_ARG 1-*
    %rep %0
        %define r%1m [rstk + stack_offset + 4*%1 + 4]
        %define r%1mp dword r%1m
        %rotate 1
    %endrep
%endmacro

DECLARE_ARG 7, 8, 9, 10, 11, 12, 13, 14

%macro PROLOGUE 2-5+ ; #args, #regs, #xmm_regs, [stack_size,] arg_names...
    %assign num_args %1
    %assign regs_used %2
    ASSERT regs_used >= num_args
    %if num_args > 7
        %assign num_args 7
    %endif
    %if regs_used > 7
        %assign regs_used 7
    %endif
    SETUP_STACK_POINTER %4
    ASSERT regs_used <= 7
    PUSH_IF_USED 3, 4, 5, 6
    ALLOC_STACK %4
    LOAD_IF_USED 0, 1, 2, 3, 4, 5, 6
    DEFINE_ARGS_INTERNAL %0, %4, %5
%endmacro

%define has_epilogue regs_used > 3 || mmsize == 32 || stack_size > 0

%macro RET 0
%if stack_size_padded > 0
%if mmsize == 32 || HAVE_ALIGNED_STACK == 0
    mov rsp, rstkm
%else
    add rsp, stack_size_padded
%endif
%endif
    POP_IF_USED 6, 5, 4, 3
%if mmsize == 32
    vzeroupper
%endif
    AUTO_REP_RET
%endmacro

%endif ;======================================================================

%if WIN64 == 0
%macro WIN64_SPILL_XMM 1
%endmacro
%macro WIN64_RESTORE_XMM 1
%endmacro
%macro WIN64_PUSH_XMM 0
%endmacro
%endif

; On AMD cpus <=K10, an ordinary ret is slow if it immediately follows either
; a branch or a branch target. So switch to a 2-byte form of ret in that case.
; We can automatically detect "follows a branch", but not a branch target.
; (SSSE3 is a sufficient condition to know that your cpu doesn't have this problem.)
%macro REP_RET 0
    %if has_epilogue
        RET
    %else
        rep ret
    %endif
%endmacro

%define last_branch_adr $$
%macro AUTO_REP_RET 0
    %ifndef cpuflags
        times ((last_branch_adr-$)>>31)+1 rep ; times 1 iff $ != last_branch_adr.
    %elif notcpuflag(ssse3)
        times ((last_branch_adr-$)>>31)+1 rep
    %endif
    ret
%endmacro

%macro BRANCH_INSTR 0-*
    %rep %0
        %macro %1 1-2 %1
            %2 %1
            %%branch_instr:
            %xdefine last_branch_adr %%branch_instr
        %endmacro
        %rotate 1
    %endrep
%endmacro

BRANCH_INSTR jz, je, jnz, jne, jl, jle, jnl, jnle, jg, jge, jng, jnge, ja, jae, jna, jnae, jb, jbe, jnb, jnbe, jc, jnc, js, jns, jo, jno, jp, jnp

%macro TAIL_CALL 2 ; callee, is_nonadjacent
    %if has_epilogue
        call %1
        RET
    %elif %2
        jmp %1
    %endif
%endmacro

;=============================================================================
; arch-independent part
;=============================================================================

%assign function_align 16

; Begin a function.
; Applies any symbol mangling needed for C linkage, and sets up a define such that
; subsequent uses of the function name automatically refer to the mangled version.
; Appends cpuflags to the function name if cpuflags has been specified.
; The "" empty default parameter is a workaround for nasm, which fails if SUFFIX
; is empty and we call cglobal_internal with just %1 %+ SUFFIX (without %2).
%macro cglobal 1-2+ "" ; name, [PROLOGUE args]
    cglobal_internal 1, %1 %+ SUFFIX, %2
%endmacro
%macro cvisible 1-2+ "" ; name, [PROLOGUE args]
    cglobal_internal 0, %1 %+ SUFFIX, %2
%endmacro
%macro cglobal_internal 2-3+
    %if %1
        %xdefine %%FUNCTION_PREFIX private_prefix
        %xdefine %%VISIBILITY hidden
    %else
        %xdefine %%FUNCTION_PREFIX public_prefix
        %xdefine %%VISIBILITY
    %endif
    %ifndef cglobaled_%2
        %xdefine %2 mangle(%%FUNCTION_PREFIX %+ _ %+ %2)
        %xdefine %2.skip_prologue %2 %+ .skip_prologue
        CAT_XDEFINE cglobaled_, %2, 1
    %endif
    %xdefine current_function %2
    %ifidn __OUTPUT_FORMAT__,elf
        global %2:function %%VISIBILITY
    %else
        global %2
    %endif
    align function_align
    %2:
    RESET_MM_PERMUTATION        ; needed for x86-64, also makes disassembly somewhat nicer
    %xdefine rstk rsp           ; copy of the original stack pointer, used when greater alignment than the known stack alignment is required
    %assign stack_offset 0      ; stack pointer offset relative to the return address
    %assign stack_size 0        ; amount of stack space that can be freely used inside a function
    %assign stack_size_padded 0 ; total amount of allocated stack space, including space for callee-saved xmm registers on WIN64 and alignment padding
    %assign xmm_regs_used 0     ; number of XMM registers requested, used for dealing with callee-saved registers on WIN64
    %ifnidn %3, ""
        PROLOGUE %3
    %endif
%endmacro

%macro cextern 1
    %xdefine %1 mangle(private_prefix %+ _ %+ %1)
    CAT_XDEFINE cglobaled_, %1, 1
    extern %1
%endmacro

; like cextern, but without the prefix
%macro cextern_naked 1
    %xdefine %1 mangle(%1)
    CAT_XDEFINE cglobaled_, %1, 1
    extern %1
%endmacro

%macro const 1-2+
    %xdefine %1 mangle(private_prefix %+ _ %+ %1)
    %ifidn __OUTPUT_FORMAT__,elf
        global %1:data hidden
    %else
        global %1
    %endif
    %1: %2
%endmacro

; This is needed for ELF, otherwise the GNU linker assumes the stack is
; executable by default.
%ifidn __OUTPUT_FORMAT__,elf
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif

; cpuflags

%assign cpuflags_mmx      (1<<0)
%assign cpuflags_mmx2     (1<<1) | cpuflags_mmx
%assign cpuflags_3dnow    (1<<2) | cpuflags_mmx
%assign cpuflags_3dnowext (1<<3) | cpuflags_3dnow
%assign cpuflags_sse      (1<<4) | cpuflags_mmx2
%assign cpuflags_sse2     (1<<5) | cpuflags_sse
%assign cpuflags_sse2slow (1<<6) | cpuflags_sse2
%assign cpuflags_sse3     (1<<7) | cpuflags_sse2
%assign cpuflags_ssse3    (1<<8) | cpuflags_sse3
%assign cpuflags_sse4     (1<<9) | cpuflags_ssse3
%assign cpuflags_sse42    (1<<10)| cpuflags_sse4
%assign cpuflags_avx      (1<<11)| cpuflags_sse42
%assign cpuflags_xop      (1<<12)| cpuflags_avx
%assign cpuflags_fma4     (1<<13)| cpuflags_avx
%assign cpuflags_avx2     (1<<14)| cpuflags_avx
%assign cpuflags_fma3     (1<<15)| cpuflags_avx

%assign cpuflags_cache32  (1<<16)
%assign cpuflags_cache64  (1<<17)
%assign cpuflags_slowctz  (1<<18)
%assign cpuflags_lzcnt    (1<<19)
%assign cpuflags_aligned  (1<<20) ; not a cpu feature, but a function variant
%assign cpuflags_atom     (1<<21)
%assign cpuflags_bmi1     (1<<22)|cpuflags_lzcnt
%assign cpuflags_bmi2     (1<<23)|cpuflags_bmi1

%define    cpuflag(x) ((cpuflags & (cpuflags_ %+ x)) == (cpuflags_ %+ x))
%define notcpuflag(x) ((cpuflags & (cpuflags_ %+ x)) != (cpuflags_ %+ x))

; Takes up to 2 cpuflags from the above list.
; All subsequent functions (up to the next INIT_CPUFLAGS) is built for the specified cpu.
; You shouldn't need to invoke this macro directly, it's a subroutine for INIT_MMX &co.
%macro INIT_CPUFLAGS 0-2
    CPUNOP amdnop
    %if %0 >= 1
        %xdefine cpuname %1
        %assign cpuflags cpuflags_%1
        %if %0 >= 2
            %xdefine cpuname %1_%2
            %assign cpuflags cpuflags | cpuflags_%2
        %endif
        %xdefine SUFFIX _ %+ cpuname
        %if cpuflag(avx)
            %assign avx_enabled 1
        %endif
        %if (mmsize == 16 && notcpuflag(sse2)) || (mmsize == 32 && notcpuflag(avx2))
            %define mova movaps
            %define movu movups
            %define movnta movntps
        %endif
        %if cpuflag(aligned)
            %define movu mova
        %elifidn %1, sse3
            %define movu lddqu
        %endif
        %if notcpuflag(sse2)
            CPUNOP basicnop
        %endif
    %else
        %xdefine SUFFIX
        %undef cpuname
        %undef cpuflags
    %endif
%endmacro

; Merge mmx and sse*
; m# is a simd regsiter of the currently selected size
; xm# is the corresponding xmmreg (if selcted xmm or ymm size), or mmreg (if selected mmx)
; ym# is the corresponding ymmreg (if selcted xmm or ymm size), or mmreg (if selected mmx)
; (All 3 remain in sync through SWAP.)

%macro CAT_XDEFINE 3
    %xdefine %1%2 %3
%endmacro

%macro CAT_UNDEF 2
    %undef %1%2
%endmacro

%macro INIT_MMX 0-1+
    %assign avx_enabled 0
    %define RESET_MM_PERMUTATION INIT_MMX %1
    %define mmsize 8
    %define num_mmregs 8
    %define mova movq
    %define movu movq
    %define movh movd
    %define movnta movntq
    %assign %%i 0
    %rep 8
    CAT_XDEFINE m, %%i, mm %+ %%i
    CAT_XDEFINE nmm, %%i, %%i
    %assign %%i %%i+1
    %endrep
    %rep 8
    CAT_UNDEF m, %%i
    CAT_UNDEF nmm, %%i
    %assign %%i %%i+1
    %endrep
    INIT_CPUFLAGS %1
%endmacro

%macro INIT_XMM 0-1+
    %assign avx_enabled 0
    %define RESET_MM_PERMUTATION INIT_XMM %1
    %define mmsize 16
    %define num_mmregs 8
    %if ARCH_X86_64
    %define num_mmregs 16
    %endif
    %define mova movdqa
    %define movu movdqu
    %define movh movq
    %define movnta movntdq
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE m, %%i, xmm %+ %%i
    CAT_XDEFINE nxmm, %%i, %%i
    %assign %%i %%i+1
    %endrep
    INIT_CPUFLAGS %1
%endmacro

%macro INIT_YMM 0-1+
    %assign avx_enabled 1
    %define RESET_MM_PERMUTATION INIT_YMM %1
    %define mmsize 32
    %define num_mmregs 8
    %if ARCH_X86_64
    %define num_mmregs 16
    %endif
    %define mova movdqa
    %define movu movdqu
    %undef movh
    %define movnta movntdq
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE m, %%i, ymm %+ %%i
    CAT_XDEFINE nymm, %%i, %%i
    %assign %%i %%i+1
    %endrep
    INIT_CPUFLAGS %1
%endmacro

INIT_XMM

%macro DECLARE_MMCAST 1
    %define  mmmm%1   mm%1
    %define  mmxmm%1  mm%1
    %define  mmymm%1  mm%1
    %define xmmmm%1   mm%1
    %define xmmxmm%1 xmm%1
    %define xmmymm%1 xmm%1
    %define ymmmm%1   mm%1
    %define ymmxmm%1 ymm%1
    %define ymmymm%1 ymm%1
    %define xm%1 xmm %+ m%1
    %define ym%1 ymm %+ m%1
%endmacro

%assign i 0
%rep 16
    DECLARE_MMCAST i
%assign i i+1
%endrep

; I often want to use macros that permute their arguments. e.g. there's no
; efficient way to implement butterfly or transpose or dct without swapping some
; arguments.
;
; I would like to not have to manually keep track of the permutations:
; If I insert a permutation in the middle of a function, it should automatically
; change everything that follows. For more complex macros I may also have multiple
; implementations, e.g. the SSE2 and SSSE3 versions may have different permutations.
;
; Hence these macros. Insert a PERMUTE or some SWAPs at the end of a macro that
; permutes its arguments. It's equivalent to exchanging the contents of the
; registers, except that this way you exchange the register names instead, so it
; doesn't cost any cycles.

%macro PERMUTE 2-* ; takes a list of pairs to swap
%rep %0/2
    %xdefine %%tmp%2 m%2
    %rotate 2
%endrep
%rep %0/2
    %xdefine m%1 %%tmp%2
    CAT_XDEFINE n, m%1, %1
    %rotate 2
%endrep
%endmacro

%macro SWAP 2+ ; swaps a single chain (sometimes more concise than pairs)
%ifnum %1 ; SWAP 0, 1, ...
    SWAP_INTERNAL_NUM %1, %2
%else ; SWAP m0, m1, ...
    SWAP_INTERNAL_NAME %1, %2
%endif
%endmacro

%macro SWAP_INTERNAL_NUM 2-*
    %rep %0-1
        %xdefine %%tmp m%1
        %xdefine m%1 m%2
        %xdefine m%2 %%tmp
        CAT_XDEFINE n, m%1, %1
        CAT_XDEFINE n, m%2, %2
    %rotate 1
    %endrep
%endmacro

%macro SWAP_INTERNAL_NAME 2-*
    %xdefine %%args n %+ %1
    %rep %0-1
        %xdefine %%args %%args, n %+ %2
    %rotate 1
    %endrep
    SWAP_INTERNAL_NUM %%args
%endmacro

; If SAVE_MM_PERMUTATION is placed at the end of a function, then any later
; calls to that function will automatically load the permutation, so values can
; be returned in mmregs.
%macro SAVE_MM_PERMUTATION 0-1
    %if %0
        %xdefine %%f %1_m
    %else
        %xdefine %%f current_function %+ _m
    %endif
    %assign %%i 0
    %rep num_mmregs
        CAT_XDEFINE %%f, %%i, m %+ %%i
    %assign %%i %%i+1
    %endrep
%endmacro

%macro LOAD_MM_PERMUTATION 1 ; name to load from
    %ifdef %1_m0
        %assign %%i 0
        %rep num_mmregs
            CAT_XDEFINE m, %%i, %1_m %+ %%i
            CAT_XDEFINE n, m %+ %%i, %%i
        %assign %%i %%i+1
        %endrep
    %endif
%endmacro

; Append cpuflags to the callee's name iff the appended name is known and the plain name isn't
%macro call 1
    call_internal %1 %+ SUFFIX, %1
%endmacro
%macro call_internal 2
    %xdefine %%i %2
    %ifndef cglobaled_%2
        %ifdef cglobaled_%1
            %xdefine %%i %1
        %endif
    %endif
    call %%i
    LOAD_MM_PERMUTATION %%i
%endmacro

; Substitutions that reduce instruction size but are functionally equivalent
%macro add 2
    %ifnum %2
        %if %2==128
            sub %1, -128
        %else
            add %1, %2
        %endif
    %else
        add %1, %2
    %endif
%endmacro

%macro sub 2
    %ifnum %2
        %if %2==128
            add %1, -128
        %else
            sub %1, %2
        %endif
    %else
        sub %1, %2
    %endif
%endmacro

;=============================================================================
; AVX abstraction layer
;=============================================================================

%assign i 0
%rep 16
    %if i < 8
        CAT_XDEFINE sizeofmm, i, 8
    %endif
    CAT_XDEFINE sizeofxmm, i, 16
    CAT_XDEFINE sizeofymm, i, 32
%assign i i+1
%endrep
%undef i

%macro CHECK_AVX_INSTR_EMU 3-*
    %xdefine %%opcode %1
    %xdefine %%dst %2
    %rep %0-2
        %ifidn %%dst, %3
            %error non-avx emulation of ``%%opcode'' is not supported
        %endif
        %rotate 1
    %endrep
%endmacro

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 1 if non-destructive or 4-operand (xmm, xmm, xmm, imm), 0 otherwise
;%4 == 1 if commutative (i.e. doesn't matter which src arg is which), 0 if not
;%5+: operands
%macro RUN_AVX_INSTR 5-8+
    %ifnum sizeof%6
        %assign __sizeofreg sizeof%6
    %elifnum sizeof%5
        %assign __sizeofreg sizeof%5
    %else
        %assign __sizeofreg mmsize
    %endif
    %assign __emulate_avx 0
    %if avx_enabled && __sizeofreg >= 16
        %xdefine __instr v%1
    %else
        %xdefine __instr %1
        %if %0 >= 7+%3
            %assign __emulate_avx 1
        %endif
    %endif

    %if __emulate_avx
        %xdefine __src1 %6
        %xdefine __src2 %7
        %ifnidn %5, %6
            %if %0 >= 8
                CHECK_AVX_INSTR_EMU {%1 %5, %6, %7, %8}, %5, %7, %8
            %else
                CHECK_AVX_INSTR_EMU {%1 %5, %6, %7}, %5, %7
            %endif
            %if %4 && %3 == 0
                %ifnid %7
                    ; 3-operand AVX instructions with a memory arg can only have it in src2,
                    ; whereas SSE emulation prefers to have it in src1 (i.e. the mov).
                    ; So, if the instruction is commutative with a memory arg, swap them.
                    %xdefine __src1 %7
                    %xdefine __src2 %6
                %endif
            %endif
            %if __sizeofreg == 8
                MOVQ %5, __src1
            %elif %2
                MOVAPS %5, __src1
            %else
                MOVDQA %5, __src1
            %endif
        %endif
        %if %0 >= 8
            %1 %5, __src2, %8
        %else
            %1 %5, __src2
        %endif
    %elif %0 >= 8
        __instr %5, %6, %7, %8
    %elif %0 == 7
        __instr %5, %6, %7
    %elif %0 == 6
        __instr %5, %6
    %else
        __instr %5
    %endif
%endmacro

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 1 if non-destructive or 4-operand (xmm, xmm, xmm, imm), 0 otherwise
;%4 == 1 if commutative (i.e. doesn't matter which src arg is which), 0 if not
%macro AVX_INSTR 1-4 0, 1, 0
    %macro %1 1-9 fnord, fnord, fnord, fnord, %1, %2, %3, %4
        %ifidn %2, fnord
            RUN_AVX_INSTR %6, %7, %8, %9, %1
        %elifidn %3, fnord
            RUN_AVX_INSTR %6, %7, %8, %9, %1, %2
        %elifidn %4, fnord
            RUN_AVX_INSTR %6, %7, %8, %9, %1, %2, %3
        %elifidn %5, fnord
            RUN_AVX_INSTR %6, %7, %8, %9, %1, %2, %3, %4
        %else
            RUN_AVX_INSTR %6, %7, %8, %9, %1, %2, %3, %4, %5
        %endif
    %endmacro
%endmacro

; Instructions with both VEX and non-VEX encodings
; Non-destructive instructions are written without parameters
AVX_INSTR addpd, 1, 0, 1
AVX_INSTR addps, 1, 0, 1
AVX_INSTR addsd, 1, 0, 1
AVX_INSTR addss, 1, 0, 1
AVX_INSTR addsubpd, 1, 0, 0
AVX_INSTR addsubps, 1, 0, 0
AVX_INSTR aesdec, 0, 0, 0
AVX_INSTR aesdeclast, 0, 0, 0
AVX_INSTR aesenc, 0, 0, 0
AVX_INSTR aesenclast, 0, 0, 0
AVX_INSTR aesimc
AVX_INSTR aeskeygenassist
AVX_INSTR andnpd, 1, 0, 0
AVX_INSTR andnps, 1, 0, 0
AVX_INSTR andpd, 1, 0, 1
AVX_INSTR andps, 1, 0, 1
AVX_INSTR blendpd, 1, 0, 0
AVX_INSTR blendps, 1, 0, 0
AVX_INSTR blendvpd, 1, 0, 0
AVX_INSTR blendvps, 1, 0, 0
AVX_INSTR cmppd, 1, 1, 0
AVX_INSTR cmpps, 1, 1, 0
AVX_INSTR cmpsd, 1, 1, 0
AVX_INSTR cmpss, 1, 1, 0
AVX_INSTR comisd
AVX_INSTR comiss
AVX_INSTR cvtdq2pd
AVX_INSTR cvtdq2ps
AVX_INSTR cvtpd2dq
AVX_INSTR cvtpd2ps
AVX_INSTR cvtps2dq
AVX_INSTR cvtps2pd
AVX_INSTR cvtsd2si
AVX_INSTR cvtsd2ss
AVX_INSTR cvtsi2sd
AVX_INSTR cvtsi2ss
AVX_INSTR cvtss2sd
AVX_INSTR cvtss2si
AVX_INSTR cvttpd2dq
AVX_INSTR cvttps2dq
AVX_INSTR cvttsd2si
AVX_INSTR cvttss2si
AVX_INSTR divpd, 1, 0, 0
AVX_INSTR divps, 1, 0, 0
AVX_INSTR divsd, 1, 0, 0
AVX_INSTR divss, 1, 0, 0
AVX_INSTR dppd, 1, 1, 0
AVX_INSTR dpps, 1, 1, 0
AVX_INSTR extractps
AVX_INSTR haddpd, 1, 0, 0
AVX_INSTR haddps, 1, 0, 0
AVX_INSTR hsubpd, 1, 0, 0
AVX_INSTR hsubps, 1, 0, 0
AVX_INSTR insertps, 1, 1, 0
AVX_INSTR lddqu
AVX_INSTR ldmxcsr
AVX_INSTR maskmovdqu
AVX_INSTR maxpd, 1, 0, 1
AVX_INSTR maxps, 1, 0, 1
AVX_INSTR maxsd, 1, 0, 1
AVX_INSTR maxss, 1, 0, 1
AVX_INSTR minpd, 1, 0, 1
AVX_INSTR minps, 1, 0, 1
AVX_INSTR minsd, 1, 0, 1
AVX_INSTR minss, 1, 0, 1
AVX_INSTR movapd
AVX_INSTR movaps
AVX_INSTR movd
AVX_INSTR movddup
AVX_INSTR movdqa
AVX_INSTR movdqu
AVX_INSTR movhlps, 1, 0, 0
AVX_INSTR movhpd, 1, 0, 0
AVX_INSTR movhps, 1, 0, 0
AVX_INSTR movlhps, 1, 0, 0
AVX_INSTR movlpd, 1, 0, 0
AVX_INSTR movlps, 1, 0, 0
AVX_INSTR movmskpd
AVX_INSTR movmskps
AVX_INSTR movntdq
AVX_INSTR movntdqa
AVX_INSTR movntpd
AVX_INSTR movntps
AVX_INSTR movq
AVX_INSTR movsd, 1, 0, 0
AVX_INSTR movshdup
AVX_INSTR movsldup
AVX_INSTR movss, 1, 0, 0
AVX_INSTR movupd
AVX_INSTR movups
AVX_INSTR mpsadbw, 0, 1, 0
AVX_INSTR mulpd, 1, 0, 1
AVX_INSTR mulps, 1, 0, 1
AVX_INSTR mulsd, 1, 0, 1
AVX_INSTR mulss, 1, 0, 1
AVX_INSTR orpd, 1, 0, 1
AVX_INSTR orps, 1, 0, 1
AVX_INSTR pabsb
AVX_INSTR pabsd
AVX_INSTR pabsw
AVX_INSTR packsswb, 0, 0, 0
AVX_INSTR packssdw, 0, 0, 0
AVX_INSTR packuswb, 0, 0, 0
AVX_INSTR packusdw, 0, 0, 0
AVX_INSTR paddb, 0, 0, 1
AVX_INSTR paddw, 0, 0, 1
AVX_INSTR paddd, 0, 0, 1
AVX_INSTR paddq, 0, 0, 1
AVX_INSTR paddsb, 0, 0, 1
AVX_INSTR paddsw, 0, 0, 1
AVX_INSTR paddusb, 0, 0, 1
AVX_INSTR paddusw, 0, 0, 1
AVX_INSTR palignr, 0, 1, 0
AVX_INSTR pand, 0, 0, 1
AVX_INSTR pandn, 0, 0, 0
AVX_INSTR pavgb, 0, 0, 1
AVX_INSTR pavgw, 0, 0, 1
AVX_INSTR pblendvb, 0, 0, 0
AVX_INSTR pblendw, 0, 1, 0
AVX_INSTR pclmulqdq, 0, 1, 0
AVX_INSTR pcmpestri
AVX_INSTR pcmpestrm
AVX_INSTR pcmpistri
AVX_INSTR pcmpistrm
AVX_INSTR pcmpeqb, 0, 0, 1
AVX_INSTR pcmpeqw, 0, 0, 1
AVX_INSTR pcmpeqd, 0, 0, 1
AVX_INSTR pcmpeqq, 0, 0, 1
AVX_INSTR pcmpgtb, 0, 0, 0
AVX_INSTR pcmpgtw, 0, 0, 0
AVX_INSTR pcmpgtd, 0, 0, 0
AVX_INSTR pcmpgtq, 0, 0, 0
AVX_INSTR pextrb
AVX_INSTR pextrd
AVX_INSTR pextrq
AVX_INSTR pextrw
AVX_INSTR phaddw, 0, 0, 0
AVX_INSTR phaddd, 0, 0, 0
AVX_INSTR phaddsw, 0, 0, 0
AVX_INSTR phminposuw
AVX_INSTR phsubw, 0, 0, 0
AVX_INSTR phsubd, 0, 0, 0
AVX_INSTR phsubsw, 0, 0, 0
AVX_INSTR pinsrb, 0, 1, 0
AVX_INSTR pinsrd, 0, 1, 0
AVX_INSTR pinsrq, 0, 1, 0
AVX_INSTR pinsrw, 0, 1, 0
AVX_INSTR pmaddwd, 0, 0, 1
AVX_INSTR pmaddubsw, 0, 0, 0
AVX_INSTR pmaxsb, 0, 0, 1
AVX_INSTR pmaxsw, 0, 0, 1
AVX_INSTR pmaxsd, 0, 0, 1
AVX_INSTR pmaxub, 0, 0, 1
AVX_INSTR pmaxuw, 0, 0, 1
AVX_INSTR pmaxud, 0, 0, 1
AVX_INSTR pminsb, 0, 0, 1
AVX_INSTR pminsw, 0, 0, 1
AVX_INSTR pminsd, 0, 0, 1
AVX_INSTR pminub, 0, 0, 1
AVX_INSTR pminuw, 0, 0, 1
AVX_INSTR pminud, 0, 0, 1
AVX_INSTR pmovmskb
AVX_INSTR pmovsxbw
AVX_INSTR pmovsxbd
AVX_INSTR pmovsxbq
AVX_INSTR pmovsxwd
AVX_INSTR pmovsxwq
AVX_INSTR pmovsxdq
AVX_INSTR pmovzxbw
AVX_INSTR pmovzxbd
AVX_INSTR pmovzxbq
AVX_INSTR pmovzxwd
AVX_INSTR pmovzxwq
AVX_INSTR pmovzxdq
AVX_INSTR pmuldq, 0, 0, 1
AVX_INSTR pmulhrsw, 0, 0, 1
AVX_INSTR pmulhuw, 0, 0, 1
AVX_INSTR pmulhw, 0, 0, 1
AVX_INSTR pmullw, 0, 0, 1
AVX_INSTR pmulld, 0, 0, 1
AVX_INSTR pmuludq, 0, 0, 1
AVX_INSTR por, 0, 0, 1
AVX_INSTR psadbw, 0, 0, 1
AVX_INSTR pshufb, 0, 0, 0
AVX_INSTR pshufd
AVX_INSTR pshufhw
AVX_INSTR pshuflw
AVX_INSTR psignb, 0, 0, 0
AVX_INSTR psignw, 0, 0, 0
AVX_INSTR psignd, 0, 0, 0
AVX_INSTR psllw, 0, 0, 0
AVX_INSTR pslld, 0, 0, 0
AVX_INSTR psllq, 0, 0, 0
AVX_INSTR pslldq, 0, 0, 0
AVX_INSTR psraw, 0, 0, 0
AVX_INSTR psrad, 0, 0, 0
AVX_INSTR psrlw, 0, 0, 0
AVX_INSTR psrld, 0, 0, 0
AVX_INSTR psrlq, 0, 0, 0
AVX_INSTR psrldq, 0, 0, 0
AVX_INSTR psubb, 0, 0, 0
AVX_INSTR psubw, 0, 0, 0
AVX_INSTR psubd, 0, 0, 0
AVX_INSTR psubq, 0, 0, 0
AVX_INSTR psubsb, 0, 0, 0
AVX_INSTR psubsw, 0, 0, 0
AVX_INSTR psubusb, 0, 0, 0
AVX_INSTR psubusw, 0, 0, 0
AVX_INSTR ptest
AVX_INSTR punpckhbw, 0, 0, 0
AVX_INSTR punpckhwd, 0, 0, 0
AVX_INSTR punpckhdq, 0, 0, 0
AVX_INSTR punpckhqdq, 0, 0, 0
AVX_INSTR punpcklbw, 0, 0, 0
AVX_INSTR punpcklwd, 0, 0, 0
AVX_INSTR punpckldq, 0, 0, 0
AVX_INSTR punpcklqdq, 0, 0, 0
AVX_INSTR pxor, 0, 0, 1
AVX_INSTR rcpps, 1, 0, 0
AVX_INSTR rcpss, 1, 0, 0
AVX_INSTR roundpd
AVX_INSTR roundps
AVX_INSTR roundsd
AVX_INSTR roundss
AVX_INSTR rsqrtps, 1, 0, 0
AVX_INSTR rsqrtss, 1, 0, 0
AVX_INSTR shufpd, 1, 1, 0
AVX_INSTR shufps, 1, 1, 0
AVX_INSTR sqrtpd, 1, 0, 0
AVX_INSTR sqrtps, 1, 0, 0
AVX_INSTR sqrtsd, 1, 0, 0
AVX_INSTR sqrtss, 1, 0, 0
AVX_INSTR stmxcsr
AVX_INSTR subpd, 1, 0, 0
AVX_INSTR subps, 1, 0, 0
AVX_INSTR subsd, 1, 0, 0
AVX_INSTR subss, 1, 0, 0
AVX_INSTR ucomisd
AVX_INSTR ucomiss
AVX_INSTR unpckhpd, 1, 0, 0
AVX_INSTR unpckhps, 1, 0, 0
AVX_INSTR unpcklpd, 1, 0, 0
AVX_INSTR unpcklps, 1, 0, 0
AVX_INSTR xorpd, 1, 0, 1
AVX_INSTR xorps, 1, 0, 1

; 3DNow instructions, for sharing code between AVX, SSE and 3DN
AVX_INSTR pfadd, 1, 0, 1
AVX_INSTR pfsub, 1, 0, 0
AVX_INSTR pfmul, 1, 0, 1

; base-4 constants for shuffles
%assign i 0
%rep 256
    %assign j ((i>>6)&3)*1000 + ((i>>4)&3)*100 + ((i>>2)&3)*10 + (i&3)
    %if j < 10
        CAT_XDEFINE q000, j, i
    %elif j < 100
        CAT_XDEFINE q00, j, i
    %elif j < 1000
        CAT_XDEFINE q0, j, i
    %else
        CAT_XDEFINE q, j, i
    %endif
%assign i i+1
%endrep
%undef i
%undef j

%macro FMA_INSTR 3
    %macro %1 4-7 %1, %2, %3
        %if cpuflag(xop)
            v%5 %1, %2, %3, %4
        %else
            %6 %1, %2, %3
            %7 %1, %4
        %endif
    %endmacro
%endmacro

FMA_INSTR  pmacsdd,  pmulld, paddd
FMA_INSTR  pmacsww,  pmullw, paddw
FMA_INSTR pmadcswd, pmaddwd, paddd

; tzcnt is equivalent to "rep bsf" and is backwards-compatible with bsf.
; This lets us use tzcnt without bumping the yasm version requirement yet.
%define tzcnt rep bsf

; convert FMA4 to FMA3 if possible
%macro FMA4_INSTR 4
    %macro %1 4-8 %1, %2, %3, %4
        %if cpuflag(fma4)
            v%5 %1, %2, %3, %4
        %elifidn %1, %2
            v%6 %1, %4, %3 ; %1 = %1 * %3 + %4
        %elifidn %1, %3
            v%7 %1, %2, %4 ; %1 = %2 * %1 + %4
        %elifidn %1, %4
            v%8 %1, %2, %3 ; %1 = %2 * %3 + %1
        %else
            %error fma3 emulation of ``%5 %1, %2, %3, %4'' is not supported
        %endif
    %endmacro
%endmacro

FMA4_INSTR fmaddpd, fmadd132pd, fmadd213pd, fmadd231pd
FMA4_INSTR fmaddps, fmadd132ps, fmadd213ps, fmadd231ps
FMA4_INSTR fmaddsd, fmadd132sd, fmadd213sd, fmadd231sd
FMA4_INSTR fmaddss, fmadd132ss, fmadd213ss, fmadd231ss

FMA4_INSTR fmaddsubpd, fmaddsub132pd, fmaddsub213pd, fmaddsub231pd
FMA4_INSTR fmaddsubps, fmaddsub132ps, fmaddsub213ps, fmaddsub231ps
FMA4_INSTR fmsubaddpd, fmsubadd132pd, fmsubadd213pd, fmsubadd231pd
FMA4_INSTR fmsubaddps, fmsubadd132ps, fmsubadd213ps, fmsubadd231ps

FMA4_INSTR fmsubpd, fmsub132pd, fmsub213pd, fmsub231pd
FMA4_INSTR fmsubps, fmsub132ps, fmsub213ps, fmsub231ps
FMA4_INSTR fmsubsd, fmsub132sd, fmsub213sd, fmsub231sd
FMA4_INSTR fmsubss, fmsub132ss, fmsub213ss, fmsub231ss

FMA4_INSTR fnmaddpd, fnmadd132pd, fnmadd213pd, fnmadd231pd
FMA4_INSTR fnmaddps, fnmadd132ps, fnmadd213ps, fnmadd231ps
FMA4_INSTR fnmaddsd, fnmadd132sd, fnmadd213sd, fnmadd231sd
FMA4_INSTR fnmaddss, fnmadd132ss, fnmadd213ss, fnmadd231ss

FMA4_INSTR fnmsubpd, fnmsub132pd, fnmsub213pd, fnmsub231pd
FMA4_INSTR fnmsubps, fnmsub132ps, fnmsub213ps, fnmsub231ps
FMA4_INSTR fnmsubsd, fnmsub132sd, fnmsub213sd, fnmsub231sd
FMA4_INSTR fnmsubss, fnmsub132ss, fnmsub213ss, fnmsub231ss

; workaround: vpbroadcastq is broken in x86_32 due to a yasm bug
%if ARCH_X86_64 == 0
%macro vpbroadcastq 2
%if sizeof%1 == 16
    movddup %1, %2
%else
    vbroadcastsd %1, %2
%endif
%endmacro
%endif
