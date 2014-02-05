/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayers.h"
#include <set>                          // for _Rb_tree_const_iterator, etc
#include <vector>                       // for vector
#include "AutoOpenSurface.h"            // for AutoOpenSurface, etc
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ISurfaceAllocator.h"          // for IsSurfaceDescriptorValid
#include "Layers.h"                     // for Layer
#include "RenderTrace.h"                // for RenderTraceScope
#include "ShadowLayerChild.h"           // for ShadowLayerChild
#include "gfx2DGlue.h"                  // for Moz2D transition helpers
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPlatform.h"                // for gfxImageFormat, gfxPlatform
#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "ipc/IPCMessageUtils.h"        // for gfxContentType, null_t
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient, etc
#include "mozilla/layers/LayersMessages.h"  // for Edit, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/LayerTransactionChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsDebug.h"                    // for NS_ABORT_IF_FALSE, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc

struct nsIntPoint;

using namespace mozilla::ipc;
using namespace mozilla::gl;
using namespace mozilla::dom;

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

class BasicTiledLayerBuffer;

typedef nsTArray<SurfaceDescriptor> BufferArray;
typedef std::vector<Edit> EditVector;
typedef std::set<ShadowableLayer*> ShadowableLayerSet;

class Transaction
{
public:
  Transaction()
    : mTargetRotation(ROTATION_0)
    , mSwapRequired(false)
    , mOpen(false)
    , mRotationChanged(false)
  {}

  void Begin(const nsIntRect& aTargetBounds, ScreenRotation aRotation,
             const nsIntRect& aClientBounds, ScreenOrientation aOrientation)
  {
    mOpen = true;
    mTargetBounds = aTargetBounds;
    if (aRotation != mTargetRotation) {
      // the first time this is called, mRotationChanged will be false if
      // aRotation is 0, but we should be OK because for the first transaction
      // we should only compose if it is non-empty. See the caller(s) of
      // RotationChanged.
      mRotationChanged = true;
    }
    mTargetRotation = aRotation;
    mClientBounds = aClientBounds;
    mTargetOrientation = aOrientation;
  }
  void MarkSyncTransaction()
  {
    mSwapRequired = true;
  }
  void AddEdit(const Edit& aEdit)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mCset.push_back(aEdit);
  }
  void AddEdit(const CompositableOperation& aEdit)
  {
    AddEdit(Edit(aEdit));
  }
  void AddPaint(const Edit& aPaint)
  {
    AddNoSwapPaint(aPaint);
    mSwapRequired = true;
  }
  void AddPaint(const CompositableOperation& aPaint)
  {
    AddNoSwapPaint(Edit(aPaint));
    mSwapRequired = true;
  }

  void AddNoSwapPaint(const Edit& aPaint)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(aPaint);
  }
  void AddNoSwapPaint(const CompositableOperation& aPaint)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(Edit(aPaint));
  }
  void AddMutant(ShadowableLayer* aLayer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mMutants.insert(aLayer);
  }
  void AddBufferToDestroy(gfxSharedImageSurface* aBuffer)
  {
    return AddBufferToDestroy(aBuffer->GetShmem());
  }
  void AddBufferToDestroy(const SurfaceDescriptor& aBuffer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mDyingBuffers.AppendElement(aBuffer);
  }

  void End()
  {
    mCset.clear();
    mPaints.clear();
    mDyingBuffers.Clear();
    mMutants.clear();
    mOpen = false;
    mSwapRequired = false;
    mRotationChanged = false;
  }

  bool Empty() const {
    return mCset.empty() && mPaints.empty() && mMutants.empty();
  }
  bool RotationChanged() const {
    return mRotationChanged;
  }
  bool Finished() const { return !mOpen && Empty(); }

  EditVector mCset;
  EditVector mPaints;
  BufferArray mDyingBuffers;
  ShadowableLayerSet mMutants;
  nsIntRect mTargetBounds;
  ScreenRotation mTargetRotation;
  nsIntRect mClientBounds;
  ScreenOrientation mTargetOrientation;
  bool mSwapRequired;

