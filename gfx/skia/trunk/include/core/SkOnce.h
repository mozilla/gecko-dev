/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkOnce_DEFINED
#define SkOnce_DEFINED

// Before trying SkOnce, see if SkLazyPtr or SkLazyFnPtr will work for you.
// They're smaller and faster, if slightly less versatile.


// SkOnce.h defines SK_DECLARE_STATIC_ONCE and SkOnce(), which you can use
// together to create a threadsafe way to call a function just once.  E.g.
//
// static void register_my_stuff(GlobalRegistry* registry) {
//     registry->register(...);
// }
// ...
// void EnsureRegistered() {
//     SK_DECLARE_STATIC_ONCE(once);
//     SkOnce(&once, register_my_stuff, GetGlobalRegistry());
// }
//
// No matter how many times you call EnsureRegistered(), register_my_stuff will be called just once.
// OnceTest.cpp also should serve as a few other simple examples.

#include "SkDynamicAnnotations.h"
#include "SkThread.h"
#include "SkTypes.h"

// This must be used in a global or function scope, not as a class member.
#define SK_DECLARE_STATIC_ONCE(name) static SkOnceFlag name

class SkOnceFlag;

inline void SkOnce(SkOnceFlag* once, void (*f)());

template <typename Arg>
inline void SkOnce(SkOnceFlag* once, void (*f)(Arg), Arg arg);

// If you've already got a lock and a flag to use, this variant lets you avoid an extra SkOnceFlag.
template <typename Lock>
inline void SkOnce(bool* done, Lock* lock, void (*f)());

template <typename Lock, typename Arg>
inline void SkOnce(bool* done, Lock* lock, void (*f)(Arg), Arg arg);

//  ----------------------  Implementation details below here. -----------------------------

// This class has no constructor and must be zero-initialized (the macro above does this).
class SkOnceFlag {
public:
    bool* mutableDone() { return &fDone; }

    void acquire() {
        // To act as a mutex, this needs an acquire barrier on success.
        // sk_atomic_cas doesn't guarantee this ...
        while (!sk_atomic_cas(&fSpinlock, 0, 1)) {
            // spin
        }
        // ... so make sure to issue one of our own.
        SkAssertResult(sk_acquire_load(&fSpinlock));
    }

    void release() {
        // To act as a mutex, this needs a release barrier.  sk_atomic_cas guarantees this.
        SkAssertResult(sk_atomic_cas(&fSpinlock, 1, 0));
    }

private:
    bool fDone;
    int32_t fSpinlock;
};

// We've pulled a pretty standard double-checked locking implementation apart
// into its main fast path and a slow path that's called when we suspect the
// one-time code hasn't run yet.

// This is the guts of the code, called when we suspect the one-time code hasn't been run yet.
// This should be rarely called, so we separate it from SkOnce and don't mark it as inline.
// (We don't mind if this is an actual function call, but odds are it'll be inlined anyway.)
template <typename Lock, typename Arg>
static void sk_once_slow(bool* done, Lock* lock, void (*f)(Arg), Arg arg) {
    lock->acquire();
    if (!*done) {
        f(arg);
        // Also known as a store-store/load-store barrier, this makes sure that the writes
        // done before here---in particular, those done by calling f(arg)---are observable
        // before the writes after the line, *done = true.
        //
        // In version control terms this is like saying, "check in the work up
        // to and including f(arg), then check in *done=true as a subsequent change".
        //
        // We'll use this in the fast path to make sure f(arg)'s effects are
        // observable whenever we observe *done == true.
        sk_release_store(done, true);
    }
    lock->release();
}

// This is our fast path, called all the time.  We do really want it to be inlined.
template <typename Lock, typename Arg>
inline void SkOnce(bool* done, Lock* lock, void (*f)(Arg), Arg arg) {
    if (!SK_ANNOTATE_UNPROTECTED_READ(*done)) {
        sk_once_slow(done, lock, f, arg);
    }
    // Also known as a load-load/load-store barrier, this acquire barrier makes
    // sure that anything we read from memory---in particular, memory written by
    // calling f(arg)---is at least as current as the value we read from done.
    //
    // In version control terms, this is a lot like saying "sync up to the
    // commit where we wrote done = true".
    //
    // The release barrier in sk_once_slow guaranteed that done = true
    // happens after f(arg), so by syncing to done = true here we're
    // forcing ourselves to also wait until the effects of f(arg) are readble.
    SkAssertResult(sk_acquire_load(done));
}

template <typename Arg>
inline void SkOnce(SkOnceFlag* once, void (*f)(Arg), Arg arg) {
    return SkOnce(once->mutableDone(), once, f, arg);
}

// Calls its argument.
// This lets us use functions that take no arguments with SkOnce methods above.
// (We pass _this_ as the function and the no-arg function as its argument.  Cute eh?)
static void sk_once_no_arg_adaptor(void (*f)()) {
    f();
}

inline void SkOnce(SkOnceFlag* once, void (*func)()) {
    return SkOnce(once, sk_once_no_arg_adaptor, func);
}

template <typename Lock>
inline void SkOnce(bool* done, Lock* lock, void (*func)()) {
    return SkOnce(done, lock, sk_once_no_arg_adaptor, func);
}

#endif  // SkOnce_DEFINED
