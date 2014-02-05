/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm/Architecture-arm.h"

#ifndef JS_ARM_SIMULATOR
#include <elf.h>
#endif

#include <fcntl.h>
#include <unistd.h>

#include "jit/arm/Assembler-arm.h"

#if !(defined(ANDROID) || defined(MOZ_B2G)) && !defined(JS_ARM_SIMULATOR)
#define HWCAP_ARMv7 (1 << 29)
#include <asm/hwcap.h>
#else
#define HWCAP_VFP      (1<<0)
#define HWCAP_VFPv3    (1<<1)
#define HWCAP_VFPv3D16 (1<<2)
#define HWCAP_VFPv4    (1<<3)
#define HWCAP_IDIVA    (1<<4)
#define HWCAP_IDIVT    (1<<5)
#define HWCAP_NEON     (1<<6)
#define HWCAP_ARMv7    (1<<7)
#endif

namespace js {
namespace jit {

uint32_t GetARMFlags()
{
    static bool isSet = false;
    static uint32_t flags = 0;
    if (isSet)
        return flags;

    static const char *env = getenv("ARMHWCAP");

    if (env && env[0]) {
        if (strstr(env, "help")) {
            fflush(NULL);
            printf(
                   "\n"
                   "usage: ARMHWCAP=option,option,option,... where options can be:\n"
                   "\n"
                   "  armv7    \n"
                   "  vfp      \n"
                   "  neon     \n"
                   "  vfpv3    \n"
                   "  vfpv3d16 \n"
                   "  vfpv4    \n"
                   "  idiva    \n"
                   "  idivt    \n"
                   "\n"
                   );
            exit(0);
            /*NOTREACHED*/
        } else {
            // Canonicalize each token to have a leading and trailing space.
            const char *start = env;  // Token start.
            for (;;) {
                char  ch = *start;
                if (!ch) {
                    // End of string.
                    break;
                }
                if (ch == ' ' || ch == ',') {
                    // Skip separator characters.
                    start++;
                    continue;
                }
                // Find the end of the token.
                const char *end = start + 1;
                for (; ; end++) {
                    ch = *end;
                    if (!ch || ch == ' ' || ch == ',')
                        break;
                }
                size_t count = end - start;
                if (count == 3 && strncmp(start, "vfp", 3) == 0)
                    flags |= HWCAP_VFP;
                else if (count == 5 && strncmp(start, "vfpv3", 5) == 0)
                    flags |= HWCAP_VFPv3;
                else if (count == 8 && strncmp(start, "vfpv3d16", 8) == 0)
                    flags |= HWCAP_VFPv3D16;
                else if (count == 5 && strncmp(start, "vfpv4", 5) == 0)
                    flags |= HWCAP_VFPv4;
                else if (count == 5 && strncmp(start, "idiva", 5) == 0)
                    flags |= HWCAP_IDIVA;
                else if (count == 5 && strncmp(start, "idivt", 5) == 0)
                    flags |= HWCAP_IDIVT;
                else if (count == 4 && strncmp(start, "neon", 4) == 0)
                    flags |= HWCAP_NEON;
                else if (count == 5 && strncmp(start, "armv7", 5) == 0)
                    flags |= HWCAP_ARMv7;
                else
                    fprintf(stderr, "Warning: unexpected ARMHWCAP flag at: %s\n", start);
                start = end;
            }
#ifdef DEBUG
            IonSpew(IonSpew_Codegen, "ARMHWCAP: '%s'\n   flags: 0x%x\n", env, flags);
#endif
            isSet = true;
            return flags;
        }
    }

#ifdef JS_ARM_SIMULATOR
    isSet = true;
    flags = HWCAP_ARMv7 | HWCAP_VFP | HWCAP_VFPv4 | HWCAP_NEON;
    return flags;
#else

#if WTF_OS_LINUX
    int fd = open("/proc/self/auxv", O_RDONLY);
    if (fd > 0) {
        Elf32_auxv_t aux;
        while (read(fd, &aux, sizeof(Elf32_auxv_t))) {
            if (aux.a_type == AT_HWCAP) {
                close(fd);
                flags = aux.a_un.a_val;
                isSet = true;
#if defined(__ARM_ARCH_7__) || defined (__ARM_ARCH_7A__)
                // this should really be detected at runtime, but
                // /proc/*/auxv doesn't seem to carry the ISA
                // I could look in /proc/cpuinfo as well, but
                // the chances that it will be different from this
                // are low.
                flags |= HWCAP_ARMv7;
#endif
                return flags;
            }
        }
        close(fd);
    }

#if defined(__ARM_ARCH_7__) || defined (__ARM_ARCH_7A__)
    flags = HWCAP_ARMv7;
#endif
    isSet = true;
    return flags;

#elif defined(WTF_OS_ANDROID) || defined(MOZ_B2G)
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp)
        return false;

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    size_t len = fread(buf, sizeof(char), sizeof(buf) - 2, fp);
    fclose(fp);
    // Canonicalize each token to have a leading and trailing space.
    buf[len] = ' ';
    buf[len + 1] = '\0';
    for (size_t i = 0; i < len; i++) {
        char  ch = buf[i];
        if (!ch)
            break;
        else if (ch == '\n')
            buf[i] = 0x20;
        else
            buf[i] = ch;
    }

