/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeParent.h"
#include <stdint.h>                     // for uint64_t, uint32_t
#include "CompositableHost.h"           // for CompositableParent, Create
#include "base/message_loop.h"          // for MessageLoop
#include "base/process.h"               // for ProcessHandle
#include "base/process_util.h"          // for OpenProcessHandle
#include "base/task.h"                  // for CancelableTask, DeleteTask, etc
#include "base/tracked.h"               // for FROM_HERE
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/CompositorParent.h"  // for CompositorParent
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply
#include "mozilla/layers/LayersSurfaces.h"  // for PGrallocBufferParent
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/PImageBridgeParent.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_RUNTIMEABORT, etc
#include "nsISupportsImpl.h"            // for ImageBridgeParent::Release, etc
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl
#include "nsTArrayForwardDeclare.h"     // for InfallibleTArray
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#include "mozilla/layers/TextureHost.h"

using namespace base;
using namespace mozilla::ipc;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class PGrallocBufferParent;

ImageBridgeParent::ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport)
  : mMessageLoop(aLoop)
  , mTransport(aTransport)
{
  // creates the map only if it has not been created already, so it is safe
  // with several bridges
  CompositableMap::Create();
}

ImageBridgeParent::~ImageBridgeParent()
{
  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
}

void
ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &ImageBridgeParent::DeferredDestroy));
}

bool
ImageBridgeParent::RecvUpdate(const EditArray& aEdits, EditReplyArray* aReply)
{
  // If we don't actually have a compositor, then don't bother
  // creating any textures.
  if (Compositor::GetBackend() == LayersBackend::LAYERS_NONE) {
    return true;
  }

  EditReplyVector replyv;
  for (EditArray::index_type i = 0; i < aEdits.Length(); ++i) {
    ReceiveCompositableUpdate(aEdits[i], replyv);
  }

  aReply->SetCapacity(replyv.size());
  if (replyv.size() > 0) {
    aReply->AppendElements(&replyv.front(), replyv.size());
  }

  // Ensure that any pending operations involving back and front
  // buffers have completed, so that neither process stomps on the
  // other's buffer contents.
  LayerManagerComposite::PlatformSyncBeforeReplyUpdate();

  return true;
}

bool
ImageBridgeParent::RecvUpdateNoSwap(const EditArray& aEdits)
{
  InfallibleTArray<EditReply> noReplies;
  bool success = RecvUpdate(aEdits, &noReplies);
  NS_ABORT_IF_FALSE(noReplies.Length() == 0, "RecvUpdateNoSwap requires a sync Update to carry Edits");
  return success;
}

static void
ConnectImageBridgeInParentProcess(ImageBridgeParent* aBridge,
                                  Transport* aTransport,
                                  ProcessHandle aOtherProcess)
{
  aBridge->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ipc::ParentSide);
}

/*static*/ PImageBridgeParent*
ImageBridgeParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }

  MessageLoop* loop = CompositorParent::CompositorLoop();
  nsRefPtr<ImageBridgeParent> bridge = new ImageBridgeParent(loop, aTransport);
  bridge->mSelfRef = bridge;
  loop->PostTask(FROM_HERE,
                 NewRunnableFunction(ConnectImageBridgeInParentProcess,
                                     bridge.get(), aTransport, processHandle));
  return bridge.get();
}

bool ImageBridgeParent::RecvStop()
{
  // If there is any texture still alive we have to force it to deallocate the
  // device data (GL textures, etc.) now because shortly after SenStop() returns
  // on the child side the widget will be destroyed along with it's associated
  // GL context.
  InfallibleTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
  return true;
}

static  uint64_t GenImageContainerID() {
  static uint64_t sNextImageID = 1;

  ++sNextImageID;
  return sNextImageID;
}

PGrallocBufferParent*
ImageBridgeParent::AllocPGrallocBufferParent(const IntSize& aSize,
                                             const uint32_t& aFormat,
                                             const uint32_t& aUsage,
                                             MaybeMagicGrallocBufferHandle* aOutHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create(aSize, aFormat, aUsage, aOutHandle);
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
ImageBridgeParent::DeallocPGrallocBufferParent(PGrallocBufferParent* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
}

PCompositableParent*
ImageBridgeParent::AllocPCompositableParent(const TextureInfo& aInfo,
                                            uint64_t* aID)
{
  uint64_t id = GenImageContainerID();
  *aID = id;
  return new CompositableParent(this, aInfo, id);
}

bool ImageBridgeParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  delete aActor;
  return true;
}

PTextureParent*
ImageBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                       const TextureFlags& aFlags)
{
  return TextureHost::CreateIPDLActor(this, aSharedData, aFlags);
}

bool
ImageBridgeParent::DeallocPTextureParent(PTextureParent* actor)
{
  return TextureHost::DestroyIPDLActor(actor);
}

MessageLoop * ImageBridgeParent::GetMessageLoop() {
  return mMessageLoop;
}

void
ImageBridgeParent::DeferredDestroy()
{
  mSelfRef = nullptr;
  // |this| was just destroyed, hands off
}

IToplevelProtocol*
ImageBridgeParent::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                 base::ProcessHandle aPeerProcess,
                                 mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == unsigned(GetProtocolId())) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PImageBridgeParent* bridge = Create(transport, base::GetProcId(aPeerProcess));
      bridge->CloneManagees(this, aCtx);
      bridge->IToplevelProtocol::SetTransport(transport);
      return bridge;
    }
  }
  return nullptr;
}

} // layers
} // mozilla
