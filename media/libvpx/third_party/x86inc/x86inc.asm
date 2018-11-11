;*****************************************************************************
;* x86inc.asm: x264asm abstraction layer
;*****************************************************************************
;* Copyright (C) 2005-2012 x264 project
;*
;* Authors: Loren Merritt <lorenm@u.washington.edu>
;*          Anton Mitrofanov <BugMaster@narod.ru>
;*          Jason Garrett-Glaser <darkshikari@gmail.com>
;*          Henrik Gramner <hengar-6@student.ltu.se>
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

%include "vpx_config.asm"

%ifndef program_name
%define program_name vp9
%endif


%define UNIX64 0
%define WIN64  0
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

%ifidn   __OUTPUT_FORMAT__,elf32
    %define mangle(x) x
%elifidn __OUTPUT_FORMAT__,elf64
    %define mangle(x) x
%elifidn __OUTPUT_FORMAT__,elf
    %define mangle(x) x
%elifidn __OUTPUT_FORMAT__,x64
    %define mangle(x) x
%elifidn __OUTPUT_FORMAT__,win64
    %define mangle(x) x
%else
    %define mangle(x) _ %+ x
%endif

; FIXME: All of the 64bit asm functions that take a stride as an argument
; via register, assume that the high dword of that register is filled with 0.
; This is true in practice (since we never do any 64bit arithmetic on strides,
; and x264's strides are all positive), but is not guaranteed by the ABI.

; Name of the .rodata section.
; Kludge: Something on OS X fails to align .rodata even given an align attribute,
; so use a different read-only section.
%macro SECTION_RODATA 0-1 16
    %ifidn __OUTPUT_FORMAT__,macho64
        SECTION .text align=%1
    %elifidn __OUTPUT_FORMAT__,macho32
        SECTION .text align=%1
        fakegot:
    %elifidn __OUTPUT_FORMAT__,macho
        SECTION .text align=%1
        fakegot:
    %elifidn __OUTPUT_FORMAT__,aout
        section .text
    %else
        SECTION .rodata align=%1
    %endif
%endmacro

; aout does not support align=
%macro SECTION_TEXT 0-1 16
    %ifidn __OUTPUT_FORMAT__,aout
        SECTION .text
    %else
        SECTION .text align=%1
    %endif
%endmacro

; PIC macros are copied from vpx_ports/x86_abi_support.asm. The "define PIC"
; from original code is added in for 64bit.
%ifidn __OUTPUT_FORMAT__,elf32
%define ABI_IS_32BIT 1
%elifidn __OUTPUT_FORMAT__,macho32
%define ABI_IS_32BIT 1
%elifidn __OUTPUT_FORMAT__,win32
%define ABI_IS_32BIT 1
%elifidn __OUTPUT_FORMAT__,aout
%define ABI_IS_32BIT 1
%else
%define ABI_IS_32BIT 0
%endif

%if ABI_IS_32BIT
  %if CONFIG_PIC=1
  %ifidn __OUTPUT_FORMAT__,elf32
    %define GET_GOT_SAVE_ARG 1
    %define WRT_PLT wrt ..plt
    %macro GET_GOT 1
      extern _GLOBAL_OFFSET_TABLE_
      push %1
      call %%get_got
      %%sub_offset:
      jmp %%exitGG
      %%get_got:
      mov %1, [esp]
      add %1, _GLOBAL_OFFSET_TABLE_ + $$ - %%sub_offset wrt ..gotpc
      ret
      %%exitGG:
      %undef GLOBAL
      %define GLOBAL(x) x + %1 wrt ..gotoff
      %undef RESTORE_GOT
      %define RESTORE_GOT pop %1
    %endmacro
  %elifidn __OUTPUT_FORMAT__,macho32
    %define GET_GOT_SAVE_ARG 1
    %macro GET_GOT 1
      push %1
      call %%get_got
      %%get_got:
      pop  %1
      %undef GLOBAL
      %define GLOBAL(x) x + %1 - %%get_got
      %undef RESTORE_GOT
      %define RESTORE_GOT pop %1
    %endmacro
  %endif
  %endif

  %if ARCH_X86_64 == 0
    %undef PIC
  %endif

%else
  %macro GET_GOT 1
  %endmacro
  %define GLOBAL(x) rel x
  %define WRT_PLT wrt ..plt

  %if WIN64
    %define PIC
  %elifidn __OUTPUT_FORMAT__,macho64
    %define PIC
  %elif CONFIG_PIC
    %define PIC
  %endif
%endif

%ifnmacro GET_GOT
    %macro GET_GOT 1
    %endmacro
    %define GLOBAL(x) x
%endif
%ifndef RESTORE_GOT
%define RESTORE_GOT
%endif
%ifndef WRT_PLT
%define WRT_PLT
%endif

%ifdef PIC
    default rel
%endif
; Done with PIC macros

; Always use long nops (reduces 0x90 spam in disassembly on x86_32)
%ifndef __NASM_VER__
CPU amdnop
%else
%use smartalign
ALIGNMODE k7
%endif

; Macros to eliminate most code duplication between x86_32 and x86_64:
; Currently this works only for leaf functions which load all their arguments
; into registers at the start, and make no other use of the stack. Luckily that
; covers most of x264's asm.

; PROLOGUE:
; %1 = number of arguments. loads them from stack if needed.
; %2 = number of registers used. pushes callee-saved regs if needed.
; %3 = number of xmm registers used. pushes callee-saved xmm regs if needed.
; %4 = list of names to define to registers
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
; Same, but if it doesn't pop anything it becomes a 2-byte ret, for athlons
; which are slow when a normal ret follows a branch.

; registers:
; rN and rNq are the native-size register holding function argument N
; rNd, rNw, rNb are dword, word, and byte size
; rNm is the original location of arg N (a register or on the stack), dword
; rNmp is native size

%macro DECLARE_REG 5-6
    %define r%1q %2
    %define r%1d %3
    %define r%1w %4
    %define r%1b %5
    %if %0 == 5
        %define r%1m  %3
        %define r%1mp %2
    %elif ARCH_X86_64 ; memory
        %define r%1m [rsp + stack_offset + %6]
        %define r%1mp qword r %+ %1 %+ m
    %else
        %define r%1m [esp + stack_offset + %6]
        %define r%1mp dword r %+ %1 %+ m
    %endif
    %define r%1  %2
%endmacro

%macro DECLARE_REG_SIZE 2
    %define r%1q r%1
    %define e%1q r%1
    %define r%1d e%1
    %define e%1d e%1
    %define r%1w %1
    %define e%1w %1
    %define r%1b %2
    %define e%1b %2
%if ARCH_X86_64 == 0
    %define r%1  e%1
%endif
%endmacro

DECLARE_REG_SIZE ax, al
DECLARE_REG_SIZE bx, bl
DECLARE_REG_SIZE cx, cl
DECLARE_REG_SIZE dx, dl
DECLARE_REG_SIZE si, sil
DECLARE_REG_SIZE di, dil
DECLARE_REG_SIZE bp, bpl

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
    %assign stack_offset stack_offset+gprsize
%endmacro

%macro POP 1
    pop %1
    %assign stack_offset stack_offset-gprsize
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
    %ifidn %1, rsp
        %assign stack_offset stack_offset+(%2)
    %endif
%endmacro

%macro ADD 2
    add %1, %2
    %ifidn %1, rsp
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

%if ARCH_X86_64
%macro ALLOC_STACK 2  ; stack_size, num_regs
  %assign %%stack_aligment ((mmsize + 15) & ~15)
  %assign stack_size_padded %1

  %assign %%reg_num (%2 - 1)
  %xdefine rsp_tmp r %+ %%reg_num
  mov  rsp_tmp, rsp
  sub  rsp, stack_size_padded
  and  rsp, ~(%%stack_aligment - 1)
%endmacro

%macro RESTORE_STACK 0  ; reset rsp register
  mov  rsp, rsp_tmp
%endmacro
%endif

%if WIN64 ; Windows x64 ;=================================================

DECLARE_REG 0,  rcx, ecx,  cx,   cl
DECLARE_REG 1,  rdx, edx,  dx,   dl
DECLARE_REG 2,  R8,  R8D,  R8W,  R8B
DECLARE_REG 3,  R9,  R9D,  R9W,  R9B
DECLARE_REG 4,  R10, R10D, R10W, R10B, 40
DECLARE_REG 5,  R11, R11D, R11W, R11B, 48
DECLARE_REG 6,  rax, eax,  ax,   al,   56
DECLARE_REG 7,  rdi, edi,  di,   dil,  64
DECLARE_REG 8,  rsi, esi,  si,   sil,  72
DECLARE_REG 9,  rbx, ebx,  bx,   bl,   80
DECLARE_REG 10, rbp, ebp,  bp,   bpl,  88
DECLARE_REG 11, R12, R12D, R12W, R12B, 96
DECLARE_REG 12, R13, R13D, R13W, R13B, 104
DECLARE_REG 13, R14, R14D, R14W, R14B, 112
DECLARE_REG 14, R15, R15D, R15W, R15B, 120

%macro PROLOGUE 2-4+ 0 ; #args, #regs, #xmm_regs, arg_names...
    %assign num_args %1
    %assign regs_used %2
    ASSERT regs_used >= num_args
    ASSERT regs_used <= 15
    PUSH_IF_USED 7, 8, 9, 10, 11, 12, 13, 14
    %if mmsize == 8
        %assign xmm_regs_used 0
    %else
        WIN64_SPILL_XMM %3
    %endif
    LOAD_IF_USED 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
    DEFINE_ARGS %4
%endmacro

%macro WIN64_SPILL_XMM 1
    %assign xmm_regs_used %1
    ASSERT xmm_regs_used <= 16
    %if xmm_regs_used > 6
        SUB rsp, (xmm_regs_used-6)*16+16
        %assign %%i xmm_regs_used
        %rep (xmm_regs_used-6)
            %assign %%i %%i-1
            movdqa [rsp + (%%i-6)*16+(~stack_offset&8)], xmm %+ %%i
        %endrep
    %endif
%endmacro

%macro WIN64_RESTORE_XMM_INTERNAL 1
    %if xmm_regs_used > 6
        %assign %%i xmm_regs_used
        %rep (xmm_regs_used-6)
            %assign %%i %%i-1
            movdqa xmm %+ %%i, [%1 + (%%i-6)*16+(~stack_offset&8)]
        %endrep
        add %1, (xmm_regs_used-6)*16+16
    %endif
%endmacro

%macro WIN64_RESTORE_XMM 1
    WIN64_RESTORE_XMM_INTERNAL %1
    %assign stack_offset stack_offset-(xmm_regs_used-6)*16+16
    %assign xmm_regs_used 0
%endmacro

%macro RET 0
    WIN64_RESTORE_XMM_INTERNAL rsp
    POP_IF_USED 14, 13, 12, 11, 10, 9, 8, 7
    ret
%endmacro

%macro REP_RET 0
    %if regs_used > 7 || xmm_regs_used > 6
        RET
    %else
        rep ret
    %endif
%endmacro

%elif ARCH_X86_64 ; *nix x64 ;=============================================

DECLARE_REG 0,  rdi, edi,  di,   dil
DECLARE_REG 1,  rsi, esi,  si,   sil
DECLARE_REG 2,  rdx, edx,  dx,   dl
DECLARE_REG 3,  rcx, ecx,  cx,   cl
DECLARE_REG 4,  R8,  R8D,  R8W,  R8B
DECLARE_REG 5,  R9,  R9D,  R9W,  R9B
DECLARE_REG 6,  rax, eax,  ax,   al,   8
DECLARE_REG 7,  R10, R10D, R10W, R10B, 16
DECLARE_REG 8,  R11, R11D, R11W, R11B, 24
DECLARE_REG 9,  rbx, ebx,  bx,   bl,   32
DECLARE_REG 10, rbp, ebp,  bp,   bpl,  40
DECLARE_REG 11, R12, R12D, R12W, R12B, 48
DECLARE_REG 12, R13, R13D, R13W, R13B, 56
DECLARE_REG 13, R14, R14D, R14W, R14B, 64
DECLARE_REG 14, R15, R15D, R15W, R15B, 72

%macro PROLOGUE 2-4+ ; #args, #regs, #xmm_regs, arg_names...
    %assign num_args %1
    %assign regs_used %2
    ASSERT regs_used >= num_args
    ASSERT regs_used <= 15
    PUSH_IF_USED 9, 10, 11, 12, 13, 14
    LOAD_IF_USED 6, 7, 8, 9, 10, 11, 12, 13, 14
    DEFINE_ARGS %4
%endmacro

%macro RET 0
    POP_IF_USED 14, 13, 12, 11, 10, 9
    ret
%endmacro

%macro REP_RET 0
    %if regs_used > 9
        RET
    %else
        rep ret
    %endif
%endmacro

%else ; X86_32 ;==============================================================

DECLARE_REG 0, eax, eax, ax, al,   4
DECLARE_REG 1, ecx, ecx, cx, cl,   8
DECLARE_REG 2, edx, edx, dx, dl,   12
DECLARE_REG 3, ebx, ebx, bx, bl,   16
DECLARE_REG 4, esi, esi, si, null, 20
DECLARE_REG 5, edi, edi, di, null, 24
DECLARE_REG 6, ebp, ebp, bp, null, 28
%define rsp esp

%macro DECLARE_ARG 1-*
    %rep %0
        %define r%1m [esp + stack_offset + 4*%1 + 4]
        %define r%1mp dword r%1m
        %rotate 1
    %endrep
%endmacro

DECLARE_ARG 7, 8, 9, 10, 11, 12, 13, 14

%macro PROLOGUE 2-4+ ; #args, #regs, #xmm_regs, arg_names...
    %assign num_args %1
    %assign regs_used %2
    %if regs_used > 7
        %assign regs_used 7
    %endif
    ASSERT regs_used >= num_args
    PUSH_IF_USED 3, 4, 5, 6
    LOAD_IF_USED 0, 1, 2, 3, 4, 5, 6
    DEFINE_ARGS %4
%endmacro

%macro RET 0
    POP_IF_USED 6, 5, 4, 3
    ret
%endmacro

%macro REP_RET 0
    %if regs_used > 3
        RET
    %else
        rep ret
    %endif
%endmacro

%endif ;======================================================================

%if WIN64 == 0
%macro WIN64_SPILL_XMM 1
%endmacro
%macro WIN64_RESTORE_XMM 1
%endmacro
%endif

;=============================================================================
; arch-independent part
;=============================================================================

%assign function_align 16

; Begin a function.
; Applies any symbol mangling needed for C linkage, and sets up a define such that
; subsequent uses of the function name automatically refer to the mangled version.
; Appends cpuflags to the function name if cpuflags has been specified.
%macro cglobal 1-2+ ; name, [PROLOGUE args]
%if %0 == 1
    cglobal_internal %1 %+ SUFFIX
%else
    cglobal_internal %1 %+ SUFFIX, %2
%endif
%endmacro
%macro cglobal_internal 1-2+
    %ifndef cglobaled_%1
        %xdefine %1 mangle(program_name %+ _ %+ %1)
        %xdefine %1.skip_prologue %1 %+ .skip_prologue
        CAT_XDEFINE cglobaled_, %1, 1
    %endif
    %xdefine current_function %1
    %ifdef CHROMIUM
        %ifidn __OUTPUT_FORMAT__,elf
            global %1:function hidden
        %elifidn __OUTPUT_FORMAT__,elf32
            global %1:function hidden
        %elifidn __OUTPUT_FORMAT__,elf64
            global %1:function hidden
        %elifidn __OUTPUT_FORMAT__,macho32
            %ifdef __NASM_VER__
                global %1
            %else
                global %1:private_extern
            %endif
        %elifidn __OUTPUT_FORMAT__,macho64
            %ifdef __NASM_VER__
                global %1
            %else
                global %1:private_extern
            %endif
        %else
            global %1
        %endif
    %else
        global %1
    %endif
    align function_align
    %1:
    RESET_MM_PERMUTATION ; not really needed, but makes disassembly somewhat nicer
    %assign stack_offset 0
    %if %0 > 1
        PROLOGUE %2
    %endif
%endmacro

%macro cextern 1
    %xdefine %1 mangle(program_name %+ _ %+ %1)
    CAT_XDEFINE cglobaled_, %1, 1
    extern %1
%endmacro

; like cextern, but without the prefix
%macro cextern_naked 1
    %xdefine %1 mangle(%1)
    CAT_XDEFINE cglobaled_, %1, 1
    extern %1
%endmacro

%macro const 2+
    %xdefine %1 mangle(program_name %+ _ %+ %1)
    global %1
    %1: %2
%endmacro

; This is needed for ELF, otherwise the GNU linker assumes the stack is
; executable by default.
%ifidn __OUTPUT_FORMAT__,elf
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%elifidn __OUTPUT_FORMAT__,elf32
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%elifidn __OUTPUT_FORMAT__,elf64
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif

; cpuflags

%assign cpuflags_mmx      (1<<0)
%assign cpuflags_mmx2     (1<<1) | cpuflags_mmx
%assign cpuflags_3dnow    (1<<2) | cpuflags_mmx
%assign cpuflags_3dnow2   (1<<3) | cpuflags_3dnow
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

%assign cpuflags_cache32  (1<<16)
%assign cpuflags_cache64  (1<<17)
%assign cpuflags_slowctz  (1<<18)
%assign cpuflags_lzcnt    (1<<19)
%assign cpuflags_misalign (1<<20)
%assign cpuflags_aligned  (1<<21) ; not a cpu feature, but a function variant
%assign cpuflags_atom     (1<<22)

%define    cpuflag(x) ((cpuflags & (cpuflags_ %+ x)) == (cpuflags_ %+ x))
%define notcpuflag(x) ((cpuflags & (cpuflags_ %+ x)) != (cpuflags_ %+ x))

; Takes up to 2 cpuflags from the above list.
; All subsequent functions (up to the next INIT_CPUFLAGS) is built for the specified cpu.
; You shouldn't need to invoke this macro directly, it's a subroutine for INIT_MMX &co.
%macro INIT_CPUFLAGS 0-2
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
        %if mmsize == 16 && notcpuflag(sse2)
            %define mova movaps
            %define movu movups
            %define movnta movntps
        %endif
        %if cpuflag(aligned)
            %define movu mova
        %elifidn %1, sse3
            %define movu lddqu
        %endif
    %else
        %xdefine SUFFIX
        %undef cpuname
        %undef cpuflags
    %endif
%endmacro

; merge mmx and sse*

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

; FIXME: INIT_AVX can be replaced by INIT_XMM avx
%macro INIT_AVX 0
    INIT_XMM
    %assign avx_enabled 1
    %define PALIGNR PALIGNR_SSSE3
    %define RESET_MM_PERMUTATION INIT_AVX
%endmacro

%macro INIT_YMM 0-1+
    %assign avx_enabled 1
    %define RESET_MM_PERMUTATION INIT_YMM %1
    %define mmsize 32
    %define num_mmregs 8
    %if ARCH_X86_64
    %define num_mmregs 16
    %endif
    %define mova vmovaps
    %define movu vmovups
    %undef movh
    %define movnta vmovntps
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE m, %%i, ymm %+ %%i
    CAT_XDEFINE nymm, %%i, %%i
    %assign %%i %%i+1
    %endrep
    INIT_CPUFLAGS %1
%endmacro

INIT_XMM

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
    %xdefine tmp%2 m%2
    %xdefine ntmp%2 nm%2
    %rotate 2
%endrep
%rep %0/2
    %xdefine m%1 tmp%2
    %xdefine nm%1 ntmp%2
    %undef tmp%2
    %undef ntmp%2
    %rotate 2
%endrep
%endmacro

%macro SWAP 2-* ; swaps a single chain (sometimes more concise than pairs)
%rep %0-1
%ifdef m%1
    %xdefine tmp m%1
    %xdefine m%1 m%2
    %xdefine m%2 tmp
    CAT_XDEFINE n, m%1, %1
    CAT_XDEFINE n, m%2, %2
%else
    ; If we were called as "SWAP m0,m1" rather than "SWAP 0,1" infer the original numbers here.
    ; Be careful using this mode in nested macros though, as in some cases there may be
    ; other copies of m# that have already been dereferenced and don't get updated correctly.
    %xdefine %%n1 n %+ %1
    %xdefine %%n2 n %+ %2
    %xdefine tmp m %+ %%n1
    CAT_XDEFINE m, %%n1, m %+ %%n2
    CAT_XDEFINE m, %%n2, tmp
    CAT_XDEFINE n, m %+ %%n1, %%n1
    CAT_XDEFINE n, m %+ %%n2, %%n2
%endif
    %undef tmp
    %rotate 1
%endrep
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
    call_internal %1, %1 %+ SUFFIX
%endmacro
%macro call_internal 2
    %xdefine %%i %1
    %ifndef cglobaled_%1
        %ifdef cglobaled_%2
            %xdefine %%i %2
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

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 1 if 4-operand (xmm, xmm, xmm, imm), 0 if 2- or 3-operand (xmm, xmm, xmm)
;%4 == number of operands given
;%5+: operands
%macro RUN_AVX_INSTR 6-7+
    %ifid %5
        %define %%size sizeof%5
    %else
        %define %%size mmsize
    %endif
    %if %%size==32
        %if %0 >= 7
            v%1 %5, %6, %7
        %else
            v%1 %5, %6
        %endif
    %else
        %if %%size==8
            %define %%regmov movq
        %elif %2
            %define %%regmov movaps
        %else
            %define %%regmov movdqa
        %endif

        %if %4>=3+%3
            %ifnidn %5, %6
                %if avx_enabled && sizeof%5==16
                    v%1 %5, %6, %7
                %else
                    %%regmov %5, %6
                    %1 %5, %7
                %endif
            %else
                %1 %5, %7
            %endif
        %elif %3
            %1 %5, %6, %7
        %else
            %1 %5, %6
        %endif
    %endif
%endmacro

; 3arg AVX ops with a memory arg can only have it in src2,
; whereas SSE emulation of 3arg prefers to have it in src1 (i.e. the mov).
; So, if the op is symmetric and the wrong one is memory, swap them.
%macro RUN_AVX_INSTR1 8
    %assign %%swap 0
    %if avx_enabled
        %ifnid %6
            %assign %%swap 1
        %endif
    %elifnidn %5, %6
        %ifnid %7
            %assign %%swap 1
        %endif
    %endif
    %if %%swap && %3 == 0 && %8 == 1
        RUN_AVX_INSTR %1, %2, %3, %4, %5, %7, %6
    %else
        RUN_AVX_INSTR %1, %2, %3, %4, %5, %6, %7
    %endif
%endmacro

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 1 if 4-operand (xmm, xmm, xmm, imm), 0 if 3-operand (xmm, xmm, xmm)
;%4 == 1 if symmetric (i.e. doesn't matter which src arg is which), 0 if not
%macro AVX_INSTR 4
    %macro %1 2-9 fnord, fnord, fnord, %1, %2, %3, %4
        %ifidn %3, fnord
            RUN_AVX_INSTR %6, %7, %8, 2, %1, %2
        %elifidn %4, fnord
            RUN_AVX_INSTR1 %6, %7, %8, 3, %1, %2, %3, %9
        %elifidn %5, fnord
            RUN_AVX_INSTR %6, %7, %8, 4, %1, %2, %3, %4
        %else
            RUN_AVX_INSTR %6, %7, %8, 5, %1, %2, %3, %4, %5
        %endif
    %endmacro
%endmacro

AVX_INSTR addpd, 1, 0, 1
AVX_INSTR addps, 1, 0, 1
AVX_INSTR addsd, 1, 0, 1
AVX_INSTR addss, 1, 0, 1
AVX_INSTR addsubpd, 1, 0, 0
AVX_INSTR addsubps, 1, 0, 0
AVX_INSTR andpd, 1, 0, 1
AVX_INSTR andps, 1, 0, 1
AVX_INSTR andnpd, 1, 0, 0
AVX_INSTR andnps, 1, 0, 0
AVX_INSTR blendpd, 1, 0, 0
AVX_INSTR blendps, 1, 0, 0
AVX_INSTR blendvpd, 1, 0, 0
AVX_INSTR blendvps, 1, 0, 0
AVX_INSTR cmppd, 1, 0, 0
AVX_INSTR cmpps, 1, 0, 0
AVX_INSTR cmpsd, 1, 0, 0
AVX_INSTR cmpss, 1, 0, 0
AVX_INSTR cvtdq2ps, 1, 0, 0
AVX_INSTR cvtps2dq, 1, 0, 0
AVX_INSTR divpd, 1, 0, 0
AVX_INSTR divps, 1, 0, 0
AVX_INSTR divsd, 1, 0, 0
AVX_INSTR divss, 1, 0, 0
AVX_INSTR dppd, 1, 1, 0
AVX_INSTR dpps, 1, 1, 0
AVX_INSTR haddpd, 1, 0, 0
AVX_INSTR haddps, 1, 0, 0
AVX_INSTR hsubpd, 1, 0, 0
AVX_INSTR hsubps, 1, 0, 0
AVX_INSTR maxpd, 1, 0, 1
AVX_INSTR maxps, 1, 0, 1
AVX_INSTR maxsd, 1, 0, 1
AVX_INSTR maxss, 1, 0, 1
AVX_INSTR minpd, 1, 0, 1
AVX_INSTR minps, 1, 0, 1
AVX_INSTR minsd, 1, 0, 1
AVX_INSTR minss, 1, 0, 1
AVX_INSTR movhlps, 1, 0, 0
AVX_INSTR movlhps, 1, 0, 0
AVX_INSTR movsd, 1, 0, 0
AVX_INSTR movss, 1, 0, 0
AVX_INSTR mpsadbw, 0, 1, 0
AVX_INSTR mulpd, 1, 0, 1
AVX_INSTR mulps, 1, 0, 1
AVX_INSTR mulsd, 1, 0, 1
AVX_INSTR mulss, 1, 0, 1
AVX_INSTR orpd, 1, 0, 1
AVX_INSTR orps, 1, 0, 1
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
AVX_INSTR pcmpestri, 0, 0, 0
AVX_INSTR pcmpestrm, 0, 0, 0
AVX_INSTR pcmpistri, 0, 0, 0
AVX_INSTR pcmpistrm, 0, 0, 0
AVX_INSTR pcmpeqb, 0, 0, 1
AVX_INSTR pcmpeqw, 0, 0, 1
AVX_INSTR pcmpeqd, 0, 0, 1
AVX_INSTR pcmpeqq, 0, 0, 1
AVX_INSTR pcmpgtb, 0, 0, 0
AVX_INSTR pcmpgtw, 0, 0, 0
AVX_INSTR pcmpgtd, 0, 0, 0
AVX_INSTR pcmpgtq, 0, 0, 0
AVX_INSTR phaddw, 0, 0, 0
AVX_INSTR phaddd, 0, 0, 0
AVX_INSTR phaddsw, 0, 0, 0
AVX_INSTR phsubw, 0, 0, 0
AVX_INSTR phsubd, 0, 0, 0
AVX_INSTR phsubsw, 0, 0, 0
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
AVX_INSTR pmulhuw, 0, 0, 1
AVX_INSTR pmulhrsw, 0, 0, 1
AVX_INSTR pmulhw, 0, 0, 1
AVX_INSTR pmullw, 0, 0, 1
AVX_INSTR pmulld, 0, 0, 1
AVX_INSTR pmuludq, 0, 0, 1
AVX_INSTR pmuldq, 0, 0, 1
AVX_INSTR por, 0, 0, 1
AVX_INSTR psadbw, 0, 0, 1
AVX_INSTR pshufb, 0, 0, 0
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
AVX_INSTR punpckhbw, 0, 0, 0
AVX_INSTR punpckhwd, 0, 0, 0
AVX_INSTR punpckhdq, 0, 0, 0
AVX_INSTR punpckhqdq, 0, 0, 0
AVX_INSTR punpcklbw, 0, 0, 0
AVX_INSTR punpcklwd, 0, 0, 0
AVX_INSTR punpckldq, 0, 0, 0
AVX_INSTR punpcklqdq, 0, 0, 0
AVX_INSTR pxor, 0, 0, 1
AVX_INSTR shufps, 1, 1, 0
AVX_INSTR subpd, 1, 0, 0
AVX_INSTR subps, 1, 0, 0
AVX_INSTR subsd, 1, 0, 0
AVX_INSTR subss, 1, 0, 0
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
