/*
 * Copyright © 2008 Mozilla Corporation
 * Copyright © 2010 Nokia Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Jeff Muizelaar (jeff@infidigm.net)
 *
 */

#ifndef PIXMAN_ARM_ASM_H
#define PIXMAN_ARM_ASM_H

#ifdef HAVE_CONFIG_H
#include "pixman-config.h"
#endif

/*
 * References:
 *  - https://developer.arm.com/documentation/101028/0012/5--Feature-test-macros
 *  - https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst
 */
#if defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT == 1
  #define BTI_C hint 34  /* bti c: for calls, IE bl instructions */
  #define GNU_PROPERTY_AARCH64_BTI 1 /* bit 0 GNU Notes is for BTI support */
#else
  #define BTI_C
  #define GNU_PROPERTY_AARCH64_BTI 0
#endif

#if defined(__ARM_FEATURE_PAC_DEFAULT)
  #if __ARM_FEATURE_PAC_DEFAULT & 1
    #define SIGN_LR hint 25 /* paciasp: sign with the A key */
    #define VERIFY_LR hint 29 /* autiasp: verify with the b key */
  #elif __ARM_FEATURE_PAC_DEFAULT & 2
    #define SIGN_LR hint 27 /* pacibsp: sign with the b key */
    #define VERIFY_LR hint 31 /* autibsp: verify with the b key */
  #endif
  #define GNU_PROPERTY_AARCH64_POINTER_AUTH 2 /* bit 1 GNU Notes is for PAC support */
#else
  #define SIGN_LR BTI_C
  #define VERIFY_LR
  #define GNU_PROPERTY_AARCH64_POINTER_AUTH 0
#endif

/* Add the BTI support to GNU Notes section for ASM files */
#if GNU_PROPERTY_AARCH64_BTI != 0 || GNU_PROPERTY_AARCH64_POINTER_AUTH != 0
    .pushsection .note.gnu.property, "a"; /* Start a new allocatable section */
    .balign 8; /* align it on a byte boundry */
    .long 4; /* size of "GNU\0" */
    .long 0x10; /* size of descriptor */
    .long 0x5; /* NT_GNU_PROPERTY_TYPE_0 */
    .asciz "GNU";
    .long 0xc0000000; /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
    .long 4; /* Four bytes of data */
    .long (GNU_PROPERTY_AARCH64_BTI|GNU_PROPERTY_AARCH64_POINTER_AUTH); /* BTI or PAC is enabled */
    .long 0; /* padding for 8 byte alignment */
    .popsection; /* end the section */
#endif

/* Supplementary macro for setting function attributes */
.macro pixman_asm_function_impl fname
#ifdef ASM_HAVE_FUNC_DIRECTIVE
	.func \fname
#endif
	.global \fname
#ifdef __ELF__
	.hidden \fname
	.type \fname, %function
#endif
\fname:
	SIGN_LR
.endm

.macro pixman_asm_function fname
#ifdef ASM_LEADING_UNDERSCORE
	pixman_asm_function_impl _\fname
#else
	pixman_asm_function_impl \fname
#endif
.endm

.macro pixman_syntax_unified
#ifdef ASM_HAVE_SYNTAX_UNIFIED
	.syntax unified
#endif
.endm

.macro pixman_end_asm_function
#ifdef ASM_HAVE_FUNC_DIRECTIVE
	.endfunc
#endif
.endm

#endif /* PIXMAN_ARM_ASM_H */