private:
  bool mOpen;
  bool mRotationChanged;

  // disabled
  Transaction(const Transaction&);
  Transaction& operator=(const Transaction&);
};
struct AutoTxnEnd {
  AutoTxnEnd(Transaction* aTxn) : mTxn(aTxn) {}
  ~AutoTxnEnd() { mTxn->End(); }
  Transaction* mTxn;
};

void
CompositableForwarder::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  mTextureFactoryIdentifier = aIdentifier;
  mMultiProcess = aIdentifier.mParentProcessId != XRE_GetProcessType();
}

ShadowLayerForwarder::ShadowLayerForwarder()
 : mDiagnosticTypes(DIAGNOSTIC_NONE)
 , mIsFirstPaint(false)
 , mWindowOverlayChanged(false)
{
  mTxn = new Transaction();
}

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  NS_ABORT_IF_FALSE(mTxn->Finished(), "unfinished transaction?");
  delete mTxn;
}

void
ShadowLayerForwarder::BeginTransaction(const nsIntRect& aTargetBounds,
                                       ScreenRotation aRotation,
                                       const nsIntRect& aClientBounds,
                                       ScreenOrientation aOrientation)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(mTxn->Finished(), "uncommitted txn?");
  mTxn->Begin(aTargetBounds, aRotation, aClientBounds, aOrientation);
}

static PLayerChild*
Shadow(ShadowableLayer* aLayer)
{
  return aLayer->GetShadow();
}

template<typename OpCreateT>
static void
CreatedLayer(Transaction* aTxn, ShadowableLayer* aLayer)
{
  aTxn->AddEdit(OpCreateT(nullptr, Shadow(aLayer)));
}

void
ShadowLayerForwarder::CreatedThebesLayer(ShadowableLayer* aThebes)
{
  CreatedLayer<OpCreateThebesLayer>(mTxn, aThebes);
}
void
ShadowLayerForwarder::CreatedContainerLayer(ShadowableLayer* aContainer)
{
  CreatedLayer<OpCreateContainerLayer>(mTxn, aContainer);
}
void
ShadowLayerForwarder::CreatedImageLayer(ShadowableLayer* aImage)
{
  CreatedLayer<OpCreateImageLayer>(mTxn, aImage);
}
void
ShadowLayerForwarder::CreatedColorLayer(ShadowableLayer* aColor)
{
  CreatedLayer<OpCreateColorLayer>(mTxn, aColor);
}
void
ShadowLayerForwarder::CreatedCanvasLayer(ShadowableLayer* aCanvas)
{
  CreatedLayer<OpCreateCanvasLayer>(mTxn, aCanvas);
}
void
ShadowLayerForwarder::CreatedRefLayer(ShadowableLayer* aRef)
{
  CreatedLayer<OpCreateRefLayer>(mTxn, aRef);
}

void
ShadowLayerForwarder::DestroyedThebesBuffer(const SurfaceDescriptor& aBackBufferToDestroy)
{
  mTxn->AddBufferToDestroy(aBackBufferToDestroy);
}

void
ShadowLayerForwarder::Mutated(ShadowableLayer* aMutant)
{
mTxn->AddMutant(aMutant);
}

