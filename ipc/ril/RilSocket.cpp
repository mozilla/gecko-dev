/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RilSocket.h"
#include <fcntl.h>
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ipc/UnixSocketConnector.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h" // for MOZ_COUNT_CTOR, MOZ_COUNT_DTOR
#include "nsXULAppAPI.h"
#include "RilSocketConsumer.h"
#include "mozilla/Unused.h"

static const size_t MAX_READ_SIZE = 1 << 16;

namespace mozilla {
namespace ipc {

USING_WORKERS_NAMESPACE

//
// RilSocketIO
//

class RilSocketIO final : public ConnectionOrientedSocketIO
{
public:
  class ConnectTask;
  class DelayedConnectTask;
  class ReceiveTask;

  RilSocketIO(WorkerCrossThreadDispatcher* aDispatcher,
              MessageLoop* aConsumerLoop,
              MessageLoop* aIOLoop,
              RilSocket* aRilSocket,
              UnixSocketConnector* aConnector);
  ~RilSocketIO();

  RilSocket* GetRilSocket();
  DataSocket* GetDataSocket();

  // Delayed-task handling
  //

  void SetDelayedConnectTask(CancelableRunnable* aTask);
  void ClearDelayedConnectTask();
  void CancelDelayedConnectTask();

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) override;
  void ConsumeBuffer() override;
  void DiscardBuffer() override;

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override;

  bool IsShutdownOnConsumerThread() const override;
  bool IsShutdownOnIOThread() const override;

  void ShutdownOnConsumerThread() override;
  void ShutdownOnIOThread() override;

private:
  /**
   * Cross-thread dispatcher for the RIL worker
   */
  RefPtr<WorkerCrossThreadDispatcher> mDispatcher;

  /**
   * Consumer pointer. Non-thread safe RefPtr, so should only be manipulated
   * directly from consumer thread. All non-consumer-thread accesses should
   * happen with mIO as container.
   */
  RefPtr<RilSocket> mRilSocket;

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  /**
   * Task member for delayed connect task. Should only be access on consumer
   * thread.
   */
  CancelableRunnable* mDelayedConnectTask;

  /**
   * I/O buffer for received data
   */
  UniquePtr<UnixSocketRawData> mBuffer;
};

RilSocketIO::RilSocketIO(WorkerCrossThreadDispatcher* aDispatcher,
                         MessageLoop* aConsumerLoop,
                         MessageLoop* aIOLoop,
                         RilSocket* aRilSocket,
                         UnixSocketConnector* aConnector)
  : ConnectionOrientedSocketIO(aConsumerLoop, aIOLoop, aConnector)
  , mDispatcher(aDispatcher)
  , mRilSocket(aRilSocket)
  , mShuttingDownOnIOThread(false)
  , mDelayedConnectTask(nullptr)
{
  MOZ_ASSERT(mDispatcher);
  MOZ_ASSERT(mRilSocket);

  MOZ_COUNT_CTOR_INHERITED(RilSocketIO, ConnectionOrientedSocketIO);
}

RilSocketIO::~RilSocketIO()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(IsShutdownOnConsumerThread());

  MOZ_COUNT_DTOR_INHERITED(RilSocketIO, ConnectionOrientedSocketIO);
}

RilSocket*
RilSocketIO::GetRilSocket()
{
  return mRilSocket.get();
}

DataSocket*
RilSocketIO::GetDataSocket()
{
  return mRilSocket.get();
}

void
RilSocketIO::SetDelayedConnectTask(CancelableRunnable* aTask)
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = aTask;
}

void
RilSocketIO::ClearDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  mDelayedConnectTask = nullptr;
}

void
RilSocketIO::CancelDelayedConnectTask()
{
  MOZ_ASSERT(IsConsumerThread());

  if (!mDelayedConnectTask) {
    return;
  }

  mDelayedConnectTask->Cancel();
  ClearDelayedConnectTask();
}

// |DataSocketIO|

nsresult
RilSocketIO::QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer)
{
  MOZ_ASSERT(aBuffer);

  if (!mBuffer) {
    mBuffer = MakeUnique<UnixSocketRawData>(MAX_READ_SIZE);
  }
  *aBuffer = mBuffer.get();

  return NS_OK;
}

/**
 * |ReceiveTask| transfers data received on the I/O thread
 * to an instance of |RilSocket| on the consumer thread.
 */
class RilSocketIO::ReceiveTask final : public WorkerTask
{
public:
  ReceiveTask(RilSocketIO* aIO, UnixSocketBuffer* aBuffer)
    : mIO(aIO)
    , mBuffer(aBuffer)
  {
    MOZ_ASSERT(mIO);
  }

  bool RunTask(JSContext* aCx) override
  {
    // Dispatched via WCTD, but still needs to run on the consumer thread
    MOZ_ASSERT(mIO->IsConsumerThread());

    if (NS_WARN_IF(mIO->IsShutdownOnConsumerThread())) {
      // Since we've already explicitly closed and the close
      // happened before this, this isn't really an error.
      return true;
    }

    RilSocket* rilSocket = mIO->GetRilSocket();
    MOZ_ASSERT(rilSocket);

    rilSocket->ReceiveSocketData(aCx, mBuffer);

    return true;
  }

private:
  RilSocketIO* mIO;
  UniquePtr<UnixSocketBuffer> mBuffer;
};

void
RilSocketIO::ConsumeBuffer()
{
  RefPtr<ReceiveTask> task = new ReceiveTask(this, mBuffer.release());
  Unused << NS_WARN_IF(!mDispatcher->PostTask(task));
}

void
RilSocketIO::DiscardBuffer()
{
  // Nothing to do.
}

