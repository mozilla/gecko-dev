/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This source code was derived from Chromium code, and as such is also subject
 * to the [Chromium license](ipc/chromium/src/LICENSE). */

#include "mozilla/ipc/SharedMemory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef XP_LINUX
#  include "base/linux_memfd_defs.h"
#endif
#ifdef MOZ_WIDGET_GTK
#  include "mozilla/WidgetUtilsGtk.h"
#endif

#ifdef __FreeBSD__
#  include <sys/capsicum.h>
#endif

#ifdef MOZ_VALGRIND
#  include <valgrind/valgrind.h>
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfilerThreadSleep.h"
#include "mozilla/UniquePtrExtensions.h"
#include "prenv.h"
#include "nsXULAppAPI.h"  // for XRE_IsParentProcess

namespace mozilla::ipc {

void SharedMemory::ResetImpl() {
  if (mFrozenFile) {
    CHROMIUM_LOG(WARNING) << "freezable shared memory was never frozen";
    mFrozenFile = nullptr;
  }
  mIsMemfd = false;
};

SharedMemory::Handle SharedMemory::CloneHandle(const Handle& aHandle) {
  const int new_fd = dup(aHandle.get());
  if (new_fd < 0) {
    CHROMIUM_LOG(WARNING) << "failed to duplicate file descriptor: "
                          << strerror(errno);
    return nullptr;
  }
  return mozilla::UniqueFileHandle(new_fd);
}

void* SharedMemory::FindFreeAddressSpace(size_t size) {
  void* memory = mmap(nullptr, size, PROT_NONE,
                      MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE, -1, 0);
  if (memory == MAP_FAILED) {
    return nullptr;
  }
  munmap(memory, size);
  return memory;
}

Maybe<void*> SharedMemory::MapImpl(size_t nBytes, void* fixedAddress) {
  // Don't use MAP_FIXED when a fixed_address was specified, since that can
  // replace pages that are alread mapped at that address.
  void* mem =
      mmap(fixedAddress, nBytes, PROT_READ | (mReadOnly ? 0 : PROT_WRITE),
           MAP_SHARED, mHandle.get(), 0);

  if (mem == MAP_FAILED) {
    CHROMIUM_LOG(WARNING) << "Call to mmap failed: " << strerror(errno);
    return Nothing();
  }

  if (fixedAddress && mem != fixedAddress) {
    bool munmap_succeeded = munmap(mem, nBytes) == 0;
    DCHECK(munmap_succeeded) << "Call to munmap failed, errno=" << errno;
    return Nothing();
  }

  return Some(mem);
}

void SharedMemory::UnmapImpl(size_t nBytes, void* address) {
  munmap(address, nBytes);
}

// memfd_create is a nonstandard interface for creating anonymous
// shared memory accessible as a file descriptor but not tied to any
// filesystem.  It first appeared in Linux 3.17, and was adopted by
// FreeBSD in version 13.

#if !defined(HAVE_MEMFD_CREATE) && defined(XP_LINUX) && \
    defined(SYS_memfd_create)

// Older libc versions (e.g., glibc before 2.27) don't have the
// wrapper, but we can supply our own; see `linux_memfd_defs.h`.

static int memfd_create(const char* name, unsigned int flags) {
  return syscall(SYS_memfd_create, name, flags);
}

#  define HAVE_MEMFD_CREATE 1
#endif

// memfd supports having "seals" applied to the file, to prevent
// various types of changes (which apply to all fds referencing the
// file).  Unfortunately, we can't rely on F_SEAL_WRITE to implement
// Freeze(); see the comments in ReadOnlyCopy() below.
//
// Instead, to prevent a child process from regaining write access to
// a read-only copy, the OS must also provide a way to remove write
// permissions at the file descriptor level.  This next section
// attempts to accomplish that.

#ifdef HAVE_MEMFD_CREATE
#  ifdef XP_LINUX
#    define USE_MEMFD_CREATE 1

// To create a read-only duplicate of an fd, we can use procfs; the
// same operation could restore write access, but sandboxing prevents
// child processes from accessing /proc.
//
// (Note: if this ever changes to not use /proc, also reconsider how
// and if HaveMemfd should check whether this works.)

static int DupReadOnly(int fd) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  std::string path = StringPrintf("/proc/self/fd/%d", fd);
  // procfs opens probably won't EINTR, but checking for it can't hurt
  return HANDLE_EINTR(open(path.c_str(), O_RDONLY | O_CLOEXEC));
}