void
ShadowLayerForwarder::SetRoot(ShadowableLayer* aRoot)
{
  mTxn->AddEdit(OpSetRoot(nullptr, Shadow(aRoot)));
}
void
ShadowLayerForwarder::InsertAfter(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild,
                                  ShadowableLayer* aAfter)
{
  if (aAfter)
    mTxn->AddEdit(OpInsertAfter(nullptr, Shadow(aContainer),
                                nullptr, Shadow(aChild),
                                nullptr, Shadow(aAfter)));
  else
    mTxn->AddEdit(OpAppendChild(nullptr, Shadow(aContainer),
                                nullptr, Shadow(aChild)));
}
void
ShadowLayerForwarder::RemoveChild(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild)
{
  MOZ_LAYERS_LOG(("[LayersForwarder] OpRemoveChild container=%p child=%p\n",
                  aContainer->AsLayer(), aChild->AsLayer()));

  mTxn->AddEdit(OpRemoveChild(nullptr, Shadow(aContainer),
                              nullptr, Shadow(aChild)));
}
void
ShadowLayerForwarder::RepositionChild(ShadowableLayer* aContainer,
                                      ShadowableLayer* aChild,
                                      ShadowableLayer* aAfter)
{
  if (aAfter) {
    MOZ_LAYERS_LOG(("[LayersForwarder] OpRepositionChild container=%p child=%p after=%p",
                   aContainer->AsLayer(), aChild->AsLayer(), aAfter->AsLayer()));
    mTxn->AddEdit(OpRepositionChild(nullptr, Shadow(aContainer),
                                    nullptr, Shadow(aChild),
                                    nullptr, Shadow(aAfter)));
  } else {
    MOZ_LAYERS_LOG(("[LayersForwarder] OpRaiseToTopChild container=%p child=%p",
                   aContainer->AsLayer(), aChild->AsLayer()));
    mTxn->AddEdit(OpRaiseToTopChild(nullptr, Shadow(aContainer),
                                    nullptr, Shadow(aChild)));
  }
}


#ifdef DEBUG
void
ShadowLayerForwarder::CheckSurfaceDescriptor(const SurfaceDescriptor* aDescriptor) const
{
  if (!aDescriptor) {
    return;
  }

  if (aDescriptor->type() == SurfaceDescriptor::TShmem) {
    const mozilla::ipc::Shmem& shmem = aDescriptor->get_Shmem();
    shmem.AssertInvariants();
    MOZ_ASSERT(mShadowManager &&
               mShadowManager->IsTrackingSharedMemory(shmem.mSegment));
  }
}
#endif

void
ShadowLayerForwarder::PaintedTiledLayerBuffer(CompositableClient* aCompositable,
                                              const SurfaceDescriptorTiles& aTileLayerDescriptor)
{
  mTxn->AddNoSwapPaint(OpPaintTiledLayerBuffer(nullptr, aCompositable->GetIPDLActor(),
                                               aTileLayerDescriptor));
}

void
ShadowLayerForwarder::UpdateTexture(CompositableClient* aCompositable,
                                    TextureIdentifier aTextureId,
                                    SurfaceDescriptor* aDescriptor)
{
  if (aDescriptor->type() != SurfaceDescriptor::T__None &&
      aDescriptor->type() != SurfaceDescriptor::Tnull_t) {
    CheckSurfaceDescriptor(aDescriptor);
    MOZ_ASSERT(aCompositable);
    MOZ_ASSERT(aCompositable->GetIPDLActor());
    mTxn->AddPaint(OpPaintTexture(nullptr, aCompositable->GetIPDLActor(), 1,
                                  SurfaceDescriptor(*aDescriptor)));
    *aDescriptor = SurfaceDescriptor();
  } else {
    NS_WARNING("Trying to send a null SurfaceDescriptor.");
  }
}

void
ShadowLayerForwarder::UpdateTextureNoSwap(CompositableClient* aCompositable,
                                          TextureIdentifier aTextureId,
                                          SurfaceDescriptor* aDescriptor)
{
  if (aDescriptor->type() != SurfaceDescriptor::T__None &&
      aDescriptor->type() != SurfaceDescriptor::Tnull_t) {
    CheckSurfaceDescriptor(aDescriptor);
    MOZ_ASSERT(aCompositable);
    MOZ_ASSERT(aCompositable->GetIPDLActor());
    mTxn->AddNoSwapPaint(OpPaintTexture(nullptr, aCompositable->GetIPDLActor(), 1,
                                        SurfaceDescriptor(*aDescriptor)));
    *aDescriptor = SurfaceDescriptor();
  } else {
    NS_WARNING("Trying to send a null SurfaceDescriptor.");
  }
}

void
ShadowLayerForwarder::UpdateTextureRegion(CompositableClient* aCompositable,
                                          const ThebesBufferData& aThebesBufferData,
                                          const nsIntRegion& aUpdatedRegion)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddPaint(OpPaintTextureRegion(nullptr, aCompositable->GetIPDLActor(),
                                      aThebesBufferData,
                                      aUpdatedRegion));
}

