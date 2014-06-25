/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEBRIDGECHILD_H
#define MOZILLA_GFX_IMAGEBRIDGECHILD_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint64_t
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for TemporaryRef
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTrackerHolder
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureIdentifier, etc
#include "mozilla/layers/PImageBridgeChild.h"
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsRegion.h"                   // for nsIntRegion
class MessageLoop;
struct nsIntPoint;
struct nsIntRect;

namespace base {
class Thread;
}

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

class ClientTiledLayerBuffer;
class AsyncTransactionTracker;
class ImageClient;
class ImageContainer;
class ImageBridgeParent;
class CompositableClient;
struct CompositableTransaction;
class Image;
class TextureClient;

/**
 * Returns true if the current thread is the ImageBrdigeChild's thread.
 *
 * Can be called from any thread.
 */
bool InImageBridgeChildThread();

/**
 * The ImageBridge protocol is meant to allow ImageContainers to forward images
 * directly to the compositor thread/process without using the main thread.
 *
 * ImageBridgeChild is a CompositableForwarder just like ShadowLayerForwarder.
 * This means it also does transactions with the compositor thread/process,
 * except that the transactions are restricted to operations on the Compositables
 * and cannot contain messages affecting layers directly.
 *
 * ImageBridgeChild is also a ISurfaceAllocator. It can be used to allocate or
 * deallocate data that is shared with the compositor. The main differerence
 * with other ISurfaceAllocators is that some of its overriden methods can be
 * invoked from any thread.
 *
 * There are three important phases in the ImageBridge protocol. These three steps
 * can do different things depending if (A) the ImageContainer uses ImageBridge
 * or (B) it does not use ImageBridge:
 *
 * - When an ImageContainer calls its method SetCurrentImage:
 *   - (A) The image is sent directly to the compositor process through the
 *   ImageBridge IPDL protocol.
 *   On the compositor side the image is stored in a global table that associates
 *   the image with an ID corresponding to the ImageContainer, and a composition is
 *   triggered.
 *   - (B) Since it does not have an ImageBridge, the image is not sent yet.
 *   instead the will be sent to the compositor during the next layer transaction
 *   (on the main thread).
 *
 * - During a Layer transaction:
 *   - (A) The ImageContainer uses ImageBridge. The image is already available to the
 *   compositor process because it has been sent with SetCurrentImage. Yet, the
 *   CompositableHost on the compositor side will needs the ID referring to the
 *   ImageContainer to access the Image. So during the Swap operation that happens
 *   in the transaction, we swap the container ID rather than the image data.
 *   - (B) Since the ImageContainer does not use ImageBridge, the image data is swaped.
 *
 * - During composition:
 *   - (A) The CompositableHost has an AsyncID, it looks up the ID in the 
 *   global table to see if there is an image. If there is no image, nothing is rendered.
 *   - (B) The CompositableHost has image data rather than an ID (meaning it is not
 *   using ImageBridge), then it just composites the image data normally.
 *
 * This means that there might be a possibility for the ImageBridge to send the first
 * frame before the first layer transaction that will pass the container ID to the
 * CompositableHost happens. In this (unlikely) case the layer is not composited
 * until the layer transaction happens. This means this scenario is not harmful.
 *
 * Since sending an image through imageBridge triggers compositing, the main thread is
 * not used at all (except for the very first transaction that provides the
 * CompositableHost with an AsyncID).
 */
