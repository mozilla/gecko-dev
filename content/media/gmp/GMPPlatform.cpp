/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPPlatform.h"
#include "mozilla/Monitor.h"

namespace mozilla {
namespace gmp {

static MessageLoop* sMainLoop = nullptr;

// We just need a refcounted wrapper for GMPTask objects.
class Runnable : public RefCounted<Runnable>
{
public:
  Runnable(GMPTask* aTask)
  : mTask(aTask)
  {
    MOZ_ASSERT(mTask);
  }

  virtual ~Runnable()
  {
  }

  void Run()
  {
    mTask->Run();
  }

private:
  nsAutoPtr<GMPTask> mTask;
};

class SyncRunnable : public RefCounted<Runnable>
{
public:
  SyncRunnable(GMPTask* aTask, MessageLoop* aMessageLoop)
  : mTask(aTask),
    mMessageLoop(aMessageLoop),
    mMonitor("GMPSyncRunnable")
  {
    MOZ_ASSERT(mTask);
  }

  virtual ~SyncRunnable()
  {
  }

  void Post()
  {
    mozilla::MonitorAutoLock lock(mMonitor);
    mMessageLoop->PostTask(FROM_HERE, NewRunnableMethod(this, &SyncRunnable::Run));
    lock.Wait();
  }

  void Run()
  {
    mTask->Run();
    mozilla::MonitorAutoLock(mMonitor).Notify();
  }

private:
  nsAutoPtr<GMPTask> mTask;
  MessageLoop* mMessageLoop;
  mozilla::Monitor mMonitor;
};

GMPErr
CreateThread(GMPThread** aThread)
{
  if (!aThread) {
    return GMPGenericErr;
  }

  auto thread = new GMPThreadImpl();
  if (!thread) {
    return GMPGenericErr;
  }

  *aThread = thread;

  return GMPNoErr;
}

GMPErr
RunOnMainThread(GMPTask* aTask)
{
  if (!aTask || !sMainLoop) {
    return GMPGenericErr;
  }

  nsRefPtr<Runnable> r = new Runnable(aTask);
  sMainLoop->PostTask(FROM_HERE, NewRunnableMethod(r.get(), &Runnable::Run));

  return GMPNoErr;
}

GMPErr
SyncRunOnMainThread(GMPTask* aTask)
{
  if (!aTask || !sMainLoop) {
    return GMPGenericErr;
  }

  nsRefPtr<SyncRunnable> r = new SyncRunnable(aTask, sMainLoop);
  r->Post();

  return GMPNoErr;
}

GMPErr
CreateMutex(GMPMutex** aMutex)
{
  if (!aMutex) {
    return GMPGenericErr;
  }

  auto mutex = new GMPMutexImpl();
  if (!mutex) {
    return GMPGenericErr;
  }

  *aMutex = mutex;

  return GMPNoErr;
}

void
InitPlatformAPI(GMPPlatformAPI& aPlatformAPI)
{
  aPlatformAPI.version = 0;
  aPlatformAPI.createthread = &CreateThread;
  aPlatformAPI.runonmainthread = &RunOnMainThread;
  aPlatformAPI.syncrunonmainthread = &SyncRunOnMainThread;
  aPlatformAPI.createmutex = &CreateMutex;
}

GMPThreadImpl::GMPThreadImpl()
: mThread("GMPThread")
{
  // We'll assume that the first time someone constructs a thread object
  // they're doing it from the main thread. I like to live dangerously.
  if (!sMainLoop) {
    sMainLoop = MessageLoop::current();
  }
}

GMPThreadImpl::~GMPThreadImpl()
{
}

void
GMPThreadImpl::Post(GMPTask* aTask)
{
  if (!mThread.IsRunning()) {
    bool started = mThread.Start();
    if (!started) {
      NS_WARNING("Unable to start GMPThread!");
      return;
    }
  }

//XXXJOSH seems like this is not resulting in the event being run
  nsRefPtr<Runnable> r = new Runnable(aTask);
  mThread.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(r.get(), &Runnable::Run));
}

void
GMPThreadImpl::Join()
{
  if (mThread.IsRunning()) {
    mThread.Stop();
  }
}

GMPMutexImpl::GMPMutexImpl()
: mMutex("gmp-mutex")
{
}

GMPMutexImpl::~GMPMutexImpl()
{
}

void
GMPMutexImpl::Acquire()
{
  mMutex.Lock();
}

void
GMPMutexImpl::Release()
{
  mMutex.Unlock();
}

} // namespace gmp
} // namespace mozilla