void
ShadowLayerForwarder::UpdateTextureIncremental(CompositableClient* aCompositable,
                                               TextureIdentifier aTextureId,
                                               SurfaceDescriptor& aDescriptor,
                                               const nsIntRegion& aUpdatedRegion,
                                               const nsIntRect& aBufferRect,
                                               const nsIntPoint& aBufferRotation)
{
  CheckSurfaceDescriptor(&aDescriptor);
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapPaint(OpPaintTextureIncremental(nullptr, aCompositable->GetIPDLActor(),
                                                 aTextureId,
                                                 aDescriptor,
                                                 aUpdatedRegion,
                                                 aBufferRect,
                                                 aBufferRotation));
}


void
ShadowLayerForwarder::UpdatePictureRect(CompositableClient* aCompositable,
                                        const nsIntRect& aRect)
{
  mTxn->AddNoSwapPaint(OpUpdatePictureRect(nullptr, aCompositable->GetIPDLActor(), aRect));
}

void
ShadowLayerForwarder::UpdatedTexture(CompositableClient* aCompositable,
                                     TextureClient* aTexture,
                                     nsIntRegion* aRegion)
{
  MaybeRegion region = aRegion ? MaybeRegion(*aRegion)
                               : MaybeRegion(null_t());
  if (aTexture->GetFlags() & TEXTURE_IMMEDIATE_UPLOAD) {
    mTxn->AddPaint(OpUpdateTexture(nullptr, aCompositable->GetIPDLActor(),
                                   nullptr, aTexture->GetIPDLActor(),
                                   region));
  } else {
    mTxn->AddNoSwapPaint(OpUpdateTexture(nullptr, aCompositable->GetIPDLActor(),
                                         nullptr, aTexture->GetIPDLActor(),
                                         region));
  }
}

void
ShadowLayerForwarder::UseTexture(CompositableClient* aCompositable,
                                 TextureClient* aTexture)
{
  mTxn->AddEdit(OpUseTexture(nullptr, aCompositable->GetIPDLActor(),
                             nullptr, aTexture->GetIPDLActor()));
}

void
ShadowLayerForwarder::RemoveTexture(TextureClient* aTexture)
{
  aTexture->ForceRemove();
}

