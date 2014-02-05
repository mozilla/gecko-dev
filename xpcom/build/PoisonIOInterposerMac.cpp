/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PoisonIOInterposer.h"
#include "mach_override.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Mutex.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/Scoped.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"
#include "nsStackWalk.h"
#include "nsTraceRefcntImpl.h"
#include "plstr.h"
#include "prio.h"

#include <algorithm>
#include <vector>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <aio.h>
#include <dlfcn.h>
#include <fcntl.h>

namespace {

using namespace mozilla;

// Bit tracking if poisoned writes are enabled
static bool sIsEnabled = false;

// Check if writes are dirty before reporting IO
static bool sOnlyReportDirtyWrites = false;

// Routines for write validation
bool IsValidWrite(int fd, const void *wbuf, size_t count);
bool IsIPCWrite(int fd, const struct stat &buf);

/******************************** IO AutoTimer ********************************/

/**
 * RAII class for timing the duration of an I/O call and reporting the result
 * to the IOInterposeObserver API.
 */
class MacIOAutoObservation : public IOInterposeObserver::Observation
{
public:
  MacIOAutoObservation(IOInterposeObserver::Operation aOp, int aFd)
    : IOInterposeObserver::Observation(aOp, sReference, sIsEnabled &&
                                       !IsDebugFile(aFd))
    , mFd(aFd)
    , mHasQueriedFilename(false)
    , mFilename(nullptr)
  {
  }

  MacIOAutoObservation(IOInterposeObserver::Operation aOp, int aFd,
                       const void *aBuf, size_t aCount)
    : IOInterposeObserver::Observation(aOp, sReference, sIsEnabled &&
                                       !IsDebugFile(aFd) &&
                                       IsValidWrite(aFd, aBuf, aCount))
    , mFd(aFd)
    , mHasQueriedFilename(false)
    , mFilename(nullptr)
  {
  }

  // Custom implementation of IOInterposeObserver::Observation::Filename
  const char16_t* Filename() MOZ_OVERRIDE;

  ~MacIOAutoObservation()
  {
    Report();
    if (mFilename) {
      NS_Free(mFilename);
      mFilename = nullptr;
    }
  }

private:
  int                 mFd;
  bool                mHasQueriedFilename;
  char16_t*           mFilename;
  static const char*  sReference;
};

const char* MacIOAutoObservation::sReference = "PoisonIOInterposer";

// Get filename for this observation
const char16_t* MacIOAutoObservation::Filename()
{
  // If mHasQueriedFilename is true, then we already have it
  if (mHasQueriedFilename) {
    return mFilename;
  }
  char filename[MAXPATHLEN];
  if (fcntl(mFd, F_GETPATH, filename) != -1) {
    mFilename = UTF8ToNewUnicode(nsDependentCString(filename));
  } else {
    mFilename = nullptr;
  }
  mHasQueriedFilename = true;

  // Return filename
  return mFilename;
}

/****************************** Write Validation ******************************/

// We want to detect "actual" writes, not IPC. Some IPC mechanisms are
// implemented with file descriptors, so filter them out.
bool IsIPCWrite(int fd, const struct stat &buf) {
  if ((buf.st_mode & S_IFMT) == S_IFIFO) {
    return true;
  }

  if ((buf.st_mode & S_IFMT) != S_IFSOCK) {
    return false;
  }

  sockaddr_storage address;
  socklen_t len = sizeof(address);
  if (getsockname(fd, (sockaddr*) &address, &len) != 0) {
    return true; // Ignore the fd if we can't find what it is.
  }

  return address.ss_family == AF_UNIX;
}

// We want to report actual disk IO not things that don't move bits on the disk
bool IsValidWrite(int fd, const void *wbuf, size_t count)
{
  // Ignore writes of zero bytes, Firefox does some during shutdown.
  if (count == 0) {
    return false;
  }

  {
    struct stat buf;
    int rv = fstat(fd, &buf);
    if (rv != 0) {
      return true;
    }

    if (IsIPCWrite(fd, buf)) {
      return false;
    }
  }

  // For writev we pass a nullptr wbuf. We should only get here from
  // dbm, and it uses write, so assert that we have wbuf.
  if (!wbuf) {
    return true;
  }

  // Break, here if we're allowed to report non-dirty writes
  if(!sOnlyReportDirtyWrites) {
    return true;
  }

  // As a really bad hack, accept writes that don't change the on disk
  // content. This is needed because dbm doesn't keep track of dirty bits
  // and can end up writing the same data to disk twice. Once when the
  // user (nss) asks it to sync and once when closing the database.
  ScopedFreePtr<void> wbuf2(malloc(count));
  if (!wbuf2) {
    return true;
  }
  off_t pos = lseek(fd, 0, SEEK_CUR);
  if (pos == -1) {
    return true;
  }
  ssize_t r = read(fd, wbuf2, count);
  if (r < 0 || (size_t)r != count) {
    return true;
  }
  int cmp = memcmp(wbuf, wbuf2, count);
  if (cmp != 0) {
    return true;
  }
  off_t pos2 = lseek(fd, pos, SEEK_SET);
  if (pos2 != pos) {
    return true;
  }

  // Otherwise this is not a valid write
  return false;
}

/*************************** Function Interception  ***************************/

/** Structure for declaration of function override */
struct FuncData {
  const char *Name;      // Name of the function for the ones we use dlsym
  const void *Wrapper;   // The function that we will replace 'Function' with
  void *Function;        // The function that will be replaced with 'Wrapper'
  void *Buffer;          // Will point to the jump buffer that lets us call
                         // 'Function' after it has been replaced.
};

// Wrap aio_write. We have not seen it before, so just assert/report it.
typedef ssize_t (*aio_write_t)(struct aiocb *aiocbp);
ssize_t wrap_aio_write(struct aiocb *aiocbp);
FuncData aio_write_data = { 0, (void*) wrap_aio_write, (void*) aio_write };
ssize_t wrap_aio_write(struct aiocb *aiocbp) {
  MacIOAutoObservation timer(IOInterposeObserver::OpWrite, aiocbp->aio_fildes);

  aio_write_t old_write = (aio_write_t) aio_write_data.Buffer;
  return old_write(aiocbp);
}

// Wrap pwrite-like functions.
// We have not seen them before, so just assert/report it.
typedef ssize_t (*pwrite_t)(int fd, const void *buf, size_t nbyte, off_t offset);
template<FuncData &foo>
ssize_t wrap_pwrite_temp(int fd, const void *buf, size_t nbyte, off_t offset) {
  MacIOAutoObservation timer(IOInterposeObserver::OpWrite, fd);
  pwrite_t old_write = (pwrite_t) foo.Buffer;
  return old_write(fd, buf, nbyte, offset);
}

// Define a FuncData for a pwrite-like functions.
#define DEFINE_PWRITE_DATA(X, NAME)                                        \
FuncData X ## _data = { NAME, (void*) wrap_pwrite_temp<X ## _data> };      \

// This exists everywhere.
DEFINE_PWRITE_DATA(pwrite, "pwrite")
// These exist on 32 bit OS X
DEFINE_PWRITE_DATA(pwrite_NOCANCEL_UNIX2003, "pwrite$NOCANCEL$UNIX2003");
DEFINE_PWRITE_DATA(pwrite_UNIX2003, "pwrite$UNIX2003");
// This exists on 64 bit OS X
DEFINE_PWRITE_DATA(pwrite_NOCANCEL, "pwrite$NOCANCEL");


typedef ssize_t (*writev_t)(int fd, const struct iovec *iov, int iovcnt);
template<FuncData &foo>
ssize_t wrap_writev_temp(int fd, const struct iovec *iov, int iovcnt) {
  MacIOAutoObservation timer(IOInterposeObserver::OpWrite, fd, nullptr, iovcnt);
  writev_t old_write = (writev_t) foo.Buffer;
  return old_write(fd, iov, iovcnt);
}

// Define a FuncData for a writev-like functions.
#define DEFINE_WRITEV_DATA(X, NAME)                                   \
FuncData X ## _data = { NAME, (void*) wrap_writev_temp<X ## _data> }; \

// This exists everywhere.
DEFINE_WRITEV_DATA(writev, "writev");
// These exist on 32 bit OS X
DEFINE_WRITEV_DATA(writev_NOCANCEL_UNIX2003, "writev$NOCANCEL$UNIX2003");
DEFINE_WRITEV_DATA(writev_UNIX2003, "writev$UNIX2003");
// This exists on 64 bit OS X
DEFINE_WRITEV_DATA(writev_NOCANCEL, "writev$NOCANCEL");

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);
template<FuncData &foo>
ssize_t wrap_write_temp(int fd, const void *buf, size_t count) {
  MacIOAutoObservation timer(IOInterposeObserver::OpWrite, fd, buf, count);
  write_t old_write = (write_t) foo.Buffer;
  return old_write(fd, buf, count);
}

