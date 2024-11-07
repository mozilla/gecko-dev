/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <atomic>
#include <array>
#include <errno.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

#include "InterposerHelper.h"

using mozilla::DebugOnly;

#if defined(MOZ_ENABLE_FORKSERVER) && !defined(MOZ_TSAN)
#  include "mozilla/pthread_atfork.h"

static constexpr const int maxHandlers = 32;
static constexpr const int idxPreFork = 0;
static constexpr const int idxPostForkParent = 1;
static constexpr const int idxPostForkChild = 2;

struct moz_pthread_atfork_handler {
  using fn_ptr = std::atomic<void (*)(void)>;
  using dso_handle = std::atomic<void*>;
  using pthread_handlers = std::array<fn_ptr, 3>;

  std::atomic<int> usedElems = 0;
  std::array<pthread_handlers, maxHandlers> handlers = {};
  std::array<dso_handle, maxHandlers> dsos = {};

  bool add(void (*aPrefork)(void), void (*aParent)(void), void (*aChild)(void),
           const void* const aHandle) {
    if (usedElems == maxHandlers) {
      return false;
    }

    int elem = 0;
    for (elem = 0; elem < maxHandlers; ++elem) {
      if (dsos[elem] == nullptr) {
        handlers[elem][idxPreFork] = aPrefork;
        handlers[elem][idxPostForkParent] = aParent;
        handlers[elem][idxPostForkChild] = aChild;
        dsos[elem] = (void*)(aHandle);
        ++usedElems;
        break;
      }
    }

    return true;
  }

  bool remove(void* aHandle) {
    int elem = 0;
    for (elem = 0; elem < maxHandlers; ++elem) {
      if (dsos[elem] == aHandle) {
        handlers[elem][idxPreFork] = nullptr;
        handlers[elem][idxPostForkParent] = nullptr;
        handlers[elem][idxPostForkChild] = nullptr;
        dsos[elem] = nullptr;
        --usedElems;
      }
    }

    return true;
  }
};

struct moz_pthread_atfork_handler mozPthreadHandlers;

#  if defined(LIBC_GLIBC)
// On glibc the pthread_atfork may be available only from libc_nonshared.a
// so prefer interposing the linker-resolved __register_atfork()

extern const void* const __dso_handle;
using register_atfork_t = int (*)(void (*)(), void (*)(), void (*)(),
                                  const void* const);
static register_atfork_t real_register_atfork = nullptr;
#  else
using pthread_atfork_t = int (*)(void (*)(), void (*)(), void (*)());
static pthread_atfork_t real_pthread_atfork = nullptr;
#  endif

static int notReadyCount = 0;

extern "C" {

#  if defined(LIBC_GLIBC)
MFBT_API int __register_atfork(void (*aPrefork)(void),
                               void (*aPostForkParent)(void),
                               void (*aPostForkChild)(void),
                               const void* const dso_handle)
#  else
MFBT_API int pthread_atfork(void (*aPrefork)(void),
                            void (*aPostForkParent)(void),
                            void (*aPostForkChild)(void))
#  endif
{
#  if defined(LIBC_GLIBC)
  MOZ_ASSERT(real_register_atfork != __register_atfork,
             "Found __register_atfork from libc");
#  else
  MOZ_ASSERT(real_pthread_atfork != pthread_atfork,
             "Found pthread_atfork from libc");
#  endif

  int rv = 0;
#  if defined(LIBC_GLIBC)
  if (real_register_atfork) {
    real_register_atfork(aPrefork, aPostForkParent, aPostForkChild, dso_handle);
#  else
  if (real_pthread_atfork) {
    real_pthread_atfork(aPrefork, aPostForkParent, aPostForkChild);
#  endif
    MOZ_ASSERT(rv == 0, "call to real_register_atfork() failed");
    if (rv != 0) {
      return rv;
    }
  } else {
    ++notReadyCount;
  }

  rv = mozPthreadHandlers.add(aPrefork, aPostForkParent, aPostForkChild
#  if defined(LIBC_GLIBC)
                              ,
                              dso_handle
#  else
                              ,
                              (void*)(1)
#  endif
                              )
           ? 0
           : 1;
  MOZ_ASSERT(rv == 0,
#  if defined(LIBC_GLIBC)
             "Should have been able to add to __register_atfork() handlers"
#  else
             "Should have been able to add to pthread_atfork() handlers"
#  endif
  );

  if (rv > 0) {
    rv = ENOMEM;
  }

  return rv;
}

#  if defined(LIBC_GLIBC)
MFBT_API void __cxa_finalize(void* handle) {
  static const auto real_cxa_finalize = GET_REAL_SYMBOL(__cxa_finalize);
  real_cxa_finalize(handle);
  mozPthreadHandlers.remove(handle);
}
#  endif
}

#  if defined(LIBC_GLIBC)
__attribute__((used)) __attribute__((constructor)) void register_atfork_setup(
    void) {
  real_register_atfork = GET_REAL_SYMBOL(__register_atfork);

  if (notReadyCount > 0) {
    for (int i = 0; i < notReadyCount; ++i) {
      real_register_atfork(mozPthreadHandlers.handlers[i][idxPreFork],
                           mozPthreadHandlers.handlers[i][idxPostForkParent],
                           mozPthreadHandlers.handlers[i][idxPostForkChild],
                           __dso_handle);
    }
  }
}
#  else
__attribute__((used)) __attribute__((constructor)) void pthread_atfork_setup(
    void) {
  real_pthread_atfork = GET_REAL_SYMBOL(pthread_atfork);

  if (notReadyCount > 0) {
    for (int i = 0; i < notReadyCount; ++i) {
      real_pthread_atfork(mozPthreadHandlers.handlers[i][idxPreFork],
                          mozPthreadHandlers.handlers[i][idxPostForkParent],
                          mozPthreadHandlers.handlers[i][idxPostForkChild]);
    }
  }
}
#  endif

void run_moz_pthread_atfork_handlers(struct moz_pthread_atfork_handler* list,
                                     int handlerIdx, bool reverse) {
  MOZ_ASSERT(list, "moz_pthread_atfork_handler should not be nullptr");
  for (int i = (reverse ? maxHandlers - 1 : 0);
       (reverse ? (i >= 0) : (i < maxHandlers)); (reverse ? --i : ++i)) {
    if (list->dsos[i]) {
      if (list->handlers[i][handlerIdx]) {
        (*list->handlers[i][handlerIdx])();
      }
    }
  }
}

void run_moz_pthread_atfork_handlers_prefork() {
  run_moz_pthread_atfork_handlers(&mozPthreadHandlers, idxPreFork, true);
}

void run_moz_pthread_atfork_handlers_postfork_parent() {
  run_moz_pthread_atfork_handlers(&mozPthreadHandlers, idxPostForkParent,
                                  false);
}

void run_moz_pthread_atfork_handlers_postfork_child() {
  run_moz_pthread_atfork_handlers(&mozPthreadHandlers, idxPostForkChild, false);
}
#endif  // defined(MOZ_ENABLE_FORKSERVER)