    if (strstr(buf, " vfp "))
        flags |= HWCAP_VFP;

    if (strstr(buf, " vfpv3 "))
        flags |= HWCAP_VFPv3;

    if (strstr(buf, " vfpv3d16 "))
        flags |= HWCAP_VFPv3D16;

    if (strstr(buf, " vfpv4 "))
        flags |= HWCAP_VFPv4;

    if (strstr(buf, " idiva "))
        flags |= HWCAP_IDIVA;

    if (strstr(buf, " idivt "))
        flags |= HWCAP_IDIVT;

    if (strstr(buf, " neon "))
        flags |= HWCAP_NEON;

    // not part of the HWCAP flag, but I need to know this, and we're not using
    //  that bit, so... I'm using it
    if (strstr(buf, "ARMv7"))
        flags |= HWCAP_ARMv7;

#ifdef DEBUG
    IonSpew(IonSpew_Codegen, "ARMHWCAP: '%s'\n   flags: 0x%x\n", buf, flags);
#endif

    isSet = true;
    return flags;
#endif

    return 0;
#endif // JS_ARM_SIMULATOR
}

bool hasMOVWT()
{
    return GetARMFlags() & HWCAP_ARMv7;
}
bool hasVFPv3()
{
    return GetARMFlags() & HWCAP_VFPv3;
}
bool hasVFP()
{
    return GetARMFlags() & HWCAP_VFP;
}

bool has32DP()
{
    return !(GetARMFlags() & HWCAP_VFPv3D16 && !(GetARMFlags() & HWCAP_NEON));
}
bool useConvReg()
{
    return has32DP();
}

bool hasIDIV()
{
#if defined HWCAP_IDIVA
    return GetARMFlags() & HWCAP_IDIVA;
#else
    return false;
#endif
}

Registers::Code
Registers::FromName(const char *name)
{
    // Check for some register aliases first.
    if (strcmp(name, "ip") == 0)
        return ip;
    if (strcmp(name, "r13") == 0)
        return r13;
    if (strcmp(name, "lr") == 0)
        return lr;
    if (strcmp(name, "r15") == 0)
        return r15;

    for (size_t i = 0; i < Total; i++) {
        if (strcmp(GetName(i), name) == 0)
            return Code(i);
    }

    return Invalid;
}

FloatRegisters::Code
FloatRegisters::FromName(const char *name)
{
    for (size_t i = 0; i < Total; i++) {
        if (strcmp(GetName(i), name) == 0)
            return Code(i);
    }

    return Invalid;
}

} // namespace jit
} // namespace js