// Define a FuncData for a write-like functions.
#define DEFINE_WRITE_DATA(X, NAME)                                   \
FuncData X ## _data = { NAME, (void*) wrap_write_temp<X ## _data> }; \

// This exists everywhere.
DEFINE_WRITE_DATA(write, "write");
// These exist on 32 bit OS X
DEFINE_WRITE_DATA(write_NOCANCEL_UNIX2003, "write$NOCANCEL$UNIX2003");
DEFINE_WRITE_DATA(write_UNIX2003, "write$UNIX2003");
// This exists on 64 bit OS X
DEFINE_WRITE_DATA(write_NOCANCEL, "write$NOCANCEL");

FuncData *Functions[] = { &aio_write_data,

                          &pwrite_data,
                          &pwrite_NOCANCEL_UNIX2003_data,
                          &pwrite_UNIX2003_data,
                          &pwrite_NOCANCEL_data,

                          &write_data,
                          &write_NOCANCEL_UNIX2003_data,
                          &write_UNIX2003_data,
                          &write_NOCANCEL_data,

                          &writev_data,
                          &writev_NOCANCEL_UNIX2003_data,
                          &writev_UNIX2003_data,
                          &writev_NOCANCEL_data};

const int NumFunctions = ArrayLength(Functions);

} // anonymous namespace

/******************************** IO Poisoning ********************************/

namespace mozilla {

void InitPoisonIOInterposer() {
  // Enable reporting from poisoned write methods
  sIsEnabled = true;

  // Make sure we only poison writes once!
  static bool WritesArePoisoned = false;
  if (WritesArePoisoned) {
    return;
  }
  WritesArePoisoned = true;

  // stdout and stderr are OK.
  MozillaRegisterDebugFD(1);
  MozillaRegisterDebugFD(2);

  for (int i = 0; i < NumFunctions; ++i) {
    FuncData *d = Functions[i];
    if (!d->Function) {
      d->Function = dlsym(RTLD_DEFAULT, d->Name);
    }
    if (!d->Function) {
      continue;
    }
    DebugOnly<mach_error_t> t = mach_override_ptr(d->Function, d->Wrapper,
                                       &d->Buffer);
    MOZ_ASSERT(t == err_none);
  }
}

void OnlyReportDirtyWrites() {
  sOnlyReportDirtyWrites = true;
}

void ClearPoisonIOInterposer() {
  // Not sure how or if we can unpoison the functions. Would be nice, but no
  // worries we won't need to do this anyway.
  sIsEnabled = false;
}

} // namespace mozilla
