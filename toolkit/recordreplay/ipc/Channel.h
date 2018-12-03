/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Channel_h
#define mozilla_recordreplay_Channel_h

#include "base/process.h"

#include "mozilla/gfx/Types.h"
#include "mozilla/Maybe.h"

#include "File.h"
#include "JSControl.h"
#include "MiddlemanCall.h"
#include "Monitor.h"

namespace mozilla {
namespace recordreplay {

// This file has definitions for creating and communicating on a special
// bidirectional channel between a middleman process and a recording or
// replaying process. This communication is not included in the recording, and
// when replaying this is the only mechanism the child can use to communicate
// with the middleman process.
//
// Replaying processes can rewind themselves, restoring execution state and the
// contents of all heap memory to that at an earlier point. To keep the
// replaying process and middleman from getting out of sync with each other,
// there are tight constraints on when messages may be sent across the channel
// by one process or the other. At any given time the child process may be
// either paused or unpaused. If it is paused, it is not doing any execution
// and cannot rewind itself. If it is unpaused, it may execute content and may
// rewind itself.
//
// Messages can be sent from the child process to the middleman only when the
// child process is unpaused, and messages can only be sent from the middleman
// to the child process when the child process is paused. This prevents
// messages from being lost when they are sent from the middleman as the
// replaying process rewinds itself. A few exceptions to this rule are noted
// below.
//
// Some additional synchronization is needed between different child processes:
// replaying processes can read from the same file which a recording process is
// writing to. While it is ok for a replaying process to read from the file
// while the recording process is appending new chunks to it (see File.cpp),
// all replaying processes must be paused when the recording process is
// flushing a new index to the file.

#define ForEachMessageType(_Macro)                             \
  /* Messages sent from the middleman to the child process. */ \
                                                               \
  /* Sent at startup. */                                       \
  _Macro(Introduction)                                         \
                                                               \
  /* Sent to recording processes to indicate that the middleman will be running */ \
  /* developer tools server-side code instead of the recording process itself. */ \
  _Macro(SetDebuggerRunsInMiddleman)                           \
                                                               \
  /* Sent to recording processes when exiting, or to force a hanged replaying */ \
  /* process to crash. */                                      \
  _Macro(Terminate)                                            \
                                                               \
  /* Flush the current recording to disk. */                   \
  _Macro(FlushRecording)                                       \
                                                               \
  /* Poke a child that is recording to create an artificial checkpoint, rather than */ \
  /* (potentially) idling indefinitely. This has no effect on a replaying process. */ \
  _Macro(CreateCheckpoint)                                     \
                                                               \
  /* Debugger JSON messages are initially sent from the parent. The child unpauses */ \
  /* after receiving the message and will pause after it sends a DebuggerResponse. */ \
  _Macro(DebuggerRequest)                                      \
                                                               \
  /* Add a breakpoint position to stop at. Because a single entry point is used for */ \
  /* calling into the ReplayDebugger after pausing, the set of breakpoints is simply */ \
  /* a set of positions at which the child process should pause and send a HitBreakpoint */ \
  /* message. */                                               \
  _Macro(AddBreakpoint)                                        \
                                                               \
  /* Clear all installed breakpoints. */                       \
  _Macro(ClearBreakpoints)                                     \
                                                               \
  /* Unpause the child and play execution either to the next point when a */ \
  /* breakpoint is hit, or to the next checkpoint. Resumption may be either */ \
  /* forward or backward. */                                   \
  _Macro(Resume)                                               \
                                                               \
  /* Rewind to a particular saved checkpoint in the past. */   \
  _Macro(RestoreCheckpoint)                                    \
                                                               \
  /* Run forward to a particular execution point between the current checkpoint */ \
  /* and the next one. */                                      \
  _Macro(RunToPoint)                                           \
                                                               \
  /* Notify the child whether it is the active child and should send paint and similar */ \
  /* messages to the middleman. */                             \
  _Macro(SetIsActive)                                          \
                                                               \
  /* Set whether to perform intentional crashes, for testing. */ \
  _Macro(SetAllowIntentionalCrashes)                           \
                                                               \
  /* Set whether to save a particular checkpoint. */           \
  _Macro(SetSaveCheckpoint)                                    \
                                                               \
  /* Respond to a MiddlemanCallRequest message. */             \
  _Macro(MiddlemanCallResponse)                                \
                                                               \
  /* Messages sent from the child process to the middleman. */ \
                                                               \
  /* Sent in response to a FlushRecording, telling the middleman that the flush */ \
  /* has finished. */                                          \
  _Macro(RecordingFlushed)                                     \
                                                               \
  /* A critical error occurred and execution cannot continue. The child will */ \
  /* stop executing after sending this message and will wait to be terminated. */ \
  /* A minidump for the child has been generated. */           \
  _Macro(FatalError)                                           \
                                                               \
  /* Sent when a fatal error has occurred, but before the minidump has been */ \
  /* generated. */                                             \
  _Macro(BeginFatalError)                                      \
                                                               \
  /* The child's graphics were repainted. */                   \
  _Macro(Paint)                                                \
                                                               \
  /* Notify the middleman that a checkpoint or breakpoint was hit. */ \
  /* The child will pause after sending these messages. */     \
  _Macro(HitCheckpoint)                                        \
  _Macro(HitBreakpoint)                                        \
                                                               \
  /* Send a response to a DebuggerRequest message. */          \
  _Macro(DebuggerResponse)                                     \
                                                               \
  /* Call a system function from the middleman process which the child has */ \
  /* encountered after diverging from the recording. */        \
  _Macro(MiddlemanCallRequest)                                 \
                                                               \
  /* Reset all information generated by previous MiddlemanCallRequest messages. */ \
  _Macro(ResetMiddlemanCalls)                                  \
                                                               \
  /* Notify that the 'AlwaysMarkMajorCheckpoints' directive was invoked. */ \
  _Macro(AlwaysMarkMajorCheckpoints)

enum class MessageType {
#define DefineEnum(Kind) Kind,
  ForEachMessageType(DefineEnum)
#undef DefineEnum
};

struct Message {
  MessageType mType;

