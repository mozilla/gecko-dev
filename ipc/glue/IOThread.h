/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IOThreadParent_h
#define mozilla_ipc_IOThreadParent_h

#include "base/thread.h"
#include "chrome/common/ipc_channel.h"
#include "mozilla/ipc/ScopedPort.h"

namespace mozilla::ipc {

// Abstract background thread used for IPC I/O.
class IOThread : private base::Thread {
 public:
  // Lifecycle Note: The IOThread is stored in a static, and is returned by raw
  // pointer here from potentially any thread. This is OK because the IOThread
  // is very long lived, and should outlive any other thread which would
  // reference it (other than the main thread, which is responsible for the
  // lifetime of the IO Thread).
  static IOThread* Get() { return sSingleton; }

  // Get the nsISerialEventTarget which should be used to dispatch events to run
  // on the IOThreadBase.
  nsISerialEventTarget* GetEventTarget() {
    return base::Thread::message_loop()->SerialEventTarget();
  }

 protected:
  IOThread(const char* aName);
  ~IOThread();

  // Called by subclasses in the constructor/destructor to start/join the target
  // thread. This cannot be done in the base class constructor/destructor, as
  // the virtual Init()/CleanUp() methods need to be available.
  void StartThread();
  void StopThread();

  // Init() and Cleanup() methods which will be invoked on the IOThread when the
  // IOThread is started/stopped.
  void Init() override = 0;
  void CleanUp() override = 0;

 private:
  static IOThread* sSingleton;
};

// Background I/O thread used by the parent process.
class IOThreadParent : public IOThread {
 public:
  IOThreadParent();
  ~IOThreadParent();

 protected:
  void Init() override;
  void CleanUp() override;
};

// Background I/O thread used by the child process.
class IOThreadChild : public IOThread {
 public:
  IOThreadChild(IPC::Channel::ChannelHandle aClientHandle,
                base::ProcessId aParentPid);
  ~IOThreadChild();

  mozilla::ipc::ScopedPort TakeInitialPort() { return std::move(mInitialPort); }

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  mozilla::ipc::ScopedPort mInitialPort;
  IPC::Channel::ChannelHandle mClientHandle;
  base::ProcessId mParentPid;
};

inline void AssertIOThread() {
  MOZ_ASSERT(IOThread::Get()->GetEventTarget()->IsOnCurrentThread(),
             "should be on the async IO event target");
}

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_IOThreadParent_h
