/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef ANDROID
#include <linux/ashmem.h>
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/Atomics.h"
#include "prenv.h"

namespace base {

SharedMemory::SharedMemory()
    : mapped_file_(-1), memory_(NULL), read_only_(false), max_size_(0) {}

SharedMemory::SharedMemory(SharedMemory&& other) {
  if (this == &other) {
    return;
  }

  mapped_file_ = other.mapped_file_;
  memory_ = other.memory_;
  read_only_ = other.read_only_;
  max_size_ = other.max_size_;

  other.mapped_file_ = -1;
  other.memory_ = nullptr;
}

SharedMemory::~SharedMemory() { Close(); }

bool SharedMemory::SetHandle(SharedMemoryHandle handle, bool read_only) {
  DCHECK(mapped_file_ == -1);

  mapped_file_ = handle.fd;
  read_only_ = read_only;
  return true;
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() { return SharedMemoryHandle(); }

// static
bool SharedMemory::AppendPosixShmPrefix(std::string* str, pid_t pid) {
#if defined(ANDROID) || defined(SHM_ANON)
  return false;
#else
  *str += '/';
#ifdef OS_LINUX
  // The Snap package environment doesn't provide a private /dev/shm
  // (it's used for communication with services like PulseAudio);
  // instead AppArmor is used to restrict access to it.  Anything with
  // this prefix is allowed:
  static const char* const kSnap = [] {
    auto instanceName = PR_GetEnv("SNAP_INSTANCE_NAME");
    if (instanceName != nullptr) {
      return instanceName;
    }
    // Compatibility for snapd <= 2.35:
    return PR_GetEnv("SNAP_NAME");
  }();

  if (kSnap) {
    StringAppendF(str, "snap.%s.", kSnap);
  }
#endif  // OS_LINUX
  // Hopefully the "implementation defined" name length limit is long
  // enough for this.
  StringAppendF(str, "org.mozilla.ipc.%d.", static_cast<int>(pid));
  return true;
#endif  // !ANDROID && !SHM_ANON
}

bool SharedMemory::Create(size_t size) {
  read_only_ = false;

  DCHECK(size > 0);
  DCHECK(mapped_file_ == -1);

  int fd;
  bool needs_truncate = true;

#ifdef ANDROID
  // Android has its own shared memory facility:
  fd = open("/" ASHMEM_NAME_DEF, O_RDWR, 0600);
  if (fd < 0) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }
  if (ioctl(fd, ASHMEM_SET_SIZE, size) != 0) {
    CHROMIUM_LOG(WARNING) << "failed to set shm size: " << strerror(errno);
    close(fd);
    return false;
  }
  needs_truncate = false;
#elif defined(SHM_ANON)
  // FreeBSD (or any other Unix that might decide to implement this
  // nice, simple API):
  fd = shm_open(SHM_ANON, O_RDWR, 0600);
#else
  // Generic Unix: shm_open + shm_unlink
  do {
    // The names don't need to be unique, but it saves time if they
    // usually are.
    static mozilla::Atomic<size_t> sNameCounter;
    std::string name;
    CHECK(AppendPosixShmPrefix(&name, getpid()));
    StringAppendF(&name, "%zu", sNameCounter++);
    // O_EXCL means the names being predictable shouldn't be a problem.
    fd = HANDLE_EINTR(shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600));
    if (fd >= 0) {
      if (shm_unlink(name.c_str()) != 0) {
        // This shouldn't happen, but if it does: assume the file is
        // in fact leaked, and bail out now while it's still 0-length.
        DLOG(FATAL) << "failed to unlink shm: " << strerror(errno);
        return false;
      }
    }
  } while (fd < 0 && errno == EEXIST);
#endif

  if (fd < 0) {
    CHROMIUM_LOG(WARNING) << "failed to open shm: " << strerror(errno);
    return false;
  }

  if (needs_truncate) {
    if (HANDLE_EINTR(ftruncate(fd, static_cast<off_t>(size))) != 0) {
      CHROMIUM_LOG(WARNING) << "failed to set shm size: " << strerror(errno);
      close(fd);
      return false;
    }
  }

  mapped_file_ = fd;
  max_size_ = size;
  return true;
}

bool SharedMemory::Map(size_t bytes) {
  if (mapped_file_ == -1) return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, 0);

  if (memory_) max_size_ = bytes;

  bool mmap_succeeded = (memory_ != (void*)-1);
  DCHECK(mmap_succeeded) << "Call to mmap failed, errno=" << errno;
  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL) return false;

  munmap(memory_, max_size_);
  memory_ = NULL;
  max_size_ = 0;
  return true;
}

bool SharedMemory::ShareToProcessCommon(ProcessId processId,
                                        SharedMemoryHandle* new_handle,
                                        bool close_self) {
  const int new_fd = dup(mapped_file_);
  DCHECK(new_fd >= -1);
  new_handle->fd = new_fd;
  new_handle->auto_close = true;

  if (close_self) Close();

  return true;
}

void SharedMemory::Close(bool unmap_view) {
  if (unmap_view) {
    Unmap();
  }

  if (mapped_file_ >= 0) {
    close(mapped_file_);
    mapped_file_ = -1;
  }
}

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

}  // namespace base
