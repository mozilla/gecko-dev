/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Hierarchy of SpiderMonkey system memory allocators:
 *
 *   - System {m,c,re}alloc/new/free: Overridden by jemalloc in most
 *     environments. Do not use these functions directly.
 *
 *   - js_{m,c,re}alloc/new/free: Wraps the system allocators and adds a
 *     failure injection framework for use by the fuzzers as well as templated,
 *     typesafe variants. See js/public/Utility.h.
 *
 *   - AllocPolicy: An interface for the js allocators, for use with templates.
 *     These allocators are for system memory whose lifetime is not associated
 *     with a GC thing. See js/src/jsalloc.h.
 *
 *       - SystemAllocPolicy: No extra functionality over bare allocators.
 *
 *       - TempAllocPolicy: Adds automatic error reporting to the provided
 *         Context when allocations fail.
 *
 *       - ContextAllocPolicy: forwards to the JSContext MallocProvider.
 *
 *       - RuntimeAllocPolicy: forwards to the JSRuntime MallocProvider.
 *
 *   - MallocProvider. A mixin base class that handles automatically updating
 *     the GC's state in response to allocations that are tied to a GC lifetime
 *     or are for a particular GC purpose. These allocators must only be used
 *     for memory that will be freed when a GC thing is swept.
 *
 *       - gc::Zone:  Automatically triggers zone GC.
 *       - JSRuntime: Automatically triggers full GC.
 *       - ThreadsafeContext > ExclusiveContext > JSContext:
 *                    Dispatches directly to the runtime.
 */

#ifndef vm_MallocProvider_h
#define vm_MallocProvider_h

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/UniquePtr.h"

#include "js/Utility.h"

namespace js {

template<class Client>
struct MallocProvider
{
    template <class T>
    T* pod_malloc() {
        return pod_malloc<T>(1);
    }

    template <class T>
    T* pod_malloc(size_t numElems) {
        size_t bytes = numElems * sizeof(T);
        T* p = js_pod_malloc<T>(numElems);
        if (MOZ_LIKELY(p)) {
            client()->updateMallocCounter(bytes);
            return p;
        }
        if (numElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        p = (T*)client()->onOutOfMemory(AllocFunction::Malloc, bytes);
        if (p)
            client()->updateMallocCounter(bytes);
        return p;
    }

    template <class T, class U>
    T* pod_malloc_with_extra(size_t numExtra) {
        if (MOZ_UNLIKELY(numExtra & mozilla::tl::MulOverflowMask<sizeof(U)>::value)) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        size_t bytes = sizeof(T) + numExtra * sizeof(U);
        if (MOZ_UNLIKELY(bytes < sizeof(T))) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        T* p = reinterpret_cast<T*>(js_pod_malloc<uint8_t>(bytes));
        if (MOZ_LIKELY(p)) {
            client()->updateMallocCounter(bytes);
            return p;
        }
        p = (T*)client()->onOutOfMemory(AllocFunction::Malloc, bytes);
        if (p)
            client()->updateMallocCounter(bytes);
        return p;
    }

    template <class T>
    mozilla::UniquePtr<T[], JS::FreePolicy>
    make_pod_array(size_t numElems) {
        return mozilla::UniquePtr<T[], JS::FreePolicy>(pod_malloc<T>(numElems));
    }

    template <class T>
    T* pod_calloc() {
        return pod_calloc<T>(1);
    }

    template <class T>
    T* pod_calloc(size_t numElems) {
        size_t bytes = numElems * sizeof(T);
        T* p = js_pod_calloc<T>(numElems);
        if (MOZ_LIKELY(p)) {
            client()->updateMallocCounter(bytes);
            return p;
        }
        if (numElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        p = (T*)client()->onOutOfMemory(AllocFunction::Calloc, bytes);
        if (p)
            client()->updateMallocCounter(bytes);
        return p;
    }

    template <class T, class U>
    T* pod_calloc_with_extra(size_t numExtra) {
        if (MOZ_UNLIKELY(numExtra & mozilla::tl::MulOverflowMask<sizeof(U)>::value)) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        size_t bytes = sizeof(T) + numExtra * sizeof(U);
        if (MOZ_UNLIKELY(bytes < sizeof(T))) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        T* p = reinterpret_cast<T*>(js_pod_calloc<uint8_t>(bytes));
        if (p) {
            client()->updateMallocCounter(bytes);
            return p;
        }
        p = (T*)client()->onOutOfMemory(AllocFunction::Calloc, bytes);
        if (p)
            client()->updateMallocCounter(bytes);
        return p;
    }

    template <class T>
    mozilla::UniquePtr<T[], JS::FreePolicy>
    make_zeroed_pod_array(size_t numElems)
    {
        return mozilla::UniquePtr<T[], JS::FreePolicy>(pod_calloc<T>(numElems));
    }

    template <class T>
    T* pod_realloc(T* prior, size_t oldSize, size_t newSize) {
        T* p = js_pod_realloc(prior, oldSize, newSize);
        if (MOZ_LIKELY(p)) {
            // For compatibility we do not account for realloc that decreases
            // previously allocated memory.
            if (newSize > oldSize)
                client()->updateMallocCounter((newSize - oldSize) * sizeof(T));
            return p;
        }
        if (newSize & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
            client()->reportAllocationOverflow();
            return nullptr;
        }
        p = (T*)client()->onOutOfMemory(AllocFunction::Realloc, newSize * sizeof(T), prior);
        if (p && newSize > oldSize)
            client()->updateMallocCounter((newSize - oldSize) * sizeof(T));
        return p;
    }

    JS_DECLARE_NEW_METHODS(new_, pod_malloc<uint8_t>, MOZ_ALWAYS_INLINE)
    JS_DECLARE_MAKE_METHODS(make_unique, new_, MOZ_ALWAYS_INLINE)

  private:
    Client* client() { return static_cast<Client*>(this); }
};

} /* namespace js */

#endif /* vm_MallocProvider_h */