#  elif defined(__FreeBSD__)
#    define USE_MEMFD_CREATE 1

// FreeBSD's Capsicum framework allows irrevocably restricting the
// operations permitted on a file descriptor.

static int DupReadOnly(int fd) {
  int rofd = dup(fd);
  if (rofd < 0) {
    return -1;
  }

  cap_rights_t rights;
  cap_rights_init(&rights, CAP_FSTAT, CAP_MMAP_R);
  if (cap_rights_limit(rofd, &rights) < 0) {
    int err = errno;
    close(rofd);
    errno = err;
    return -1;
  }

  return rofd;
}

#  else  // unhandled OS
#    warning "OS has memfd_create but no DupReadOnly implementation"
#  endif  // OS selection
#endif    // HAVE_MEMFD_CREATE

// Runtime detection for memfd support.  Returns `Nothing()` if not
// supported, or `Some(flags)` if supported, where `flags` contains
// flags like `MFD_CLOEXEC` that should be passed to all calls.
static Maybe<unsigned> HaveMemfd() {
#ifdef USE_MEMFD_CREATE
  static const Maybe<unsigned> kHave = []() -> Maybe<unsigned> {
    unsigned flags = MFD_CLOEXEC | MFD_ALLOW_SEALING;
#  ifdef MFD_NOEXEC_SEAL
    flags |= MFD_NOEXEC_SEAL;
#  endif

    mozilla::UniqueFileHandle fd(memfd_create("mozilla-ipc-test", flags));

#  ifdef MFD_NOEXEC_SEAL
    if (!fd && errno == EINVAL) {
      flags &= ~MFD_NOEXEC_SEAL;
      fd.reset(memfd_create("mozilla-ipc-test", flags));
    }
#  endif

    if (!fd) {
      DCHECK_EQ(errno, ENOSYS);
      return Nothing();
    }

    // Verify that DupReadOnly works; on Linux it's known to fail if:
    //
    // * SELinux assigns the memfd a type for which this process's
    //   domain doesn't have "open" permission; this is always the
    //   case on Android but could occur on desktop as well
    //
    // * /proc (used by the DupReadOnly implementation) isn't mounted,
    //   which is a configuration that the Tor Browser project is
    //   interested in as a way to reduce fingerprinting risk
    //
    // Sandboxed processes on Linux also can't use it if sandboxing
    // has already been started, but that's expected.  It should be
    // safe for sandboxed child processes to use memfd even if an
    // unsandboxed process couldn't freeze them, because freezing
    // isn't allowed (or meaningful) for memory created by another
    // process.

    if (XRE_IsParentProcess()) {
      mozilla::UniqueFileHandle rofd(DupReadOnly(fd.get()));
      if (!rofd) {
        CHROMIUM_LOG(WARNING) << "read-only dup failed (" << strerror(errno)
                              << "); not using memfd";
        return Nothing();
      }
    }
    return Some(flags);
  }();
  return kHave;
#else
  return Nothing();
#endif  // USE_MEMFD_CREATE
}

// static
bool SharedMemory::AppendPosixShmPrefix(std::string* str, pid_t pid) {
  if (HaveMemfd()) {
    return false;
  }
  *str += '/';
#ifdef MOZ_WIDGET_GTK
  // The Snap package environment doesn't provide a private /dev/shm
  // (it's used for communication with services like PulseAudio);
  // instead AppArmor is used to restrict access to it.  Anything with
  // this prefix is allowed:
  if (const char* snap = mozilla::widget::GetSnapInstanceName()) {
    StringAppendF(str, "snap.%s.", snap);
  }
#endif  // XP_LINUX
  // Hopefully the "implementation defined" name length limit is long
  // enough for this.
  StringAppendF(str, "org.mozilla.ipc.%d.", static_cast<int>(pid));
  return true;
}