bool
ShadowLayerForwarder::EndTransaction(InfallibleTArray<EditReply>* aReplies, bool aScheduleComposite, bool* aSent)
{
  *aSent = false;

  PROFILER_LABEL("ShadowLayerForwarder", "EndTranscation");
  RenderTraceScope rendertrace("Foward Transaction", "000091");
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(!mTxn->Finished(), "forgot BeginTransaction?");

  DiagnosticTypes diagnostics = gfxPlatform::GetPlatform()->GetLayerDiagnosticTypes();
  if (mDiagnosticTypes != diagnostics) {
    mDiagnosticTypes = diagnostics;
    mTxn->AddEdit(OpSetDiagnosticTypes(diagnostics));
  }

  AutoTxnEnd _(mTxn);

  if (mTxn->Empty() && !mTxn->RotationChanged() && !mWindowOverlayChanged) {
    MOZ_LAYERS_LOG(("[LayersForwarder] 0-length cset (?) and no rotation event, skipping Update()"));
    return true;
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] destroying buffers..."));

  for (uint32_t i = 0; i < mTxn->mDyingBuffers.Length(); ++i) {
    DestroySharedSurface(&mTxn->mDyingBuffers[i]);
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] building transaction..."));

  // We purposely add attribute-change ops to the final changeset
  // before we add paint ops.  This allows layers to record the
  // attribute changes before new pixels arrive, which can be useful
  // for setting up back/front buffers.
  RenderTraceScope rendertrace2("Foward Transaction", "000092");
  for (ShadowableLayerSet::const_iterator it = mTxn->mMutants.begin();
       it != mTxn->mMutants.end(); ++it) {
    ShadowableLayer* shadow = *it;
    Layer* mutant = shadow->AsLayer();
    NS_ABORT_IF_FALSE(!!mutant, "unshadowable layer?");

    LayerAttributes attrs;
    CommonLayerAttributes& common = attrs.common();
    common.visibleRegion() = mutant->GetVisibleRegion();
    common.eventRegions() = mutant->GetEventRegions();
    common.postXScale() = mutant->GetPostXScale();
    common.postYScale() = mutant->GetPostYScale();
    common.transform() = mutant->GetBaseTransform();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : nsIntRect());
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    common.fixedPositionAnchor() = mutant->GetFixedPositionAnchor();
    common.fixedPositionMargin() = mutant->GetFixedPositionMargins();
    common.isStickyPosition() = mutant->GetIsStickyPosition();
    if (mutant->GetIsStickyPosition()) {
      common.stickyScrollContainerId() = mutant->GetStickyScrollContainerId();
      common.stickyScrollRangeOuter() = mutant->GetStickyScrollRangeOuter();
      common.stickyScrollRangeInner() = mutant->GetStickyScrollRangeInner();
    }
    common.scrollbarTargetContainerId() = mutant->GetScrollbarTargetContainerId();
    common.scrollbarDirection() = mutant->GetScrollbarDirection();
    if (Layer* maskLayer = mutant->GetMaskLayer()) {
      common.maskLayerChild() = Shadow(maskLayer->AsShadowableLayer());
    } else {
      common.maskLayerChild() = nullptr;
    }
    common.maskLayerParent() = nullptr;
    common.animations() = mutant->GetAnimations();
    common.invalidRegion() = mutant->GetInvalidRegion();
    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    MOZ_LAYERS_LOG(("[LayersForwarder] OpSetLayerAttributes(%p)\n", mutant));

    mTxn->AddEdit(OpSetLayerAttributes(nullptr, Shadow(shadow), attrs));
  }

  AutoInfallibleTArray<Edit, 10> cset;
  size_t nCsets = mTxn->mCset.size() + mTxn->mPaints.size();
  NS_ABORT_IF_FALSE(nCsets > 0 || mWindowOverlayChanged, "should have bailed by now");

  cset.SetCapacity(nCsets);
  if (!mTxn->mCset.empty()) {
    cset.AppendElements(&mTxn->mCset.front(), mTxn->mCset.size());
  }
  // Paints after non-paint ops, including attribute changes.  See
  // above.
  if (!mTxn->mPaints.empty()) {
    cset.AppendElements(&mTxn->mPaints.front(), mTxn->mPaints.size());
  }

  mWindowOverlayChanged = false;

  TargetConfig targetConfig(mTxn->mTargetBounds, mTxn->mTargetRotation, mTxn->mClientBounds, mTxn->mTargetOrientation);

  MOZ_LAYERS_LOG(("[LayersForwarder] syncing before send..."));
  PlatformSyncBeforeUpdate();

  profiler_tracing("Paint", "Rasterize", TRACING_INTERVAL_END);
  if (mTxn->mSwapRequired) {
    MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));
    RenderTraceScope rendertrace3("Forward Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->SendUpdate(cset, targetConfig, mIsFirstPaint,
                                    aScheduleComposite, aReplies)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    MOZ_LAYERS_LOG(("[LayersForwarder] sending no swap transaction..."));
    RenderTraceScope rendertrace3("Forward NoSwap Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->SendUpdateNoSwap(cset, targetConfig, mIsFirstPaint, aScheduleComposite)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  }

  *aSent = true;
  mIsFirstPaint = false;
  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return true;
}

bool
ShadowLayerForwarder::AllocShmem(size_t aSize,
                                 ipc::SharedMemory::SharedMemoryType aType,
                                 ipc::Shmem* aShmem)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  return mShadowManager->AllocShmem(aSize, aType, aShmem);
}
bool
ShadowLayerForwarder::AllocUnsafeShmem(size_t aSize,
                                          ipc::SharedMemory::SharedMemoryType aType,
                                          ipc::Shmem* aShmem)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  return mShadowManager->AllocUnsafeShmem(aSize, aType, aShmem);
}
void
ShadowLayerForwarder::DeallocShmem(ipc::Shmem& aShmem)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  mShadowManager->DeallocShmem(aShmem);
}