  // Total message size, including the header.
  uint32_t mSize;

 protected:
  Message(MessageType aType, uint32_t aSize) : mType(aType), mSize(aSize) {
    MOZ_RELEASE_ASSERT(mSize >= sizeof(*this));
  }

 public:
  Message* Clone() const {
    char* res = (char*)malloc(mSize);
    memcpy(res, this, mSize);
    return (Message*)res;
  }

  const char* TypeString() const {
    switch (mType) {
#define EnumToString(Kind) \
  case MessageType::Kind:  \
    return #Kind;
      ForEachMessageType(EnumToString)
#undef EnumToString
          default : return "Unknown";
    }
  }

  // Return whether this is a middleman->child message that can be sent while
  // the child is unpaused.
  bool CanBeSentWhileUnpaused() const {
    return mType == MessageType::CreateCheckpoint ||
           mType == MessageType::SetDebuggerRunsInMiddleman ||
           mType == MessageType::MiddlemanCallResponse ||
           mType == MessageType::Terminate;
  }

 protected:
  template <typename T, typename Elem>
  Elem* Data() {
    return (Elem*)(sizeof(T) + (char*)this);
  }

  template <typename T, typename Elem>
  const Elem* Data() const {
    return (const Elem*)(sizeof(T) + (const char*)this);
  }

  template <typename T, typename Elem>
  size_t DataSize() const {
    return (mSize - sizeof(T)) / sizeof(Elem);
  }

  template <typename T, typename Elem, typename... Args>
  static T* NewWithData(size_t aBufferSize, Args&&... aArgs) {
    size_t size = sizeof(T) + aBufferSize * sizeof(Elem);
    void* ptr = malloc(size);
    return new (ptr) T(size, std::forward<Args>(aArgs)...);
  }
};

struct IntroductionMessage : public Message {
  base::ProcessId mParentPid;
  uint32_t mArgc;

  IntroductionMessage(uint32_t aSize, base::ProcessId aParentPid,
                      uint32_t aArgc)
      : Message(MessageType::Introduction, aSize),
        mParentPid(aParentPid),
        mArgc(aArgc) {}

  char* ArgvString() { return Data<IntroductionMessage, char>(); }
  const char* ArgvString() const { return Data<IntroductionMessage, char>(); }

  static IntroductionMessage* New(base::ProcessId aParentPid, int aArgc,
                                  char* aArgv[]) {
    size_t argsLen = 0;
    for (int i = 0; i < aArgc; i++) {
      argsLen += strlen(aArgv[i]) + 1;
    }

    IntroductionMessage* res =
        NewWithData<IntroductionMessage, char>(argsLen, aParentPid, aArgc);

    size_t offset = 0;
    for (int i = 0; i < aArgc; i++) {
      memcpy(&res->ArgvString()[offset], aArgv[i], strlen(aArgv[i]) + 1);
      offset += strlen(aArgv[i]) + 1;
    }
    MOZ_RELEASE_ASSERT(offset == argsLen);

    return res;
  }