// |SocketIOBase|

SocketBase*
RilSocketIO::GetSocketBase()
{
  return GetDataSocket();
}

bool
RilSocketIO::IsShutdownOnConsumerThread() const
{
  MOZ_ASSERT(IsConsumerThread());

  return mRilSocket == nullptr;
}

bool
RilSocketIO::IsShutdownOnIOThread() const
{
  return mShuttingDownOnIOThread;
}

void
RilSocketIO::ShutdownOnConsumerThread()
{
  MOZ_ASSERT(IsConsumerThread());
  MOZ_ASSERT(!IsShutdownOnConsumerThread());

  mRilSocket = nullptr;
}

void
RilSocketIO::ShutdownOnIOThread()
{
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  Close(); // will also remove fd from I/O loop
  mShuttingDownOnIOThread = true;
}

//
// Socket tasks
//

class RilSocketIO::ConnectTask final
  : public SocketIOTask<RilSocketIO>
{
public:
  ConnectTask(RilSocketIO* aIO)
    : SocketIOTask<RilSocketIO>(aIO)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect();

    return NS_OK;
  }
};

class RilSocketIO::DelayedConnectTask final
  : public SocketIOTask<RilSocketIO>
{
public:
  DelayedConnectTask(RilSocketIO* aIO)
    : SocketIOTask<RilSocketIO>(aIO)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(GetIO()->IsConsumerThread());

    if (IsCanceled()) {
      return NS_OK;
    }

    RilSocketIO* io = GetIO();
    if (io->IsShutdownOnConsumerThread()) {
      return NS_OK;
    }

    io->ClearDelayedConnectTask();
    io->GetIOLoop()->PostTask(MakeAndAddRef<ConnectTask>(io));

    return NS_OK;
  }
};

//
// RilSocket
//

RilSocket::RilSocket(WorkerCrossThreadDispatcher* aDispatcher,
                     RilSocketConsumer* aConsumer, int aIndex)
  : mIO(nullptr)
  , mDispatcher(aDispatcher)
  , mConsumer(aConsumer)
  , mIndex(aIndex)
{
  MOZ_ASSERT(mDispatcher);
  MOZ_ASSERT(mConsumer);

  MOZ_COUNT_CTOR_INHERITED(RilSocket, ConnectionOrientedSocket);
}

RilSocket::~RilSocket()
{
  MOZ_ASSERT(!mIO);

  MOZ_COUNT_DTOR_INHERITED(RilSocket, ConnectionOrientedSocket);
}

void
RilSocket::ReceiveSocketData(JSContext* aCx,
                             UniquePtr<UnixSocketBuffer>& aBuffer)
{
  mConsumer->ReceiveSocketData(aCx, mIndex, aBuffer);
}

nsresult
RilSocket::Connect(UnixSocketConnector* aConnector, int aDelayMs,
                   MessageLoop* aConsumerLoop, MessageLoop* aIOLoop)
{
  MOZ_ASSERT(!mIO);

  mIO = new RilSocketIO(mDispatcher, aConsumerLoop, aIOLoop, this, aConnector);
  SetConnectionStatus(SOCKET_CONNECTING);

  if (aDelayMs > 0) {
    RefPtr<RilSocketIO::DelayedConnectTask> connectTask =
      MakeAndAddRef<RilSocketIO::DelayedConnectTask>(mIO);
    mIO->SetDelayedConnectTask(connectTask);
    MessageLoop::current()->PostDelayedTask(connectTask.forget(), aDelayMs);
  } else {
    aIOLoop->PostTask(MakeAndAddRef<RilSocketIO::ConnectTask>(mIO));
  }

  return NS_OK;
}

nsresult
RilSocket::Connect(UnixSocketConnector* aConnector, int aDelayMs)
{
  return Connect(aConnector, aDelayMs,
                 MessageLoop::current(), XRE_GetIOMessageLoop());
}

// |ConnectionOrientedSocket|

nsresult
RilSocket::PrepareAccept(UnixSocketConnector* aConnector,
                         MessageLoop* aConsumerLoop,
                         MessageLoop* aIOLoop,
                         ConnectionOrientedSocketIO*& aIO)
{
  MOZ_CRASH("|RilSocket| does not support accepting connections.");
}

// |DataSocket|

void
RilSocket::SendSocketData(UnixSocketIOBuffer* aBuffer)
{
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(mIO->IsConsumerThread());
  MOZ_ASSERT(!mIO->IsShutdownOnConsumerThread());

  mIO->GetIOLoop()->PostTask(
    MakeAndAddRef<SocketIOSendTask<RilSocketIO, UnixSocketIOBuffer>>(mIO, aBuffer));
}

// |SocketBase|

void
RilSocket::Close()
{
  MOZ_ASSERT(mIO);
  MOZ_ASSERT(mIO->IsConsumerThread());

  mIO->CancelDelayedConnectTask();

  // From this point on, we consider |mIO| as being deleted. We sever
  // the relationship here so any future calls to |Connect| will create
  // a new I/O object.
  mIO->ShutdownOnConsumerThread();
  mIO->GetIOLoop()->PostTask(MakeAndAddRef<SocketIOShutdownTask>(mIO));
  mIO = nullptr;

  NotifyDisconnect();
}

void
RilSocket::OnConnectSuccess()
{
  mConsumer->OnConnectSuccess(mIndex);
}

void
RilSocket::OnConnectError()
{
  mConsumer->OnConnectError(mIndex);
}

void
RilSocket::OnDisconnect()
{
  mConsumer->OnDisconnect(mIndex);
}

} // namespace ipc
} // namespace mozilla