bool
ShadowLayerForwarder::IPCOpen() const
{
  return mShadowManager->IPCOpen();
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::OpenDescriptor(OpenMode aMode,
                                     const SurfaceDescriptor& aSurface)
{
  nsRefPtr<gfxASurface> surf = PlatformOpenDescriptor(aMode, aSurface);
  if (surf) {
    return surf.forget();
  }

  switch (aSurface.type()) {
  case SurfaceDescriptor::TShmem: {
    surf = gfxSharedImageSurface::Open(aSurface.get_Shmem());
    return surf.forget();
  } case SurfaceDescriptor::TRGBImage: {
    const RGBImage& rgb = aSurface.get_RGBImage();
    gfxImageFormat rgbFormat
      = static_cast<gfxImageFormat>(rgb.rgbFormat());
    uint32_t stride = gfxASurface::BytesPerPixel(rgbFormat) * rgb.picture().width;
    nsIntSize size(rgb.picture().width, rgb.picture().height);
    surf = new gfxImageSurface(rgb.data().get<uint8_t>(),
                               size,
                               stride,
                               rgbFormat);
    return surf.forget();
  }
  case SurfaceDescriptor::TMemoryImage: {
    const MemoryImage& image = aSurface.get_MemoryImage();
    gfxImageFormat format
      = static_cast<gfxImageFormat>(image.format());
    surf = new gfxImageSurface((unsigned char *)image.data(),
                               gfx::ThebesIntSize(image.size()),
                               image.stride(),
                               format);
    return surf.forget();
  }
  default:
    NS_ERROR("unexpected SurfaceDescriptor type!");
    return nullptr;
  }
}

/*static*/ gfxContentType
ShadowLayerForwarder::GetDescriptorSurfaceContentType(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxASurface** aSurface)
{
  gfxContentType content;
  if (PlatformGetDescriptorSurfaceContentType(aDescriptor, aMode,
                                              &content, aSurface)) {
    return content;
  }

  nsRefPtr<gfxASurface> surface = OpenDescriptor(aMode, aDescriptor);
  content = surface->GetContentType();
  *aSurface = surface.forget().get();
  return content;
}

/*static*/ gfx::IntSize
ShadowLayerForwarder::GetDescriptorSurfaceSize(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxASurface** aSurface)
{
  gfx::IntSize size;
  if (PlatformGetDescriptorSurfaceSize(aDescriptor, aMode, &size, aSurface)) {
    return size;
  }

  nsRefPtr<gfxASurface> surface = OpenDescriptor(aMode, aDescriptor);
  size = surface->GetSize().ToIntSize();
  surface.forget(aSurface);
  return size;
}

/*static*/ gfxImageFormat
ShadowLayerForwarder::GetDescriptorSurfaceImageFormat(
  const SurfaceDescriptor& aDescriptor, OpenMode aMode,
  gfxASurface** aSurface)
{
  gfxImageFormat format;
  if (PlatformGetDescriptorSurfaceImageFormat(aDescriptor, aMode, &format, aSurface)) {
    return format;
  }

  nsRefPtr<gfxASurface> surface = OpenDescriptor(aMode, aDescriptor);
  NS_ENSURE_TRUE(surface, gfxImageFormat::Unknown);

  nsRefPtr<gfxImageSurface> img = surface->GetAsImageSurface();
  NS_ENSURE_TRUE(img, gfxImageFormat::Unknown);

  format = img->Format();
  NS_ASSERTION(format != gfxImageFormat::Unknown,
               "ImageSurface RGB format should be known");

  *aSurface = surface.forget().get();
  return format;
}

/*static*/ void
ShadowLayerForwarder::CloseDescriptor(const SurfaceDescriptor& aDescriptor)
{
  PlatformCloseDescriptor(aDescriptor);
  // There's no "close" needed for Shmem surfaces.
}

/**
  * We bail out when we have no shadow manager. That can happen when the
  * layer manager is created by the preallocated process.
  * See bug 914843 for details.
  */
PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  return mShadowManager->SendPLayerConstructor(new ShadowLayerChild(aLayer));
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(OpenMode,
                                             const SurfaceDescriptor&)
{
  return nullptr;
}

/*static*/ bool
ShadowLayerForwarder::PlatformCloseDescriptor(const SurfaceDescriptor&)
{
  return false;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceContentType(
  const SurfaceDescriptor&,
  OpenMode,
  gfxContentType*,
  gfxASurface**)
{
  return false;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceSize(
  const SurfaceDescriptor&,
  OpenMode,
  gfx::IntSize*,
  gfxASurface**)
{
  return false;
}

/*static*/ bool
ShadowLayerForwarder::PlatformGetDescriptorSurfaceImageFormat(
  const SurfaceDescriptor&,
  OpenMode,
  gfxImageFormat*,
  gfxASurface**)
{
  return false;
}

bool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return false;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

bool
ISurfaceAllocator::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return false;
}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

AutoOpenSurface::AutoOpenSurface(OpenMode aMode,
                                 const SurfaceDescriptor& aDescriptor)
  : mDescriptor(aDescriptor)
  , mMode(aMode)
{
  MOZ_ASSERT(IsSurfaceDescriptorValid(mDescriptor));
}

AutoOpenSurface::~AutoOpenSurface()
{
  if (mSurface) {
    mSurface = nullptr;
    ShadowLayerForwarder::CloseDescriptor(mDescriptor);
  }
}

gfxContentType
AutoOpenSurface::ContentType()
{
  if (mSurface) {
    return mSurface->GetContentType();
  }
  return ShadowLayerForwarder::GetDescriptorSurfaceContentType(
    mDescriptor, mMode, getter_AddRefs(mSurface));
}

gfxImageFormat
AutoOpenSurface::ImageFormat()
{
  if (mSurface) {
    nsRefPtr<gfxImageSurface> img = mSurface->GetAsImageSurface();
    if (img) {
      gfxImageFormat format = img->Format();
      NS_ASSERTION(format != gfxImageFormat::Unknown,
                   "ImageSurface RGB format should be known");

      return format;
    }
  }

  return ShadowLayerForwarder::GetDescriptorSurfaceImageFormat(
    mDescriptor, mMode, getter_AddRefs(mSurface));
}

gfx::IntSize
AutoOpenSurface::Size()
{
  if (mSurface) {
    return mSurface->GetSize().ToIntSize();
  }
  return ShadowLayerForwarder::GetDescriptorSurfaceSize(
    mDescriptor, mMode, getter_AddRefs(mSurface));
}

gfxASurface*
AutoOpenSurface::Get()
{
  if (!mSurface) {
    mSurface = ShadowLayerForwarder::OpenDescriptor(mMode, mDescriptor);
  }
  return mSurface.get();
}

gfxImageSurface*
AutoOpenSurface::GetAsImage()
{
  if (!mSurfaceAsImage) {
    mSurfaceAsImage = Get()->GetAsImageSurface();
  }
  return mSurfaceAsImage.get();
}

void
ShadowLayerForwarder::Connect(CompositableClient* aCompositable)
{
#ifdef GFX_COMPOSITOR_LOGGING
  printf("ShadowLayerForwarder::Connect(Compositable)\n");
#endif
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(mShadowManager);
  CompositableChild* child = static_cast<CompositableChild*>(
    mShadowManager->SendPCompositableConstructor(aCompositable->GetTextureInfo()));
  MOZ_ASSERT(child);
  aCompositable->SetIPDLActor(child);
  child->SetClient(aCompositable);
}

void
ShadowLayerForwarder::CreatedSingleBuffer(CompositableClient* aCompositable,
                                          const SurfaceDescriptor& aDescriptor,
                                          const TextureInfo& aTextureInfo,
                                          const SurfaceDescriptor* aDescriptorOnWhite)
{
  CheckSurfaceDescriptor(&aDescriptor);
  CheckSurfaceDescriptor(aDescriptorOnWhite);

  MOZ_ASSERT(aDescriptor.type() != SurfaceDescriptor::T__None &&
             aDescriptor.type() != SurfaceDescriptor::Tnull_t);
  mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                 TextureFront,
                                 aDescriptor,
                                 aTextureInfo));
  if (aDescriptorOnWhite) {
    mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                   TextureOnWhiteFront,
                                   *aDescriptorOnWhite,
                                   aTextureInfo));
  }
}

