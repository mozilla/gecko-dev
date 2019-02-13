/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_ipc_SocketBase_h
#define mozilla_ipc_SocketBase_h

#include "base/message_loop.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace ipc {

//
// UnixSocketBuffer
//

/**
 * |UnixSocketBuffer| implements a FIFO buffer that stores raw socket
 * data, either for sending on a socket or received from a socket.
 */
class UnixSocketBuffer
{
public:
  virtual ~UnixSocketBuffer();

  const uint8_t* GetData() const
  {
    return mData + mOffset;
  }

  size_t GetSize() const
  {
    return mSize - mOffset;
  }

  const uint8_t* Consume(size_t aLen);

  nsresult Read(void* aValue, size_t aLen);

  nsresult Read(int8_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  nsresult Read(uint8_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  nsresult Read(int16_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  nsresult Read(uint16_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  nsresult Read(int32_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  nsresult Read(uint32_t& aValue)
  {
    return Read(&aValue, sizeof(aValue));
  }

  uint8_t* Append(size_t aLen);

  nsresult Write(const void* aValue, size_t aLen);

  nsresult Write(int8_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

  nsresult Write(uint8_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

  nsresult Write(int16_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

  nsresult Write(uint16_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

  nsresult Write(int32_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

  nsresult Write(uint32_t aValue)
  {
    return Write(&aValue, sizeof(aValue));
  }

protected:
  UnixSocketBuffer();

  /**
   * Sets the raw memory. The caller is responsible for freeing
   * this memory.
   *
   * @param aData A pointer to the buffer's raw memory.
   * @param aOffset The start of valid bytes in |aData|.
   * @param aSize The number of valid bytes in |aData|.
   * @param aAvailableSpace The number of bytes in |aData|.
   */
  void ResetBuffer(uint8_t* aData,
                   size_t aOffset, size_t aSize, size_t aAvailableSpace)
  {
    MOZ_ASSERT(aData || !aAvailableSpace);
    MOZ_ASSERT((aOffset + aSize) <= aAvailableSpace);

    mOffset = aOffset;
    mSize = aSize;
    mAvailableSpace = aAvailableSpace;
    mData = aData;
  }

  /**
   * Retrieves the memory buffer.
   *
   * @return A pointer to the buffer's raw memory.
   */
  uint8_t* GetBuffer()
  {
    return mData;
  }

  size_t GetLeadingSpace() const
  {
    return mOffset;
  }

  size_t GetTrailingSpace() const
  {
    return mAvailableSpace - mSize;
  }

  size_t GetAvailableSpace() const
  {
    return mAvailableSpace;
  }

  void* GetTrailingBytes()
  {
    return mData + mSize;
  }

  uint8_t* GetData(size_t aOffset)
  {
    MOZ_ASSERT(aOffset <= mSize);

    return mData + aOffset;
  }

  void SetRange(size_t aOffset, size_t aSize)
  {
    MOZ_ASSERT((aOffset + aSize) <= mAvailableSpace);

    mOffset = aOffset;
    mSize = mOffset + aSize;
  }

  void CleanupLeadingSpace();

private:
  size_t mSize;
  size_t mOffset;
  size_t mAvailableSpace;
  uint8_t* mData;
};

//
// UnixSocketIOBuffer
//

/**
 * |UnixSocketIOBuffer| is a |UnixSocketBuffer| that supports being
 * received on a socket or being send on a socket. Network protocols
 * might differ in their exact usage of Unix socket functions and
 * |UnixSocketIOBuffer| provides a protocol-neutral interface.
 */
class UnixSocketIOBuffer : public UnixSocketBuffer
{
public:
  virtual ~UnixSocketIOBuffer();

  /**
   * Receives data from aFd at the end of the buffer. The returned value
   * is the number of newly received bytes, or 0 if the peer shut down
   * its connection, or a negative value on errors.
   */
  virtual ssize_t Receive(int aFd) = 0;

  /**
   * Sends data to aFd from the beginning of the buffer. The returned value
   * is the number of bytes written, or a negative value on error.
   */
  virtual ssize_t Send(int aFd) = 0;
};

//
// UnixSocketRawData
//

class UnixSocketRawData final : public UnixSocketIOBuffer
{
public:
  /**
   * This constructor copies aData of aSize bytes length into the
   * new instance of |UnixSocketRawData|.
   *
   * @param aData The buffer to copy.
   * @param aSize The number of bytes in |aData|.
   */
  UnixSocketRawData(const void* aData, size_t aSize);

  /**
   * This constructor reserves aSize bytes of space. Currently
   * it's only possible to fill this buffer by calling |Receive|.
   *
   * @param aSize The number of bytes to allocate.
   */
  UnixSocketRawData(size_t aSize);

  /**
   * The destructor releases the buffer's raw memory.
   */
  ~UnixSocketRawData();

  /**
   * Receives data from aFd at the end of the buffer. The returned value
   * is the number of newly received bytes, or 0 if the peer shut down
   * its connection, or a negative value on errors.
   */
  ssize_t Receive(int aFd) override;

  /**
   * Sends data to aFd from the beginning of the buffer. The returned value
   * is the number of bytes written, or a negative value on error.
   */
  ssize_t Send(int aFd) override;
};

enum SocketConnectionStatus {
  SOCKET_DISCONNECTED = 0,
  SOCKET_LISTENING = 1,
  SOCKET_CONNECTING = 2,
  SOCKET_CONNECTED = 3
};

//
// SocketBase
//

class SocketBase
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketBase)

  SocketConnectionStatus GetConnectionStatus() const;

  int GetSuggestedConnectDelayMs() const;

  /**
   * Queues the internal representation of socket for deletion. Can be called
   * from consumer thread.
   */
  virtual void Close() = 0;

  /**
   * Callback for socket connect/accept success. Called after connect/accept has
   * finished. Will be run on consumer thread before any reads take place.
   */
  virtual void OnConnectSuccess() = 0;

  /**
   * Callback for socket connect/accept error. Will be run on consumer thread.
   */
  virtual void OnConnectError() = 0;

  /**
   * Callback for socket disconnect. Will be run on consumer thread.
   */
  virtual void OnDisconnect() = 0;

  /**
   * Called by implementation to notify consumer of success.
   */
  void NotifySuccess();

  /**
   * Called by implementation to notify consumer of error.
   */
  void NotifyError();

  /**
   * Called by implementation to notify consumer of disconnect.
   */
  void NotifyDisconnect();

protected:
  SocketBase();
  virtual ~SocketBase();

  void SetConnectionStatus(SocketConnectionStatus aConnectionStatus);

private:
  uint32_t CalculateConnectDelayMs() const;

  SocketConnectionStatus mConnectionStatus;
  PRIntervalTime mConnectTimestamp;
  uint32_t mConnectDelayMs;
};

//
// SocketIOBase
//

/**
 * |SocketIOBase| is a base class for Socket I/O classes that
 * perform operations on the I/O thread.
 */
class SocketIOBase
{
public:
  virtual ~SocketIOBase();

  /**
   * Implemented by socket I/O classes to return the current instance of
   * |SocketBase|.
   *
   * @return The current instance of |SocketBase|
   */
  virtual SocketBase* GetSocketBase() = 0;

  /**
   * Implemented by socket I/O classes to signal that the socket I/O class has
   * been shut down.
   *
   * @return True if the socket I/O class has been shut down, false otherwise.
   */
  virtual bool IsShutdownOnIOThread() const = 0;

  /**
   * Implemented by socket I/O classes to signal that socket class has
   * been shut down.
   *
   * @return True if the socket class has been shut down, false otherwise.
   */
  virtual bool IsShutdownOnConsumerThread() const = 0;

  /**
   * Signals to the socket I/O classes that it has been shut down.
   */
  virtual void ShutdownOnIOThread() = 0;

  /**
   * Signals to the socket I/O classes that the socket class has been
   * shut down.
   */
  virtual void ShutdownOnConsumerThread() = 0;

  /**
   * Returns the consumer thread.
   *
   * @return A pointer to the consumer thread.
   */
  MessageLoop* GetConsumerThread() const;

  /**
   * @return True if the current thread is the consumer thread, or false
   *         otherwise.
   */
  bool IsConsumerThread() const;

protected:
  SocketIOBase(MessageLoop* aConsumerLoop);

private:
  MessageLoop* mConsumerLoop;
};

//
// Socket tasks
//

/* |SocketTask| is a task for sending a message from
 * the I/O thread to the consumer thread.
 */
template <typename T>
class SocketTask : public Task
{
public:
  virtual ~SocketTask()
  { }

  T* GetIO() const
  {
    return mIO;
  }

protected:
  SocketTask(T* aIO)
    : mIO(aIO)
  {
    MOZ_ASSERT(aIO);
  }

private:
  T* mIO;
};

/**
 * |SocketEventTask| reports the connection state on the
 * I/O thread back to the consumer thread.
 */
class SocketEventTask final : public SocketTask<SocketIOBase>
{
public:
  enum SocketEvent {
    CONNECT_SUCCESS,
    CONNECT_ERROR,
    DISCONNECT
  };

  SocketEventTask(SocketIOBase* aIO, SocketEvent aEvent);

  void Run() override;

private:
  SocketEvent mEvent;
};

/**
 * |SocketRequestClosingTask| closes an instance of |SocketBase|
 * on the consumer thread.
 */
class SocketRequestClosingTask final : public SocketTask<SocketIOBase>
{
public:
  SocketRequestClosingTask(SocketIOBase* aIO);

  void Run() override;
};

/**
 * |SocketDeleteInstanceTask| deletes an object on the consumer thread.
 */
class SocketDeleteInstanceTask final : public Task
{
public:
  SocketDeleteInstanceTask(SocketIOBase* aIO);

  void Run() override;

private:
  nsAutoPtr<SocketIOBase> mIO;
};

//
// Socket I/O tasks
//

/* |SocketIOTask| holds a reference to a Socket I/O object. It's
 * supposed to run on the I/O thread.
 */
template<typename Tio>
class SocketIOTask : public CancelableTask
{
public:
  virtual ~SocketIOTask()
  { }

  Tio* GetIO() const
  {
    return mIO;
  }

  void Cancel() override
  {
    mIO = nullptr;
  }

  bool IsCanceled() const
  {
    return !mIO;
  }

protected:
  SocketIOTask(Tio* aIO)
    : mIO(aIO)
  {
    MOZ_ASSERT(mIO);
  }

private:
  Tio* mIO;
};

/**
 * |SocketIOShutdownTask| signals shutdown to the socket I/O class on
 * the I/O thread and sends it to the consumer thread for destruction.
 */
class SocketIOShutdownTask final : public SocketIOTask<SocketIOBase>
{
public:
  SocketIOShutdownTask(SocketIOBase* aIO);

  void Run() override;
};

}
}

#endif
