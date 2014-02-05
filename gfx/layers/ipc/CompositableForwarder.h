/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITABLEFORWARDER
#define MOZILLA_LAYERS_COMPOSITABLEFORWARDER

#include <stdint.h>                     // for int32_t, uint64_t
#include "gfxTypes.h"
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "nsRegion.h"                   // for nsIntRegion

struct nsIntPoint;
struct nsIntRect;

namespace mozilla {
namespace layers {

class CompositableClient;
class TextureFactoryIdentifier;
class SurfaceDescriptor;
class SurfaceDescriptorTiles;
class ThebesBufferData;
class DeprecatedTextureClient;
class BasicTiledLayerBuffer;
class PTextureChild;

/**
 * A transaction is a set of changes that happenned on the content side, that
 * should be sent to the compositor side.
 * CompositableForwarder is an interface to manage a transaction of
 * compositable objetcs.
 *
 * ShadowLayerForwarder is an example of a CompositableForwarder (that can
 * additionally forward modifications of the Layer tree).
 * ImageBridgeChild is another CompositableForwarder.
 */
class CompositableForwarder : public ISurfaceAllocator
{
  friend class AutoOpenSurface;
  friend class DeprecatedTextureClientShmem;
public:

  CompositableForwarder()
    : mMultiProcess(false)
  {}

  /**
   * Setup the IPDL actor for aCompositable to be part of layers
   * transactions.
   */
  virtual void Connect(CompositableClient* aCompositable) = 0;

  /**
   * When using the Thebes layer pattern of swapping or updating
   * TextureClient/Host pairs without sending SurfaceDescriptors,
   * use these messages to assign the single or double buffer
   * (TextureClient/Host pairs) to the CompositableHost.
   * We expect the textures to already have been created.
   * With these messages, the ownership of the SurfaceDescriptor(s)
   * moves to the compositor.
   */
  virtual void CreatedSingleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aDescriptorOnWhite = nullptr) = 0;
  virtual void CreatedDoubleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aFrontDescriptor,
                                   const SurfaceDescriptor& aBackDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aFrontDescriptorOnWhite = nullptr,
                                   const SurfaceDescriptor* aBackDescriptorOnWhite = nullptr) = 0;

  /**
   * Notify the CompositableHost that it should create host-side-only
   * texture(s), that we will update incrementally using UpdateTextureIncremental.
   */
  virtual void CreatedIncrementalBuffer(CompositableClient* aCompositable,
                                        const TextureInfo& aTextureInfo,
                                        const nsIntRect& aBufferRect) = 0;

  /**
   * Tell the compositor that a Compositable is killing its buffer(s),
   * that is TextureClient/Hosts.
   */
  virtual void DestroyThebesBuffer(CompositableClient* aCompositable) = 0;

  virtual void PaintedTiledLayerBuffer(CompositableClient* aCompositable,
                                       const SurfaceDescriptorTiles& aTiledDescriptor) = 0;

  /**
   * Create a TextureChild/Parent pair as as well as the TextureHost on the parent side.
   */
  virtual PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData, TextureFlags aFlags) = 0;

  /**
   * Communicate to the compositor that the texture identified by aCompositable
   * and aTextureId has been updated to aImage.
   */
  virtual void UpdateTexture(CompositableClient* aCompositable,
                             TextureIdentifier aTextureId,
                             SurfaceDescriptor* aDescriptor) = 0;

  /**
   * Same as UpdateTexture, but performs an asynchronous layer transaction (if possible)
   */
  virtual void UpdateTextureNoSwap(CompositableClient* aCompositable,
                                   TextureIdentifier aTextureId,
                                   SurfaceDescriptor* aDescriptor) = 0;

  /**
   * Communicate to the compositor that aRegion in the texture identified by
   * aCompositable and aIdentifier has been updated to aThebesBuffer.
   */
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) = 0;

  /**
   * Notify the compositor to update aTextureId using aDescriptor, and take
   * ownership of aDescriptor.
   *
   * aDescriptor only contains the pixels for aUpdatedRegion, and is relative
   * to aUpdatedRegion.TopLeft().
   *
   * aBufferRect/aBufferRotation define the new valid region contained
   * within the texture after the update has been applied.
   */
  virtual void UpdateTextureIncremental(CompositableClient* aCompositable,
                                        TextureIdentifier aTextureId,
                                        SurfaceDescriptor& aDescriptor,
                                        const nsIntRegion& aUpdatedRegion,
                                        const nsIntRect& aBufferRect,
                                        const nsIntPoint& aBufferRotation) = 0;

  /**
   * Communicate the picture rect of a YUV image in aLayer to the compositor
   */
  virtual void UpdatePictureRect(CompositableClient* aCompositable,
                                 const nsIntRect& aRect) = 0;

  /**
   * The specified layer is destroying its buffers.
   * |aBackBufferToDestroy| is deallocated when this transaction is
   * posted to the parent.  During the parent-side transaction, the
   * shadow is told to destroy its front buffer.  This can happen when
   * a new front/back buffer pair have been created because of a layer
   * resize, e.g.
   */
  virtual void DestroyedThebesBuffer(const SurfaceDescriptor& aBackBufferToDestroy) = 0;

  /**
   * Tell the compositor side to delete the TextureHost corresponding to the
   * TextureClient passed in parameter.
   */
  virtual void RemoveTexture(TextureClient* aTexture) = 0;

  /**
   * Forcibly remove texture data from TextureClient
   * after a tansaction with Compositor.
   */
  virtual void AddForceRemovingTexture(TextureClient* aClient)
  {
    if (aClient) {
      mForceRemovingTextures.AppendElement(aClient);
    }
  }

  /**
   * Forcibly remove texture data from TextureClient
   * This function needs to be called after a tansaction with Compositor.
   */
  virtual void ForceRemoveTexturesIfNecessary()
  {
    for (uint32_t i = 0; i < mForceRemovingTextures.Length(); i++) {
       mForceRemovingTextures[i]->ForceRemove();
    }
    mForceRemovingTextures.Clear();
  }

  /**
   * Tell the CompositableHost on the compositor side what texture to use for
   * the next composition.
   */
  virtual void UseTexture(CompositableClient* aCompositable,
                          TextureClient* aClient) = 0;

  /**
   * Tell the compositor side that the shared data has been modified so that
   * it can react accordingly (upload textures, etc.).
   */
  virtual void UpdatedTexture(CompositableClient* aCompositable,
                              TextureClient* aTexture,
                              nsIntRegion* aRegion) = 0;

  void IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier);

  /**
   * Returns the maximum texture size supported by the compositor.
   */
  virtual int32_t GetMaxTextureSize() const { return mTextureFactoryIdentifier.mMaxTextureSize; }

  bool IsOnCompositorSide() const MOZ_OVERRIDE { return false; }

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   */
  LayersBackend GetCompositorBackendType() const
  {
    return mTextureFactoryIdentifier.mParentBackend;
  }

  bool SupportsTextureBlitting() const
  {
    return mTextureFactoryIdentifier.mSupportsTextureBlitting;
  }

  bool SupportsPartialUploads() const
  {
    return mTextureFactoryIdentifier.mSupportsPartialUploads;
  }

  bool ForwardsToDifferentProcess() const
  {
    return mMultiProcess;
  }

  const TextureFactoryIdentifier& GetTextureFactoryIdentifier() const
  {
    return mTextureFactoryIdentifier;
  }

protected:
  TextureFactoryIdentifier mTextureFactoryIdentifier;
  bool mMultiProcess;
  nsTArray<RefPtr<TextureClient> > mForceRemovingTextures;
};

} // namespace
} // namespace

#endif