void
ShadowLayerForwarder::CreatedIncrementalBuffer(CompositableClient* aCompositable,
                                               const TextureInfo& aTextureInfo,
                                               const nsIntRect& aBufferRect)
{
  mTxn->AddNoSwapPaint(OpCreatedIncrementalTexture(nullptr, aCompositable->GetIPDLActor(),
                                                   aTextureInfo, aBufferRect));
}

void
ShadowLayerForwarder::CreatedDoubleBuffer(CompositableClient* aCompositable,
                                          const SurfaceDescriptor& aFrontDescriptor,
                                          const SurfaceDescriptor& aBackDescriptor,
                                          const TextureInfo& aTextureInfo,
                                          const SurfaceDescriptor* aFrontDescriptorOnWhite,
                                          const SurfaceDescriptor* aBackDescriptorOnWhite)
{
  CheckSurfaceDescriptor(&aFrontDescriptor);
  CheckSurfaceDescriptor(&aBackDescriptor);
  CheckSurfaceDescriptor(aFrontDescriptorOnWhite);
  CheckSurfaceDescriptor(aBackDescriptorOnWhite);
  MOZ_ASSERT(aFrontDescriptor.type() != SurfaceDescriptor::T__None &&
             aBackDescriptor.type() != SurfaceDescriptor::T__None &&
             aFrontDescriptor.type() != SurfaceDescriptor::Tnull_t &&
             aBackDescriptor.type() != SurfaceDescriptor::Tnull_t);
  mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                 TextureFront,
                                 aFrontDescriptor,
                                 aTextureInfo));
  mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                 TextureBack,
                                 aBackDescriptor,
                                 aTextureInfo));
  if (aFrontDescriptorOnWhite) {
    MOZ_ASSERT(aBackDescriptorOnWhite);
    mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                   TextureOnWhiteFront,
                                   *aFrontDescriptorOnWhite,
                                   aTextureInfo));
    mTxn->AddEdit(OpCreatedTexture(nullptr, aCompositable->GetIPDLActor(),
                                   TextureOnWhiteBack,
                                   *aBackDescriptorOnWhite,
                                   aTextureInfo));
  }
}