bool SharedMemory::CreateImpl(size_t size, bool freezable) {
  DCHECK(size > 0);
  DCHECK(!mHandle);
  DCHECK(!mFrozenFile);

  MOZ_DIAGNOSTIC_ASSERT(
      !freezable || XRE_IsParentProcess(),
      "Child processes may not create freezable shared memory");

  mozilla::UniqueFileHandle fd;
  mozilla::UniqueFileHandle frozen_fd;
  bool is_memfd = false;

#ifdef USE_MEMFD_CREATE
  if (auto flags = HaveMemfd()) {
    fd.reset(memfd_create("mozilla-ipc", *flags));
    if (!fd) {
      // In general it's too late to fall back here -- in a sandboxed
      // child process, shm_open is already blocked.  And it shouldn't
      // be necessary.
      CHROMIUM_LOG(WARNING) << "failed to create memfd: " << strerror(errno);
      return false;
    }
    is_memfd = true;
    if (freezable) {
      frozen_fd.reset(DupReadOnly(fd.get()));
      if (!frozen_fd) {
        CHROMIUM_LOG(WARNING)
            << "failed to create read-only memfd: " << strerror(errno);
        return false;
      }
    }
  }
#endif

  if (!fd) {
    // Generic Unix: shm_open + shm_unlink
    do {
      // The names don't need to be unique, but it saves time if they
      // usually are.
      static mozilla::Atomic<size_t> sNameCounter;
      std::string name;
      CHECK(AppendPosixShmPrefix(&name, getpid()));
      StringAppendF(&name, "%zu", sNameCounter++);
      // O_EXCL means the names being predictable shouldn't be a problem.
      fd.reset(HANDLE_EINTR(
          shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)));
      if (fd) {
        if (freezable) {
          frozen_fd.reset(HANDLE_EINTR(shm_open(name.c_str(), O_RDONLY, 0400)));
          if (!frozen_fd) {
            int open_err = errno;
            shm_unlink(name.c_str());
            DLOG(FATAL) << "failed to re-open freezable shm: "
                        << strerror(open_err);
            return false;
          }
        }
        if (shm_unlink(name.c_str()) != 0) {
          // This shouldn't happen, but if it does: assume the file is
          // in fact leaked, and bail out now while it's still 0-length.
          DLOG(FATAL) << "failed to unlink shm: " << strerror(errno);
          return false;
        }
      }
    } while (!fd && errno == EEXIST);
  }

  if (!fd) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }

  mozilla::Maybe<int> fallocateError;
#if defined(HAVE_POSIX_FALLOCATE)
  // Using posix_fallocate will ensure that there's actually space for this
  // file. Otherwise we end up with a sparse file that can give SIGBUS if we
  // run out of space while writing to it.  (This doesn't apply to memfd.)
  if (!is_memfd) {
    int rv;
    // Avoid repeated interruptions of posix_fallocate by the profiler's
    // SIGPROF sampling signal. Indicating "thread sleep" here means we'll
    // get up to one interruption but not more. See bug 1658847 for more.
    // This has to be scoped outside the HANDLE_RV_EINTR retry loop.
    {
      AUTO_PROFILER_THREAD_SLEEP;

      rv = HANDLE_RV_EINTR(
          posix_fallocate(fd.get(), 0, static_cast<off_t>(size)));
    }

    // Some filesystems have trouble with posix_fallocate. For now, we must
    // fallback ftruncate and accept the allocation failures like we do
    // without posix_fallocate.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1618914
    if (rv != 0 && rv != EOPNOTSUPP && rv != EINVAL && rv != ENODEV) {
      CHROMIUM_LOG(WARNING)
          << "fallocate failed to set shm size: " << strerror(rv);
      return false;
    }
    fallocateError = mozilla::Some(rv);
  }