  static IntroductionMessage* RecordReplay(const IntroductionMessage& aMsg) {
    size_t introductionSize = RecordReplayValue(aMsg.mSize);
    IntroductionMessage* msg = (IntroductionMessage*)malloc(introductionSize);
    if (IsRecording()) {
      memcpy(msg, &aMsg, introductionSize);
    }
    RecordReplayBytes(msg, introductionSize);
    return msg;
  }
};

template <MessageType Type>
struct EmptyMessage : public Message {
  EmptyMessage() : Message(Type, sizeof(*this)) {}
};

typedef EmptyMessage<MessageType::SetDebuggerRunsInMiddleman>
    SetDebuggerRunsInMiddlemanMessage;
typedef EmptyMessage<MessageType::Terminate> TerminateMessage;
typedef EmptyMessage<MessageType::CreateCheckpoint> CreateCheckpointMessage;
typedef EmptyMessage<MessageType::FlushRecording> FlushRecordingMessage;

template <MessageType Type>
struct JSONMessage : public Message {
  explicit JSONMessage(uint32_t aSize) : Message(Type, aSize) {}

  const char16_t* Buffer() const { return Data<JSONMessage<Type>, char16_t>(); }
  size_t BufferSize() const { return DataSize<JSONMessage<Type>, char16_t>(); }

  static JSONMessage<Type>* New(const char16_t* aBuffer, size_t aBufferSize) {
    JSONMessage<Type>* res =
        NewWithData<JSONMessage<Type>, char16_t>(aBufferSize);
    MOZ_RELEASE_ASSERT(res->BufferSize() == aBufferSize);
    PodCopy(res->Data<JSONMessage<Type>, char16_t>(), aBuffer, aBufferSize);
    return res;
  }
};

typedef JSONMessage<MessageType::DebuggerRequest> DebuggerRequestMessage;
typedef JSONMessage<MessageType::DebuggerResponse> DebuggerResponseMessage;

struct AddBreakpointMessage : public Message {
  js::BreakpointPosition mPosition;

  explicit AddBreakpointMessage(const js::BreakpointPosition& aPosition)
      : Message(MessageType::AddBreakpoint, sizeof(*this)),
        mPosition(aPosition) {}
};

typedef EmptyMessage<MessageType::ClearBreakpoints> ClearBreakpointsMessage;

struct ResumeMessage : public Message {
  // Whether to travel forwards or backwards.
  bool mForward;

  explicit ResumeMessage(bool aForward)
      : Message(MessageType::Resume, sizeof(*this)), mForward(aForward) {}
};

struct RestoreCheckpointMessage : public Message {
  // The checkpoint to restore.
  size_t mCheckpoint;

  explicit RestoreCheckpointMessage(size_t aCheckpoint)
      : Message(MessageType::RestoreCheckpoint, sizeof(*this)),
        mCheckpoint(aCheckpoint) {}
};

struct RunToPointMessage : public Message {
  // The target execution point.
  js::ExecutionPoint mTarget;

  explicit RunToPointMessage(const js::ExecutionPoint& aTarget)
      : Message(MessageType::RunToPoint, sizeof(*this)), mTarget(aTarget) {}
};

struct SetIsActiveMessage : public Message {
  // Whether this is the active child process (see ParentIPC.cpp).
  bool mActive;

  explicit SetIsActiveMessage(bool aActive)
      : Message(MessageType::SetIsActive, sizeof(*this)), mActive(aActive) {}
};

struct SetAllowIntentionalCrashesMessage : public Message {
  // Whether to allow intentional crashes in the future or not.
  bool mAllowed;

  explicit SetAllowIntentionalCrashesMessage(bool aAllowed)
      : Message(MessageType::SetAllowIntentionalCrashes, sizeof(*this)),
        mAllowed(aAllowed) {}
};

struct SetSaveCheckpointMessage : public Message {
  // The checkpoint in question.
  size_t mCheckpoint;

  // Whether to save this checkpoint whenever it is encountered.
  bool mSave;

  SetSaveCheckpointMessage(size_t aCheckpoint, bool aSave)
      : Message(MessageType::SetSaveCheckpoint, sizeof(*this)),
        mCheckpoint(aCheckpoint),
        mSave(aSave) {}
};

typedef EmptyMessage<MessageType::RecordingFlushed> RecordingFlushedMessage;

struct FatalErrorMessage : public Message {
  explicit FatalErrorMessage(uint32_t aSize)
      : Message(MessageType::FatalError, aSize) {}

  const char* Error() const { return Data<FatalErrorMessage, const char>(); }
};

typedef EmptyMessage<MessageType::BeginFatalError> BeginFatalErrorMessage;

// The format for graphics data which will be sent to the middleman process.
// This needs to match the format expected for canvas image data, to avoid
// transforming the data before rendering it in the middleman process.
static const gfx::SurfaceFormat gSurfaceFormat = gfx::SurfaceFormat::R8G8B8X8;

struct PaintMessage : public Message {
  // Checkpoint whose state is being painted.
  uint32_t mCheckpointId;

  uint32_t mWidth;
  uint32_t mHeight;

