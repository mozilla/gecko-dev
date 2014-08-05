/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "jit/ExecutableAllocator.h"

#if ENABLE_ASSEMBLER && WTF_OS_UNIX && !WTF_OS_SYMBIAN

#include "mozilla/DebugOnly.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include <sys/mman.h>
#include <unistd.h>

#include "js/Utility.h"

namespace JSC {

size_t ExecutableAllocator::determinePageSize()
{
    return getpagesize();
}

ExecutablePool::Allocation ExecutableAllocator::systemAlloc(size_t n)
{
    void *allocation = MozTaggedAnonymousMmap(NULL, n, INITIAL_PROTECTION_FLAGS, MAP_PRIVATE | MAP_ANON, -1, 0, "js-jit-code");
    if (allocation == MAP_FAILED)
        allocation = NULL;
    ExecutablePool::Allocation alloc = { reinterpret_cast<char*>(allocation), n };
    return alloc;
}

void ExecutableAllocator::systemRelease(const ExecutablePool::Allocation& alloc)
{
    mozilla::DebugOnly<int> result = munmap(alloc.pages, alloc.size);
    MOZ_ASSERT(!result);
}

#if WTF_ENABLE_ASSEMBLER_WX_EXCLUSIVE
void ExecutableAllocator::reprotectRegion(void* start, size_t size, ProtectionSetting setting)
{
    if (!pageSize)
        intializePageSize();

    // Calculate the start of the page containing this region,
    // and account for this extra memory within size.
    intptr_t startPtr = reinterpret_cast<intptr_t>(start);
    intptr_t pageStartPtr = startPtr & ~(pageSize - 1);
    void* pageStart = reinterpret_cast<void*>(pageStartPtr);
    size += (startPtr - pageStartPtr);

    // Round size up
    size += (pageSize - 1);
    size &= ~(pageSize - 1);

    mprotect(pageStart, size, (setting == Writable) ? PROTECTION_FLAGS_RW : PROTECTION_FLAGS_RX);
}
#endif

#if WTF_CPU_ARM_TRADITIONAL && WTF_OS_LINUX && WTF_COMPILER_RVCT
__asm void ExecutableAllocator::cacheFlush(void* code, size_t size)
{
    ARM
    push {r7}
    add r1, r1, r0
    mov r7, #0xf0000
    add r7, r7, #0x2
    mov r2, #0x0
    svc #0x0
    pop {r7}
    bx lr
}
#endif

void
ExecutablePool::toggleAllCodeAsAccessible(bool accessible)
{
    char* begin = m_allocation.pages;
    size_t size = m_freePtr - begin;

    if (size) {
        // N.B. Some systems, like 32bit Mac OS 10.6, implicitly add PROT_EXEC
        // when mprotect'ing memory with any flag other than PROT_NONE. Be
        // sure to use PROT_NONE when making inaccessible.
        int flags = accessible ? PROT_READ | PROT_WRITE | PROT_EXEC : PROT_NONE;
        if (mprotect(begin, size, flags))
            MOZ_CRASH();
    }
}

}

#endif // HAVE(ASSEMBLER)
