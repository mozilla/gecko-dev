/*
 * Copyright 2014 ARM Ltd.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkUtils.h"
#include "SkUtilsArm.h"

#if defined(SK_CPU_LENDIAN) && !SK_ARM_NEON_IS_NONE
extern "C" void memset16_neon(uint16_t dst[], uint16_t value, int count);
extern "C" void memset32_neon(uint32_t dst[], uint32_t value, int count);
#endif

#if defined(SK_CPU_LENDIAN)
extern "C" void arm_memset16(uint16_t* dst, uint16_t value, int count);
extern "C" void arm_memset32(uint32_t* dst, uint32_t value, int count);
#endif

SkMemset16Proc SkMemset16GetPlatformProc() {
    // FIXME: memset.arm.S is using syntax incompatible with XCode
#if !defined(SK_CPU_LENDIAN) || defined(SK_BUILD_FOR_IOS)
    return NULL;
#elif SK_ARM_NEON_IS_DYNAMIC
    if (sk_cpu_arm_has_neon()) {
        return memset16_neon;
    } else {
        return arm_memset16;
    }
#elif SK_ARM_NEON_IS_ALWAYS
    return memset16_neon;
#else
    return arm_memset16;
#endif
}

SkMemset32Proc SkMemset32GetPlatformProc() {
    // FIXME: memset.arm.S is using syntax incompatible with XCode
#if !defined(SK_CPU_LENDIAN) || defined(SK_BUILD_FOR_IOS)
    return NULL;
#elif SK_ARM_NEON_IS_DYNAMIC
    if (sk_cpu_arm_has_neon()) {
        return memset32_neon;
    } else {
        return arm_memset32;
    }
#elif SK_ARM_NEON_IS_ALWAYS
    return memset32_neon;
#else
    return arm_memset32;
#endif
}

SkMemcpy32Proc SkMemcpy32GetPlatformProc() {
    return NULL;
}
