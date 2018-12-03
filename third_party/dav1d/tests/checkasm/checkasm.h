/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DAV1D_TESTS_CHECKASM_CHECKASM_H
#define __DAV1D_TESTS_CHECKASM_CHECKASM_H

#include "config.h"

#include <stdint.h>
#include <stdlib.h>

#include "include/common/attributes.h"
#include "include/common/intops.h"

void checkasm_check_cdef_8bpc(void);
void checkasm_check_cdef_10bpc(void);

void checkasm_check_ipred_8bpc(void);
void checkasm_check_ipred_10bpc(void);

void checkasm_check_itx_8bpc(void);
void checkasm_check_itx_10bpc(void);

void checkasm_check_loopfilter_8bpc(void);
void checkasm_check_loopfilter_10bpc(void);

void checkasm_check_looprestoration_8bpc(void);
void checkasm_check_looprestoration_10bpc(void);

void checkasm_check_mc_8bpc(void);
void checkasm_check_mc_10bpc(void);

void *checkasm_check_func(void *func, const char *name, ...);
int checkasm_bench_func(void);
void checkasm_fail_func(const char *msg, ...);
void checkasm_update_bench(int iterations, uint64_t cycles);
void checkasm_report(const char *name, ...);

/* float compare utilities */
int float_near_ulp(float a, float b, unsigned max_ulp);
int float_near_abs_eps(float a, float b, float eps);
int float_near_abs_eps_ulp(float a, float b, float eps, unsigned max_ulp);
int float_near_ulp_array(const float *a, const float *b, unsigned max_ulp,
                         int len);
int float_near_abs_eps_array(const float *a, const float *b, float eps,
                             int len);
int float_near_abs_eps_array_ulp(const float *a, const float *b, float eps,
                                 unsigned max_ulp, int len);

static void *func_ref, *func_new;

#define BENCH_RUNS (1 << 12) /* Trade-off between accuracy and speed */

/* Decide whether or not the specified function needs to be tested */
#define check_func(func, ...)\
    (func_ref = checkasm_check_func((func_new = func), __VA_ARGS__))

/* Declare the function prototype. The first argument is the return value,
 * the remaining arguments are the function parameters. Naming parameters
 * is optional. */
#define declare_func(ret, ...)\
    declare_new(ret, __VA_ARGS__) typedef ret func_type(__VA_ARGS__)

/* Indicate that the current test has failed */
#define fail() checkasm_fail_func("%s:%d", __FILE__, __LINE__)

/* Print the test outcome */
#define report checkasm_report

/* Call the reference function */
#define call_ref(...) ((func_type *)func_ref)(__VA_ARGS__)

#if HAVE_ASM
#if ARCH_X86
#ifdef _MSC_VER
#include <intrin.h>
#define readtime() (_mm_lfence(), __rdtsc())
#else
static inline uint64_t readtime(void) {
    uint32_t eax, edx;
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(eax), "=d"(edx));
    return (((uint64_t)edx) << 32) | eax;
}
#define readtime readtime
#endif
#elif ARCH_AARCH64
#ifdef _MSC_VER
#include <windows.h>
#define readtime() (_InstructionSynchronizationBarrier(), ReadTimeStampCounter())
#else
static inline uint64_t readtime(void) {
    uint64_t cycle_counter;
    /* This requires enabling user mode access to the cycle counter (which
     * can only be done from kernel space).
     * This could also read cntvct_el0 instead of pmccntr_el0; that register
     * might also be readable (depending on kernel version), but it has much
     * worse precision (it's a fixed 50 MHz timer). */
    __asm__ __volatile__("isb\nmrs %0, pmccntr_el0"
                         : "=r"(cycle_counter)
                         :: "memory");
    return cycle_counter;
}
#define readtime readtime
#endif
#elif ARCH_ARM && !defined(_MSC_VER)
static inline uint64_t readtime(void) {
    uint32_t cycle_counter;
    /* This requires enabling user mode access to the cycle counter (which
     * can only be done from kernel space). */
    __asm__ __volatile__("isb\nmrc p15, 0, %0, c9, c13, 0"
                         : "=r"(cycle_counter)
                         :: "memory");
    return cycle_counter;
}
#define readtime readtime
#endif