class ImageBridgeChild : public PImageBridgeChild
                       , public CompositableForwarder
                       , public AsyncTransactionTrackersHolder
{
  friend class ImageContainer;
  typedef InfallibleTArray<AsyncParentMessageData> AsyncParentMessageArray;
public:

  /**
   * Creates the image bridge with a dedicated thread for ImageBridgeChild.
   *
   * We may want to use a specifi thread in the future. In this case, use
   * CreateWithThread instead.
   */
  static void StartUp();

  static PImageBridgeChild*
  StartUpInChildProcess(Transport* aTransport, ProcessId aOtherProcess);

  /**
   * Destroys the image bridge by calling DestroyBridge, and destroys the
   * ImageBridge's thread.
   *
   * If you don't want to destroy the thread, call DestroyBridge directly
   * instead.
   */
  static void ShutDown();

  /**
   * Creates the ImageBridgeChild manager protocol.
   */
  static bool StartUpOnThread(base::Thread* aThread);

  /**
   * Returns true if the singleton has been created.
   *
   * Can be called from any thread.
   */
  static bool IsCreated();

  /**
   * returns the singleton instance.
   *
   * can be called from any thread.
   */
  static ImageBridgeChild* GetSingleton();


  /**
   * Dispatches a task to the ImageBridgeChild thread to do the connection
   */
  void ConnectAsync(ImageBridgeParent* aParent);

  static void IdentifyCompositorTextureHost(const TextureFactoryIdentifier& aIdentifier);

  void BeginTransaction();
  void EndTransaction();

  /**
   * Returns the ImageBridgeChild's thread.
   *
   * Can be called from any thread.
   */
  base::Thread * GetThread() const;

  /**
   * Returns the ImageBridgeChild's message loop.
   *
   * Can be called from any thread.
   */
  MessageLoop * GetMessageLoop() const;

  PCompositableChild* AllocPCompositableChild(const TextureInfo& aInfo, uint64_t* aID) MOZ_OVERRIDE;
  bool DeallocPCompositableChild(PCompositableChild* aActor) MOZ_OVERRIDE;

  /**
   * This must be called by the static function DeleteImageBridgeSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  ~ImageBridgeChild();

  virtual PTextureChild*
  AllocPTextureChild(const SurfaceDescriptor& aSharedData, const TextureFlags& aFlags) MOZ_OVERRIDE;

  virtual bool
  DeallocPTextureChild(PTextureChild* actor) MOZ_OVERRIDE;

  virtual bool
  RecvParentAsyncMessages(const InfallibleTArray<AsyncParentMessageData>& aMessages) MOZ_OVERRIDE;

  TemporaryRef<ImageClient> CreateImageClient(CompositableType aType);
  TemporaryRef<ImageClient> CreateImageClientNow(CompositableType aType);

  static void DispatchReleaseImageClient(ImageClient* aClient);
  static void DispatchReleaseTextureClient(TextureClient* aClient);
  static void DispatchImageClientUpdate(ImageClient* aClient, ImageContainer* aContainer);

  /**
   * Flush all Images sent to CompositableHost.
   */
  static void FlushAllImages(ImageClient* aClient, ImageContainer* aContainer, bool aExceptFront);

  // CompositableForwarder

  virtual void Connect(CompositableClient* aCompositable) MOZ_OVERRIDE;

  /**
   * See CompositableForwarder::UpdatedTexture
   */
  virtual void UpdatedTexture(CompositableClient* aCompositable,
                              TextureClient* aTexture,
                              nsIntRegion* aRegion) MOZ_OVERRIDE;

  virtual bool IsImageBridgeChild() const MOZ_OVERRIDE { return true; }

  /**
   * See CompositableForwarder::UseTexture
   */
  virtual void UseTexture(CompositableClient* aCompositable,
                          TextureClient* aClient) MOZ_OVERRIDE;
  virtual void UseComponentAlphaTextures(CompositableClient* aCompositable,
                                         TextureClient* aClientOnBlack,
                                         TextureClient* aClientOnWhite) MOZ_OVERRIDE;

  virtual void SendFenceHandle(AsyncTransactionTracker* aTracker,
                               PTextureChild* aTexture,
                               const FenceHandle& aFence) MOZ_OVERRIDE;

  virtual void RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                             TextureClient* aTexture) MOZ_OVERRIDE;

  virtual void RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                  CompositableClient* aCompositable,
                                                  TextureClient* aTexture) MOZ_OVERRIDE;

  virtual void RemoveTexture(TextureClient* aTexture) MOZ_OVERRIDE;

  virtual void UseTiledLayerBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptorTiles& aTileLayerDescriptor) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("should not be called");
  }

  virtual void UpdateTextureIncremental(CompositableClient* aCompositable,
                                        TextureIdentifier aTextureId,
                                        SurfaceDescriptor& aDescriptor,
                                        const nsIntRegion& aUpdatedRegion,
                                        const nsIntRect& aBufferRect,
                                        const nsIntPoint& aBufferRotation) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("should not be called");
  }

  /**
   * Communicate the picture rect of a YUV image in aLayer to the compositor
   */
  virtual void UpdatePictureRect(CompositableClient* aCompositable,
                                 const nsIntRect& aRect) MOZ_OVERRIDE;


  virtual void CreatedIncrementalBuffer(CompositableClient* aCompositable,
                                        const TextureInfo& aTextureInfo,
                                        const nsIntRect& aBufferRect) MOZ_OVERRIDE
  {
    NS_RUNTIMEABORT("should not be called");
  }
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) MOZ_OVERRIDE {
    NS_RUNTIMEABORT("should not be called");
  }

  // ISurfaceAllocator

  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual bool AllocUnsafeShmem(size_t aSize,
                                mozilla::ipc::SharedMemory::SharedMemoryType aType,
                                mozilla::ipc::Shmem* aShmem) MOZ_OVERRIDE;
  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual bool AllocShmem(size_t aSize,
                          mozilla::ipc::SharedMemory::SharedMemoryType aType,
                          mozilla::ipc::Shmem* aShmem) MOZ_OVERRIDE;
  /**
   * See ISurfaceAllocator.h
   * Can be used from any thread.
   * If used outside the ImageBridgeChild thread, it will proxy a synchronous
   * call on the ImageBridgeChild thread.
   */
  virtual void DeallocShmem(mozilla::ipc::Shmem& aShmem);

  virtual PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                                       TextureFlags aFlags) MOZ_OVERRIDE;

  virtual bool IsSameProcess() const MOZ_OVERRIDE;

  void SendPendingAsyncMessge();

  void MarkShutDown();
protected:
  ImageBridgeChild();
  bool DispatchAllocShmemInternal(size_t aSize,
                                  SharedMemory::SharedMemoryType aType,
                                  Shmem* aShmem,
                                  bool aUnsafe);

  CompositableTransaction* mTxn;
  bool mShuttingDown;
};

} // layers
} // mozilla

#endif
