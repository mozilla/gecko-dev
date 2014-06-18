/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProfilingStack_h
#define js_ProfilingStack_h

#include "mozilla/NullPtr.h"
#include "mozilla/TypedEnum.h"

#include "jsbytecode.h"
#include "jstypes.h"

#include "js/Utility.h"

struct JSRuntime;

namespace js {

// A call stack can be specified to the JS engine such that all JS entry/exits
// to functions push/pop an entry to/from the specified stack.
//
// For more detailed information, see vm/SPSProfiler.h.
//
class ProfileEntry
{
    // All fields are marked volatile to prevent the compiler from re-ordering
    // instructions. Namely this sequence:
    //
    //    entry[size] = ...;
    //    size++;
    //
    // If the size modification were somehow reordered before the stores, then
    // if a sample were taken it would be examining bogus information.
    //
    // A ProfileEntry represents both a C++ profile entry and a JS one.

    // Descriptive string of this entry.
    const char * volatile string;

    // Stack pointer for non-JS entries, the script pointer otherwise.
    void * volatile spOrScript;

    // Line number for non-JS entries, the bytecode offset otherwise.
    int32_t volatile lineOrPc;

    // General purpose storage describing this frame.
    uint32_t volatile flags_;

  public:
    // These traits are bit masks. Make sure they're powers of 2.
    enum Flags {
        // Indicate whether a profile entry represents a CPP frame. If not set,
        // a JS frame is assumed by default. You're not allowed to publicly
        // change the frame type. Instead, call `setJsFrame` or `setCppFrame`.
        IS_CPP_ENTRY = 0x01,

        // Indicate that copying the frame label is not necessary when taking a
        // sample of the pseudostack.
        FRAME_LABEL_COPY = 0x02,

        // Mask for removing all flags except the category information.
        CATEGORY_MASK = ~IS_CPP_ENTRY & ~FRAME_LABEL_COPY
    };

    MOZ_BEGIN_NESTED_ENUM_CLASS(Category, uint32_t)
        OTHER    = 0x04,
        CSS      = 0x08,
        JS       = 0x10,
        GC       = 0x20,
        CC       = 0x40,
        NETWORK  = 0x80,
        GRAPHICS = 0x100,
        STORAGE  = 0x200,
        EVENTS   = 0x400,

        FIRST    = OTHER,
        LAST     = EVENTS
    MOZ_END_NESTED_ENUM_CLASS(Category)

    // All of these methods are marked with the 'volatile' keyword because SPS's
    // representation of the stack is stored such that all ProfileEntry
    // instances are volatile. These methods would not be available unless they
    // were marked as volatile as well.

    bool isCpp() const volatile { return hasFlag(IS_CPP_ENTRY); }
    bool isJs() const volatile { return !isCpp(); }

    bool isCopyLabel() const volatile { return hasFlag(FRAME_LABEL_COPY); };

    void setLabel(const char *aString) volatile { string = aString; }
    const char *label() const volatile { return string; }

    void setJsFrame(JSScript *aScript, jsbytecode *aPc) volatile {
        flags_ = 0;
        spOrScript = aScript;
        setPC(aPc);
    }
    void setCppFrame(void *aSp, uint32_t aLine) volatile {
        flags_ = IS_CPP_ENTRY;
        spOrScript = aSp;
        lineOrPc = static_cast<int32_t>(aLine);
    }

    void setFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flags_ |= flag;
    }
    void unsetFlag(uint32_t flag) volatile {
        MOZ_ASSERT(flag != IS_CPP_ENTRY);
        flags_ &= ~flag;
    }
    bool hasFlag(uint32_t flag) const volatile {
        return bool(flags_ & flag);
    }

    uint32_t flags() const volatile {
        return flags_;
    }
    uint32_t category() const volatile {
        return flags_ & CATEGORY_MASK;
    }

    void *stackAddress() const volatile {
        MOZ_ASSERT(!isJs());
        return spOrScript;
    }
    JSScript *script() const volatile {
        MOZ_ASSERT(isJs());
        return (JSScript *)spOrScript;
    }
    uint32_t line() const volatile {
        MOZ_ASSERT(!isJs());
        return static_cast<uint32_t>(lineOrPc);
    }

    // We can't know the layout of JSScript, so look in vm/SPSProfiler.cpp.
    JS_FRIEND_API(jsbytecode *) pc() const volatile;
    JS_FRIEND_API(void) setPC(jsbytecode *pc) volatile;

    // The offset of a pc into a script's code can actually be 0, so to
    // signify a nullptr pc, use a -1 index. This is checked against in
    // pc() and setPC() to set/get the right pc.
    static const int32_t NullPCOffset = -1;

    static size_t offsetOfLabel() { return offsetof(ProfileEntry, string); }
    static size_t offsetOfSpOrScript() { return offsetof(ProfileEntry, spOrScript); }
    static size_t offsetOfLineOrPc() { return offsetof(ProfileEntry, lineOrPc); }
    static size_t offsetOfFlags() { return offsetof(ProfileEntry, flags_); }
};

JS_FRIEND_API(void)
SetRuntimeProfilingStack(JSRuntime *rt, ProfileEntry *stack, uint32_t *size,
                         uint32_t max);

JS_FRIEND_API(void)
EnableRuntimeProfilingStack(JSRuntime *rt, bool enabled);

JS_FRIEND_API(void)
RegisterRuntimeProfilingEventMarker(JSRuntime *rt, void (*fn)(const char *));

JS_FRIEND_API(jsbytecode*)
ProfilingGetPC(JSRuntime *rt, JSScript *script, void *ip);

} // namespace js

#endif  /* js_ProfilingStack_h */
