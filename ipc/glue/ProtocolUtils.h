/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProtocolUtils_h
#define mozilla_ipc_ProtocolUtils_h 1

#include "base/process.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "prenv.h"

#include "IPCMessageStart.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/Transport.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "MainThreadUtils.h"

#if defined(ANDROID) && defined(DEBUG)
#include <android/log.h>
#endif

// WARNING: this takes into account the private, special-message-type
// enum in ipc_channel.h.  They need to be kept in sync.
namespace {
// XXX the max message ID is actually kuint32max now ... when this
// changed, the assumptions of the special message IDs changed in that
// they're not carving out messages from likely-unallocated space, but
// rather carving out messages from the end of space allocated to
// protocol 0.  Oops!  We can get away with this until protocol 0
// starts approaching its 65,536th message.
enum {
    CHANNEL_OPENED_MESSAGE_TYPE = kuint16max - 6,
    SHMEM_DESTROYED_MESSAGE_TYPE = kuint16max - 5,
    SHMEM_CREATED_MESSAGE_TYPE = kuint16max - 4,
    GOODBYE_MESSAGE_TYPE       = kuint16max - 3,
    CANCEL_MESSAGE_TYPE        = kuint16max - 2,

    // kuint16max - 1 is used by ipc_channel.h.
};
}

namespace mozilla {
namespace dom {
class ContentParent;
}

namespace net {
class NeckoParent;
}

namespace ipc {

#ifdef XP_WIN
const base::ProcessHandle kInvalidProcessHandle = INVALID_HANDLE_VALUE;

// In theory, on Windows, this is a valid process ID, but in practice they are
// currently divisible by four. Process IDs share the kernel handle allocation
// code and they are guaranteed to be divisible by four.
// As this could change for process IDs we shouldn't generally rely on this
// property, however even if that were to change, it seems safe to rely on this
// particular value never being used.
const base::ProcessId kInvalidProcessId = kuint32max;
#else
const base::ProcessHandle kInvalidProcessHandle = -1;
const base::ProcessId kInvalidProcessId = -1;
#endif

// Scoped base::ProcessHandle to ensure base::CloseProcessHandle is called.
struct ScopedProcessHandleTraits
{
  typedef base::ProcessHandle type;

  static type empty()
  {
    return kInvalidProcessHandle;
  }

  static void release(type aProcessHandle)
  {
    if (aProcessHandle && aProcessHandle != kInvalidProcessHandle) {
      base::CloseProcessHandle(aProcessHandle);
    }
  }
};
typedef mozilla::Scoped<ScopedProcessHandleTraits> ScopedProcessHandle;

class ProtocolFdMapping;
class ProtocolCloneContext;

// Used to pass references to protocol actors across the wire.
// Actors created on the parent-side have a positive ID, and actors
// allocated on the child side have a negative ID.
struct ActorHandle
{
    int mId;
};

// Used internally to represent a "trigger" that might cause a state
// transition.  Triggers are normalized across parent+child to Send
// and Recv (instead of child-in, child-out, parent-in, parent-out) so
// that they can share the same state machine implementation.  To
// further normalize, |Send| is used for 'call', |Recv| for 'answer'.
struct Trigger
{
    enum Action { Send, Recv };

    Trigger(Action action, int32_t msg) :
        mAction(action),
        mMsg(msg)
    {}

    Action mAction;
    int32_t mMsg;
};

class ProtocolCloneContext
{
  typedef mozilla::dom::ContentParent ContentParent;
  typedef mozilla::net::NeckoParent NeckoParent;

  nsRefPtr<ContentParent> mContentParent;
  NeckoParent* mNeckoParent;

public:
  ProtocolCloneContext();

  ~ProtocolCloneContext();

  void SetContentParent(ContentParent* aContentParent);

  ContentParent* GetContentParent() { return mContentParent; }

  void SetNeckoParent(NeckoParent* aNeckoParent)
  {
    mNeckoParent = aNeckoParent;
  }

  NeckoParent* GetNeckoParent() { return mNeckoParent; }
};

template<class ListenerT>
class IProtocolManager
{
public:
    enum ActorDestroyReason {
        FailedConstructor,
        Deletion,
        AncestorDeletion,
        NormalShutdown,
        AbnormalShutdown
    };

    typedef base::ProcessId ProcessId;

    virtual int32_t Register(ListenerT*) = 0;
    virtual int32_t RegisterID(ListenerT*, int32_t) = 0;
    virtual ListenerT* Lookup(int32_t) = 0;
    virtual void Unregister(int32_t) = 0;
    virtual void RemoveManagee(int32_t, ListenerT*) = 0;

    virtual Shmem::SharedMemory* CreateSharedMemory(
        size_t, SharedMemory::SharedMemoryType, bool, int32_t*) = 0;
    virtual bool AdoptSharedMemory(Shmem::SharedMemory*, int32_t*) = 0;
    virtual Shmem::SharedMemory* LookupSharedMemory(int32_t) = 0;
    virtual bool IsTrackingSharedMemory(Shmem::SharedMemory*) = 0;
    virtual bool DestroySharedMemory(Shmem&) = 0;

    // XXX odd ducks, acknowledged
    virtual ProcessId OtherPid() const = 0;
    virtual MessageChannel* GetIPCChannel() = 0;

