/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscrashreport.h"

#include <time.h>

#include "jsapi.h"
#include "jscrashformat.h"
#include "jsutil.h"

using namespace js;
using namespace js::crash;

#if defined(XP_WIN)

static const int stack_snapshot_max_size = 32768;

#include <windows.h>

static bool
GetStack(uint64_t *stack, uint64_t *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    /* Try to figure out how big the stack is. */
    char dummy;
    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(&dummy), &info, sizeof(info)) == 0)
        return false;
    if (info.State != MEM_COMMIT)
        return false;

    /* 256 is a fudge factor to account for the rest of GetStack's frame. */
    uint64_t p = uint64_t(&dummy) - 256;
    uint64_t len = stack_snapshot_max_size;

    if (p + len > uint64_t(info.BaseAddress) + info.RegionSize)
        len = uint64_t(info.BaseAddress) + info.RegionSize - p;

    if (len > size)
        len = size;

    *stack = p;
    *stack_len = len;

    /* Get the register state. */
#if defined(_MSC_VER) && defined(_M_IX86)
    /* ASM version for win2k that doesn't support RtlCaptureContext */
    uint32_t vip, vsp, vbp;
    __asm {
    MyLabel:
        mov [vbp], ebp;
        mov [vsp], esp;
        mov eax, [MyLabel];
        mov [vip], eax;
    }
    regs->ip = vip;
    regs->sp = vsp;
    regs->bp = vbp;
#else
    CONTEXT context;
    RtlCaptureContext(&context);
#if defined(_M_IX86)
    regs->ip = context.Eip;
    regs->sp = context.Esp;
    regs->bp = context.Ebp;
#elif defined(_M_X64)
    regs->ip = context.Rip;
    regs->sp = context.Rsp;
    regs->bp = context.Rbp;
#else
#error unknown cpu architecture
#endif
#endif

    js_memcpy(buffer, (void *)p, len);

    return true;
}

#elif 0

#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

static bool
GetStack(uint64_t *stack, uint64_t *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    /* 256 is a fudge factor to account for the rest of GetStack's frame. */
    char dummy;
    uint64_t p = uint64_t(&dummy) - 256;
    uint64_t pgsz = getpagesize();
    uint64_t len = stack_snapshot_max_size;
    p &= ~(pgsz - 1);

    /* Try to figure out how big the stack is. */
    while (len > 0) {
	if (mlock((const void *)p, len) == 0) {
	    munlock((const void *)p, len);
	    break;
	}
	len -= pgsz;
    }

    if (len > size)
        len = size;

    *stack = p;
    *stack_len = len;

    /* Get the register state. */
    ucontext_t context;
    if (getcontext(&context) != 0)
	return false;

#if defined(__x86_64__)
    regs->sp = (uint64_t)context.uc_mcontext.gregs[REG_RSP];
    regs->bp = (uint64_t)context.uc_mcontext.gregs[REG_RBP];
    regs->ip = (uint64_t)context.uc_mcontext.gregs[REG_RIP];
#elif defined(__i386__)
    regs->sp = (uint64_t)context.uc_mcontext.gregs[REG_ESP];
    regs->bp = (uint64_t)context.uc_mcontext.gregs[REG_EBP];
    regs->ip = (uint64_t)context.uc_mcontext.gregs[REG_EIP];
#else
#error unknown cpu architecture
#endif

    js_memcpy(buffer, (void *)p, len);

    return true;
}

#else

static bool
GetStack(uint64_t *stack, uint64_t *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    return false;
}

#endif

namespace js {
namespace crash {

class Stack : private CrashStack
{
public:
    explicit Stack(uint64_t id);

    bool snapshot();
};

Stack::Stack(uint64_t id)
  : CrashStack(id)
{
}

bool
Stack::snapshot()
{
    snaptime = time(nullptr);
    return GetStack(&stack_base, &stack_len, &regs, stack, sizeof(stack));
}

class Ring : private CrashRing
{
public:
    explicit Ring(uint64_t id);

    void push(uint64_t tag, void *data, size_t size);

private:
    size_t bufferSize() { return crash_buffer_size; }
    void copyBytes(void *data, size_t size);
};

Ring::Ring(uint64_t id)
  : CrashRing(id)
{
}

void
Ring::push(uint64_t tag, void *data, size_t size)
{
    uint64_t t = time(nullptr);

    copyBytes(&tag, sizeof(uint64_t));
    copyBytes(&t, sizeof(uint64_t));
    copyBytes(data, size);
    uint64_t mysize = size;
    copyBytes(&mysize, sizeof(uint64_t));
}

void
Ring::copyBytes(void *data, size_t size)
{
    if (size >= bufferSize())
        size = bufferSize();

    if (offset + size > bufferSize()) {
        size_t first = bufferSize() - offset;
        size_t second = size - first;
        js_memcpy(&buffer[offset], data, first);
        js_memcpy(buffer, (char *)data + first, second);
        offset = second;
    } else {
        js_memcpy(&buffer[offset], data, size);
        offset += size;
    }
}

} /* namespace crash */
} /* namespace js */

#ifdef JS_CRASH_DIAGNOSTICS
static bool gInitialized;

static Stack gGCStack(JS_CRASH_STACK_GC);
static Stack gErrorStack(JS_CRASH_STACK_ERROR);
static Ring gRingBuffer(JS_CRASH_RING);
#endif

void
js::crash::SnapshotGCStack()
{
#ifdef JS_CRASH_DIAGNOSTICS
    if (gInitialized)
        gGCStack.snapshot();
#endif
}

void
js::crash::SnapshotErrorStack()
{
#ifdef JS_CRASH_DIAGNOSTICS
    if (gInitialized)
        gErrorStack.snapshot();
#endif
}

void
js::crash::SaveCrashData(uint64_t tag, void *ptr, size_t size)
{
#ifdef JS_CRASH_DIAGNOSTICS
    if (gInitialized)
        gRingBuffer.push(tag, ptr, size);
#endif
}

JS_PUBLIC_API(void)
JS_EnumerateDiagnosticMemoryRegions(JSEnumerateDiagnosticMemoryCallback callback)
{
#ifdef JS_CRASH_DIAGNOSTICS
    if (!gInitialized) {
        gInitialized = true;
        (*callback)(&gGCStack, sizeof(gGCStack));
        (*callback)(&gErrorStack, sizeof(gErrorStack));
        (*callback)(&gRingBuffer, sizeof(gRingBuffer));
    }
#endif
}