void
ShadowLayerForwarder::DestroyThebesBuffer(CompositableClient* aCompositable)
{
  mTxn->AddEdit(OpDestroyThebesBuffer(nullptr, aCompositable->GetIPDLActor()));
}

void ShadowLayerForwarder::Attach(CompositableClient* aCompositable,
                                  ShadowableLayer* aLayer)
{
  MOZ_ASSERT(aLayer);
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddEdit(OpAttachCompositable(nullptr, Shadow(aLayer),
                                     nullptr, aCompositable->GetIPDLActor()));
}

void ShadowLayerForwarder::AttachAsyncCompositable(uint64_t aCompositableID,
                                                   ShadowableLayer* aLayer)
{
  MOZ_ASSERT(aLayer);
  MOZ_ASSERT(aCompositableID != 0); // zero is always an invalid compositable id.
  mTxn->AddEdit(OpAttachAsyncCompositable(nullptr, Shadow(aLayer),
                                          aCompositableID));
}

PTextureChild*
ShadowLayerForwarder::CreateTexture(const SurfaceDescriptor& aSharedData,
                                    TextureFlags aFlags)
{
  return mShadowManager->SendPTextureConstructor(aSharedData, aFlags);
}


void ShadowLayerForwarder::SetShadowManager(PLayerTransactionChild* aShadowManager)
{
  mShadowManager = static_cast<LayerTransactionChild*>(aShadowManager);
}


} // namespace layers
} // namespace mozilla