    // The implementation of function is generated by code generator.
    virtual void CloneManagees(ListenerT* aSource,
                               ProtocolCloneContext* aCtx) = 0;
};

typedef IPCMessageStart ProtocolId;

/**
 * All RPC protocols should implement this interface.
 */
class IProtocol : protected MessageListener
{
public:
    /**
     * This function is used to clone this protocol actor.
     *
     * see IProtocol::CloneProtocol()
     */
    virtual IProtocol*
    CloneProtocol(MessageChannel* aChannel,
                  ProtocolCloneContext* aCtx) = 0;
};

/**
 * All top-level protocols should inherit this class.
 *
 * IToplevelProtocol tracks all top-level protocol actors created from
 * this protocol actor.
 */
class IToplevelProtocol : private LinkedListElement<IToplevelProtocol>
{
    friend class LinkedList<IToplevelProtocol>;
    friend class LinkedListElement<IToplevelProtocol>;

protected:
    explicit IToplevelProtocol(ProtocolId aProtoId);
    ~IToplevelProtocol();

    /**
     * Add an actor to the list of actors that have been opened by this
     * protocol.
     */
    void AddOpenedActor(IToplevelProtocol* aActor);

public:
    void SetTransport(Transport* aTrans)
    {
        mTrans = aTrans;
    }

    Transport* GetTransport() const { return mTrans; }

    ProtocolId GetProtocolId() const { return mProtocolId; }

    void GetOpenedActors(nsTArray<IToplevelProtocol*>& aActors);

    // This Unsafe version should only be used when all other threads are
    // frozen, since it performs no locking. It also takes a stack-allocated
    // array and its size (number of elements) rather than an nsTArray. The Nuwa
    // code that calls this function is not allowed to allocate memory.
    size_t GetOpenedActorsUnsafe(IToplevelProtocol** aActors, size_t aActorsMax);

    virtual IToplevelProtocol*
    CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                  base::ProcessHandle aPeerProcess,
                  ProtocolCloneContext* aCtx);

    void CloneOpenedToplevels(IToplevelProtocol* aTemplate,
                              const InfallibleTArray<ProtocolFdMapping>& aFds,
                              base::ProcessHandle aPeerProcess,
                              ProtocolCloneContext* aCtx);

private:
    void AddOpenedActorLocked(IToplevelProtocol* aActor);
    void GetOpenedActorsLocked(nsTArray<IToplevelProtocol*>& aActors);

    LinkedList<IToplevelProtocol> mOpenActors; // All protocol actors opened by this.
    IToplevelProtocol* mOpener;

    ProtocolId mProtocolId;
    Transport* mTrans;
};


inline bool
LoggingEnabled()
{
#if defined(DEBUG)
    return !!PR_GetEnv("MOZ_IPC_MESSAGE_LOG");
#else
    return false;
#endif
}

inline bool
LoggingEnabledFor(const char *aTopLevelProtocol)
{
#if defined(DEBUG)
    const char *filter = PR_GetEnv("MOZ_IPC_MESSAGE_LOG");
    if (!filter) {
        return false;
    }
    return strcmp(filter, "1") == 0 || strcmp(filter, aTopLevelProtocol) == 0;
#else
    return false;
#endif
}

MOZ_NEVER_INLINE void
ProtocolErrorBreakpoint(const char* aMsg);

MOZ_NEVER_INLINE void
FatalError(const char* aProtocolName, const char* aMsg,
           base::ProcessId aOtherPid, bool aIsParent);

struct PrivateIPDLInterface {};

nsresult
Bridge(const PrivateIPDLInterface&,
       MessageChannel*, base::ProcessId, MessageChannel*, base::ProcessId,
       ProtocolId, ProtocolId);

bool
Open(const PrivateIPDLInterface&,
     MessageChannel*, base::ProcessId, Transport::Mode,
     ProtocolId, ProtocolId);

bool
UnpackChannelOpened(const PrivateIPDLInterface&,
                    const IPC::Message&,
                    TransportDescriptor*, base::ProcessId*, ProtocolId*);

#if defined(XP_WIN)
// This is a restricted version of Windows' DuplicateHandle() function
// that works inside the sandbox and can send handles but not retrieve
// them.  Unlike DuplicateHandle(), it takes a process ID rather than
// a process handle.  It returns true on success, false otherwise.
bool
DuplicateHandle(HANDLE aSourceHandle,
                DWORD aTargetProcessId,
                HANDLE* aTargetHandle,
                DWORD aDesiredAccess,
                DWORD aOptions);
#endif

} // namespace ipc
} // namespace mozilla


namespace IPC {

template <>
struct ParamTraits<mozilla::ipc::ActorHandle>
{
    typedef mozilla::ipc::ActorHandle paramType;

    static void Write(Message* aMsg, const paramType& aParam)
    {
        IPC::WriteParam(aMsg, aParam.mId);
    }

    static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
    {
        int id;
        if (IPC::ReadParam(aMsg, aIter, &id)) {
            aResult->mId = id;
            return true;
        }
        return false;
    }

    static void Log(const paramType& aParam, std::wstring* aLog)
    {
        aLog->append(StringPrintf(L"(%d)", aParam.mId));
    }
};

} // namespace IPC


#endif  // mozilla_ipc_ProtocolUtils_h
