// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/debug/alias.h"
#include "base/debug/profiler.h"
#include "base/logging.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"

#include "base/win/windows_version.h"

namespace base {

namespace {

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;

// This function has try handling, so it is separated out of its caller.
void SetNameInternal(PlatformThreadId thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
  }
}

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;
  if (!thread_params->joinable)
    base::ThreadRestrictions::SetSingletonAllowed(false);

  /* Retrieve a copy of the thread handle to use as the key in the
   * thread name mapping. */
  PlatformThreadHandle::Handle platform_handle;
  DuplicateHandle(
      GetCurrentProcess(),
      GetCurrentThread(),
      GetCurrentProcess(),
      &platform_handle,
      0,
      FALSE,
      DUPLICATE_SAME_ACCESS);

  ThreadIdNameManager::GetInstance()->RegisterThread(
      platform_handle,
      PlatformThread::CurrentId());

  delete thread_params;
  delegate->ThreadMain();

  ThreadIdNameManager::GetInstance()->RemoveName(
      platform_handle,
      PlatformThread::CurrentId());
  return NULL;
}

// CreateThreadInternal() matches PlatformThread::Create(), except that
// |out_thread_handle| may be NULL, in which case a non-joinable thread is
// created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThread::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle) {
  unsigned int flags = 0;
  if (stack_size > 0 && base::win::GetVersion() >= base::win::VERSION_XP) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  } else {
    stack_size = 0;
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = out_thread_handle != NULL;

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code
  // on the Windows thread pool, etc.  For some background on the difference:
  //   http://www.microsoft.com/msj/1099/win32/win321099.aspx
  void* thread_handle = CreateThread(
      NULL, stack_size, ThreadFunc, params, flags, NULL);
  if (!thread_handle) {
    delete params;
    return false;
  }

  if (out_thread_handle)
    *out_thread_handle = PlatformThreadHandle(thread_handle);
  else
    CloseHandle(thread_handle);
  return true;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return GetCurrentThreadId();
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  NOTIMPLEMENTED(); // See OpenThread()
  return PlatformThreadHandle();
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  // When measured with a high resolution clock, Sleep() sometimes returns much
  // too early. We may need to call it repeatedly to get the desired duration.
  TimeTicks end = TimeTicks::Now() + duration;
  TimeTicks now;
  while ((now = TimeTicks::Now()) < end)
    ::Sleep((end - now).InMillisecondsRoundedUp());
}

// static
void PlatformThread::SetName(const char* name) {
  ThreadIdNameManager::GetInstance()->SetName(CurrentId(), name);

  // On Windows only, we don't need to tell the profiler about the "BrokerEvent"
  // thread, as it exists only in the chrome.exe image, and never spawns or runs
  // tasks (items which could be profiled).  This test avoids the notification,
  // which would also (as a side effect) initialize the profiler in this unused
  // context, including setting up thread local storage, etc.  The performance
  // impact is not terrible, but there is no reason to do initialize it.
  if (0 != strcmp(name, "BrokerEvent"))
    tracked_objects::ThreadData::InitializeThreadContext(name);

  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  // If this image file is instrumented, we raise the exception anyway
  // to provide the profiler with human-readable thread names.
  if (!::IsDebuggerPresent() && !base::debug::IsBinaryInstrumented())
    return;

  SetNameInternal(CurrentId(), name);
}

// static
const char* PlatformThread::GetName() {
  return ThreadIdNameManager::GetInstance()->GetName(CurrentId());
}

// static
bool PlatformThread::Create(size_t stack_size, Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle);
}

// static
bool PlatformThread::CreateWithPriority(size_t stack_size, Delegate* delegate,
                                        PlatformThreadHandle* thread_handle,
                                        ThreadPriority priority) {
  bool result = Create(stack_size, delegate, thread_handle);
  if (result)
    SetThreadPriority(*thread_handle, priority);
  return result;
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateThreadInternal(stack_size, delegate, NULL);
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  DCHECK(thread_handle.handle_);
  // TODO(willchan): Enable this check once I can get it to work for Windows
  // shutdown.
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
#if 0
  base::ThreadRestrictions::AssertIOAllowed();
#endif

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  DWORD result = WaitForSingleObject(thread_handle.handle_, INFINITE);
  if (result != WAIT_OBJECT_0) {
    // Debug info for bug 127931.
    DWORD error = GetLastError();
    debug::Alias(&error);
    debug::Alias(&result);
    debug::Alias(&thread_handle.handle_);
    CHECK(false);
  }

  CloseHandle(thread_handle.handle_);
}

// static
void PlatformThread::SetThreadPriority(PlatformThreadHandle handle,
                                       ThreadPriority priority) {
  switch (priority) {
    case kThreadPriority_Normal:
      ::SetThreadPriority(handle.handle_, THREAD_PRIORITY_NORMAL);
      break;
    case kThreadPriority_RealtimeAudio:
      ::SetThreadPriority(handle.handle_, THREAD_PRIORITY_TIME_CRITICAL);
      break;
    default:
      NOTREACHED() << "Unknown priority.";
      break;
  }
}

}  // namespace base
