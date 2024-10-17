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

static constexpr const int maxHandlers = 16;
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
                                  const void* const) noexcept;
static register_atfork_t real_register_atfork = nullptr;

using cxa_finalize_t = void (*)(void*) noexcept;
static cxa_finalize_t real_cxa_finalize = nullptr;
#  else
using pthread_atfork_t = int (*)(void (*)(), void (*)(), void (*)()) noexcept;
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
  real_cxa_finalize(handle);
  mozPthreadHandlers.remove(handle);
}
#  endif
}

#  if defined(LIBC_GLIBC)
__attribute__((used)) __attribute__((constructor)) void register_atfork_setup(
    void) {
  const char* glibc_version_register =
#    if defined(__x86_64__) || defined(__i386__)
      "GLIBC_2.3.2"
#    elif defined(__aarch64__)
      "GLIBC_2.17"
#    else
#      error \
          "Missing GLIBC version for __register_atfork(). Please objdump -tTC libc.so.6 and add"
#    endif
      ;

  // Use dlvsym() otherwise symbol resolution may find the interposed version
  real_register_atfork = (register_atfork_t)dlvsym(nullptr, "__register_atfork",
                                                   glibc_version_register);

  MOZ_ASSERT(real_register_atfork != nullptr, "Found real_register_atfork");
  MOZ_ASSERT(real_register_atfork != __register_atfork,
             "Found register_atfork from libc");

  const char* glibc_version_finalize =
#    if defined(__i386__)
      "GLIBC_2.1.3"
#    elif defined(__x86_64__)
      "GLIBC_2.2.5"
#    elif defined(__aarch64__)
      "GLIBC_2.17"
#    else
#      error \
          "Missing GLIBC version for __cxa_finalize(). Please objdump -tTC libc.so.6 and add"
#    endif
      ;
  // Use dlvsym() otherwise symbol resolution may find the interposed version
  real_cxa_finalize =
      (cxa_finalize_t)dlvsym(nullptr, "__cxa_finalize", glibc_version_finalize);

  MOZ_ASSERT(real_cxa_finalize != nullptr, "Found real_cxa_finalize");
  MOZ_ASSERT(real_cxa_finalize != __cxa_finalize,
             "Found cxa_finalize from libc");

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
  real_pthread_atfork = (pthread_atfork_t)dlsym(RTLD_NEXT, "pthread_atfork");

  MOZ_ASSERT(real_pthread_atfork != nullptr, "Found real_pthread_atfork");
  MOZ_ASSERT(real_pthread_atfork != pthread_atfork,
             "Found pthread_atfork from libc");

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