/* Verifies that clobbered callee-saved registers
 * are properly saved and restored */
void checkasm_checked_call(void *func, ...);

#if ARCH_X86_64
/* Evil hack: detect incorrect assumptions that 32-bit ints are zero-extended
 * to 64-bit. This is done by clobbering the stack with junk around the stack
 * pointer and calling the assembly function through checked_call() with added
 * dummy arguments which forces all real arguments to be passed on the stack
 * and not in registers. For 32-bit arguments the upper half of the 64-bit
 * register locations on the stack will now contain junk which will cause
 * misbehaving functions to either produce incorrect output or segfault. Note
 * that even though this works extremely well in practice, it's technically
 * not guaranteed and false negatives is theoretically possible, but there
 * can never be any false positives. */
void checkasm_stack_clobber(uint64_t clobber, ...);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int, int, int, int, int, __VA_ARGS__) =\
    (void *)checkasm_checked_call;
#define CLOB (UINT64_C(0xdeadbeefdeadbeef))
#define call_new(...)\
    (checkasm_stack_clobber(CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB),\
     checked_call(func_new, 0, 0, 0, 0, 0, __VA_ARGS__))
#elif ARCH_X86_32
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, __VA_ARGS__) = (void *)checkasm_checked_call;
#define call_new(...) checked_call(func_new, __VA_ARGS__)
#elif ARCH_ARM
/* Use a dummy argument, to offset the real parameters by 2, not only 1.
 * This makes sure that potential 8-byte-alignment of parameters is kept
 * the same even when the extra parameters have been removed. */
void checkasm_checked_call_vfp(void *func, int dummy, ...);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int dummy, __VA_ARGS__) =\
    (void *)checkasm_checked_call_vfp;
#define call_new(...) checked_call(func_new, 0, __VA_ARGS__)
#elif ARCH_AARCH64 && !defined(__APPLE__)
void checkasm_stack_clobber(uint64_t clobber, ...);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int, int, int, int, int, int, int,\
                        __VA_ARGS__) =\
    (void *)checkasm_checked_call;
#define CLOB (UINT64_C(0xdeadbeefdeadbeef))
#define call_new(...)\
    (checkasm_stack_clobber(CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB),\
     checked_call(func_new, 0, 0, 0, 0, 0, 0, 0, __VA_ARGS__))
#else
#define declare_new(ret, ...)
#define call_new(...) ((func_type *)func_new)(__VA_ARGS__)
#endif
#else /* HAVE_ASM */
#define declare_new(ret, ...)
/* Call the function */
#define call_new(...) ((func_type *)func_new)(__VA_ARGS__)
#endif /* HAVE_ASM */

/* Benchmark the function */
#ifdef readtime
#define bench_new(...)\
    do {\
        if (checkasm_bench_func()) {\
            func_type *tfunc = func_new;\
            uint64_t tsum = 0;\
            int ti, tcount = 0;\
            for (ti = 0; ti < BENCH_RUNS; ti++) {\
                uint64_t t = readtime();\
                tfunc(__VA_ARGS__);\
                tfunc(__VA_ARGS__);\
                tfunc(__VA_ARGS__);\
                tfunc(__VA_ARGS__);\
                t = readtime() - t;\
                if (t*tcount <= tsum*4 && ti > 0) {\
                    tsum += t;\
                    tcount++;\
                }\
            }\
            checkasm_update_bench(tcount, tsum);\
        }\
    } while (0)
#else
#define bench_new(...) while (0)
#endif

#endif /* __DAV1D_TESTS_CHECKASM_CHECKASM_H */