#endif

  // If posix_fallocate isn't supported / relevant for this type of
  // file (either failed with an expected error, or wasn't attempted),
  // then set the size with ftruncate:
  if (fallocateError != mozilla::Some(0)) {
    int rv = HANDLE_EINTR(ftruncate(fd.get(), static_cast<off_t>(size)));
    if (rv != 0) {
      int ftruncate_errno = errno;
      if (fallocateError) {
        CHROMIUM_LOG(WARNING) << "fallocate failed to set shm size: "
                              << strerror(*fallocateError);
      }
      CHROMIUM_LOG(WARNING)
          << "ftruncate failed to set shm size: " << strerror(ftruncate_errno);
      return false;
    }
  }

  mHandle = std::move(fd);
  mFrozenFile = std::move(frozen_fd);
  mIsMemfd = is_memfd;
  return true;
}

Maybe<SharedMemory::Handle> SharedMemory::ReadOnlyCopyImpl() {
#ifdef USE_MEMFD_CREATE
#  ifdef MOZ_VALGRIND
  // Valgrind allows memfd_create but doesn't understand F_ADD_SEALS.
  static const bool haveSeals = RUNNING_ON_VALGRIND == 0;
#  else
  static const bool haveSeals = true;
#  endif
  static const bool useSeals = !PR_GetEnv("MOZ_SHM_NO_SEALS");
  if (mIsMemfd && haveSeals && useSeals) {
    // Seals are added to the file as defense-in-depth.  The primary
    // method of access control is creating a read-only fd (using
    // procfs in this case) and requiring that sandboxes processes not
    // have access to /proc/self/fd to regain write permission; this
    // is the same as with shm_open.
    //
    // Unfortunately, F_SEAL_WRITE is unreliable: if the process
    // forked while there was a writeable mapping, it will inherit a
    // copy of the mapping, which causes the seal to fail.
    //
    // (Also, in the future we may want to split this into separate
    // classes for mappings and shared memory handles, which would
    // complicate identifying the case where `F_SEAL_WRITE` would be
    // possible even in the absence of races with fork.)
    //
    // However, Linux 5.1 added F_SEAL_FUTURE_WRITE, which prevents
    // write operations afterwards, but existing writeable mappings
    // are unaffected (similar to ashmem protection semantics).

    const int seals = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
    int sealError = EINVAL;

#  ifdef F_SEAL_FUTURE_WRITE
    sealError =
        fcntl(mHandle.get(), F_ADD_SEALS, seals | F_SEAL_FUTURE_WRITE) == 0
            ? 0
            : errno;
#  endif  // F_SEAL_FUTURE_WRITE
    if (sealError == EINVAL) {
      sealError = fcntl(mHandle.get(), F_ADD_SEALS, seals) == 0 ? 0 : errno;
    }
    if (sealError != 0) {
      CHROMIUM_LOG(WARNING) << "failed to seal memfd: " << strerror(errno);
      return Nothing();
    }
  }
#else  // !USE_MEMFD_CREATE
  DCHECK(!mIsMemfd);
#endif

  DCHECK(mFrozenFile);
  DCHECK(mHandle);
  mozilla::UniqueFileHandle ro_file = std::move(mFrozenFile);

  DCHECK(ro_file);

  return Some(std::move(ro_file));
}

void SharedMemory::SystemProtect(char* aAddr, size_t aSize, int aRights) {
  if (!SystemProtectFallible(aAddr, aSize, aRights)) {
    MOZ_CRASH("can't mprotect()");
  }
}

bool SharedMemory::SystemProtectFallible(char* aAddr, size_t aSize,
                                         int aRights) {
  int flags = 0;
  if (aRights & RightsRead) flags |= PROT_READ;
  if (aRights & RightsWrite) flags |= PROT_WRITE;
  if (RightsNone == aRights) flags = PROT_NONE;

  return 0 == mprotect(aAddr, aSize, flags);
}

size_t SharedMemory::SystemPageSize() { return sysconf(_SC_PAGESIZE); }

bool SharedMemory::UsingPosixShm() { return !HaveMemfd(); }

}  // namespace mozilla::ipc
