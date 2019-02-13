// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sched.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/safe_strerror_posix.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"

#if defined(OS_MACOSX)
#include <sys/resource.h>
#include <algorithm>
#endif

#if defined(OS_LINUX)
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace base {

void InitThreading();
void InitOnThread();
void TerminateOnThread();
size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes);

namespace {

struct ThreadParams {
  ThreadParams()
      : delegate(NULL),
        joinable(false),
        priority(kThreadPriority_Normal),
        handle(NULL),
        handle_set(false, false) {
  }

  PlatformThread::Delegate* delegate;
  bool joinable;
  ThreadPriority priority;
  PlatformThreadHandle* handle;
  WaitableEvent handle_set;
};

void* ThreadFunc(void* params) {
  base::InitOnThread();
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);

  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable)
    base::ThreadRestrictions::SetSingletonAllowed(false);

  if (thread_params->priority != kThreadPriority_Normal) {
    PlatformThread::SetThreadPriority(PlatformThread::CurrentHandle(),
                                      thread_params->priority);
  }

  // Stash the id in the handle so the calling thread has a complete
  // handle, and unblock the parent thread.
  *(thread_params->handle) = PlatformThreadHandle(pthread_self(),
                                                  PlatformThread::CurrentId());
  thread_params->handle_set.Signal();

  ThreadIdNameManager::GetInstance()->RegisterThread(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

  delegate->ThreadMain();

  ThreadIdNameManager::GetInstance()->RemoveName(
      PlatformThread::CurrentHandle().platform_handle(),
      PlatformThread::CurrentId());

  base::TerminateOnThread();
  return NULL;
}

bool CreateThread(size_t stack_size, bool joinable,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle,
                  ThreadPriority priority) {
  base::InitThreading();

  bool success = false;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Pthreads are joinable by default, so only specify the detached
  // attribute if the thread should be non-joinable.
  if (!joinable) {
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  }

  // Get a better default if available.
  if (stack_size == 0)
    stack_size = base::GetDefaultThreadStackSize(attributes);

  if (stack_size > 0)
    pthread_attr_setstacksize(&attributes, stack_size);

  ThreadParams params;
  params.delegate = delegate;
  params.joinable = joinable;
  params.priority = priority;
  params.handle = thread_handle;

  pthread_t handle;
  int err = pthread_create(&handle,
                           &attributes,
                           ThreadFunc,
                           &params);
  success = !err;
  if (!success) {
    // Value of |handle| is undefined if pthread_create fails.
    handle = 0;
    errno = err;
    PLOG(ERROR) << "pthread_create";
  }

  pthread_attr_destroy(&attributes);

  // Don't let this call complete until the thread id
  // is set in the handle.
  if (success)
    params.handle_set.Wait();
  CHECK_EQ(handle, thread_handle->platform_handle());

  return success;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_MACOSX)
  return pthread_mach_thread_np(pthread_self());
#elif defined(OS_LINUX)
  return syscall(__NR_gettid);
#elif defined(OS_ANDROID)
  return gettid();
#elif defined(OS_SOLARIS) || defined(OS_QNX)
  return pthread_self();
#elif defined(OS_NACL) && defined(__GLIBC__)
  return pthread_self();
#elif defined(OS_NACL) && !defined(__GLIBC__)
  // Pointers are 32-bits in NaCl.
  return reinterpret_cast<int32>(pthread_self());
#elif defined(OS_POSIX)
  return reinterpret_cast<int64>(pthread_self());
#endif
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(pthread_self());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(pthread_self(), CurrentId());
}

// static
void PlatformThread::YieldCurrentThread() {
  sched_yield();
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  struct timespec sleep_time, remaining;

  // Break the duration into seconds and nanoseconds.
  // NOTE: TimeDelta's microseconds are int64s while timespec's
  // nanoseconds are longs, so this unpacking must prevent overflow.
  sleep_time.tv_sec = duration.InSeconds();
  duration -= TimeDelta::FromSeconds(sleep_time.tv_sec);
  sleep_time.tv_nsec = duration.InMicroseconds() * 1000;  // nanoseconds

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::Create(size_t stack_size, Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  return CreateThread(stack_size, true /* joinable thread */,
                      delegate, thread_handle, kThreadPriority_Normal);
}

// static
bool PlatformThread::CreateWithPriority(size_t stack_size, Delegate* delegate,
                                        PlatformThreadHandle* thread_handle,
                                        ThreadPriority priority) {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  return CreateThread(stack_size, true,  // joinable thread
                      delegate, thread_handle, priority);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  PlatformThreadHandle unused;

  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  bool result = CreateThread(stack_size, false /* non-joinable thread */,
                             delegate, &unused, kThreadPriority_Normal);
  return result;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  base::ThreadRestrictions::AssertIOAllowed();
  CHECK_EQ(0, pthread_join(thread_handle.handle_, NULL));
}

}  // namespace base