  PaintMessage(uint32_t aCheckpointId, uint32_t aWidth, uint32_t aHeight)
      : Message(MessageType::Paint, sizeof(*this)),
        mCheckpointId(aCheckpointId),
        mWidth(aWidth),
        mHeight(aHeight) {}
};

struct HitCheckpointMessage : public Message {
  uint32_t mCheckpointId;
  bool mRecordingEndpoint;

  // When recording, the amount of non-idle time taken to get to this
  // checkpoint from the previous one.
  double mDurationMicroseconds;

  HitCheckpointMessage(uint32_t aCheckpointId, bool aRecordingEndpoint,
                       double aDurationMicroseconds)
      : Message(MessageType::HitCheckpoint, sizeof(*this)),
        mCheckpointId(aCheckpointId),
        mRecordingEndpoint(aRecordingEndpoint),
        mDurationMicroseconds(aDurationMicroseconds) {}
};

struct HitBreakpointMessage : public Message {
  bool mRecordingEndpoint;

  explicit HitBreakpointMessage(bool aRecordingEndpoint)
      : Message(MessageType::HitBreakpoint, sizeof(*this)),
        mRecordingEndpoint(aRecordingEndpoint) {}
};

typedef EmptyMessage<MessageType::AlwaysMarkMajorCheckpoints>
    AlwaysMarkMajorCheckpointsMessage;

template <MessageType Type>
struct BinaryMessage : public Message {
  explicit BinaryMessage(uint32_t aSize) : Message(Type, aSize) {}

  const char* BinaryData() const { return Data<BinaryMessage<Type>, char>(); }
  size_t BinaryDataSize() const {
    return DataSize<BinaryMessage<Type>, char>();
  }

  static BinaryMessage<Type>* New(const char* aData, size_t aDataSize) {
    BinaryMessage<Type>* res =
        NewWithData<BinaryMessage<Type>, char>(aDataSize);
    MOZ_RELEASE_ASSERT(res->BinaryDataSize() == aDataSize);
    PodCopy(res->Data<BinaryMessage<Type>, char>(), aData, aDataSize);
    return res;
  }
};

typedef BinaryMessage<MessageType::MiddlemanCallRequest>
    MiddlemanCallRequestMessage;
typedef BinaryMessage<MessageType::MiddlemanCallResponse>
    MiddlemanCallResponseMessage;
typedef EmptyMessage<MessageType::ResetMiddlemanCalls>
    ResetMiddlemanCallsMessage;

static inline MiddlemanCallResponseMessage* ProcessMiddlemanCallMessage(
    const MiddlemanCallRequestMessage& aMsg) {
  InfallibleVector<char> outputData;
  ProcessMiddlemanCall(aMsg.BinaryData(), aMsg.BinaryDataSize(), &outputData);
  return MiddlemanCallResponseMessage::New(outputData.begin(),
                                           outputData.length());
}

class Channel {
 public:
  // Note: the handler is responsible for freeing its input message. It will be
  // called on the channel's message thread.
  typedef std::function<void(Message*)> MessageHandler;

 private:
  // ID for this channel, unique for the middleman.
  size_t mId;

  // Callback to invoke off thread on incoming messages.
  MessageHandler mHandler;

  // Whether the channel is initialized and ready for outgoing messages.
  Atomic<bool, SequentiallyConsistent, Behavior::DontPreserve> mInitialized;

  // Descriptor used to accept connections on the parent side.
  int mConnectionFd;

  // Descriptor used to communicate with the other side.
  int mFd;

  // For synchronizing initialization of the channel.
  Monitor mMonitor;

  // Buffer for message data received from the other side of the channel.
  typedef InfallibleVector<char, 0, AllocPolicy<MemoryKind::Generic>>
      MessageBuffer;
  MessageBuffer* mMessageBuffer;

  // The number of bytes of data already in the message buffer.
  size_t mMessageBytes;

  // If spew is enabled, print a message and associated info to stderr.
  void PrintMessage(const char* aPrefix, const Message& aMsg);

  // Block until a complete message is received from the other side of the
  // channel.
  Message* WaitForMessage();

  // Main routine for the channel's thread.
  static void ThreadMain(void* aChannel);

 public:
  // Initialize this channel, connect to the other side, and spin up a thread
  // to process incoming messages by calling aHandler.
  Channel(size_t aId, bool aMiddlemanRecording, const MessageHandler& aHandler);

  size_t GetId() { return mId; }

  // Send a message to the other side of the channel. This must be called on
  // the main thread, except for fatal error messages.
  void SendMessage(const Message& aMsg);
};

// Command line option used to specify the middleman pid for a child process.
static const char* gMiddlemanPidOption = "-middlemanPid";

// Command line option used to specify the channel ID for a child process.
static const char* gChannelIDOption = "-recordReplayChannelID";

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_Channel_h
