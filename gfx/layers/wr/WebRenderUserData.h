/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERUSERDATA_H
#define GFX_WEBRENDERUSERDATA_H

#include <vector>
#include "BasicLayers.h"  // for BasicLayerManager
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/AnimationInfo.h"
#include "nsIFrame.h"

class nsDisplayItemGeometry;

namespace mozilla {
namespace wr {
class IpcResourceUpdateQueue;
}

namespace gfx {
class SourceSurface;
}

namespace layers {
class CanvasLayer;
class ImageClient;
class ImageContainer;
class WebRenderBridgeChild;
class WebRenderCanvasData;
class WebRenderCanvasRendererAsync;
class WebRenderImageData;
class WebRenderFallbackData;
class WebRenderLayerManager;
class WebRenderGroupData;

class WebRenderBackgroundData {
 public:
  WebRenderBackgroundData(wr::LayoutRect aBounds, wr::ColorF aColor)
      : mBounds(aBounds), mColor(aColor) {}
  void AddWebRenderCommands(wr::DisplayListBuilder& aBuilder);

 protected:
  wr::LayoutRect mBounds;
  wr::ColorF mColor;
};

class WebRenderUserData {
 public:
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>>
      WebRenderUserDataRefTable;

  static bool SupportsAsyncUpdate(nsIFrame* aFrame);

  static bool ProcessInvalidateForImage(nsIFrame* aFrame,
                                        DisplayItemType aType);

  NS_INLINE_DECL_REFCOUNTING(WebRenderUserData)

  WebRenderUserData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem);

  virtual WebRenderImageData* AsImageData() { return nullptr; }
  virtual WebRenderFallbackData* AsFallbackData() { return nullptr; }
  virtual WebRenderCanvasData* AsCanvasData() { return nullptr; }
  virtual WebRenderGroupData* AsGroupData() { return nullptr; }

  enum class UserDataType {
    eImage,
    eFallback,
    eAnimation,
    eCanvas,
    eGroup,
  };

  virtual UserDataType GetType() = 0;
  bool IsUsed() { return mUsed; }
  void SetUsed(bool aUsed) { mUsed = aUsed; }
  nsIFrame* GetFrame() { return mFrame; }
  uint32_t GetDisplayItemKey() { return mDisplayItemKey; }
  void RemoveFromTable();
  virtual nsDisplayItemGeometry* GetGeometry() { return nullptr; }

 protected:
  virtual ~WebRenderUserData();

  WebRenderBridgeChild* WrBridge() const;

  RefPtr<WebRenderLayerManager> mWRManager;
  nsIFrame* mFrame;
  uint32_t mDisplayItemKey;
  WebRenderUserDataRefTable* mTable;
  bool mUsed;
};

struct WebRenderUserDataKey {
  WebRenderUserDataKey(uint32_t aFrameKey,
                       WebRenderUserData::UserDataType aType)
      : mFrameKey(aFrameKey), mType(aType) {}

  bool operator==(const WebRenderUserDataKey& other) const {
    return mFrameKey == other.mFrameKey && mType == other.mType;
  }
  PLDHashNumber Hash() const {
    return HashGeneric(
        mFrameKey,
        static_cast<std::underlying_type<decltype(mType)>::type>(mType));
  }

  uint32_t mFrameKey;
  WebRenderUserData::UserDataType mType;
};

typedef nsRefPtrHashtable<
    nsGenericHashKey<mozilla::layers::WebRenderUserDataKey>, WebRenderUserData>
    WebRenderUserDataTable;

/// Holds some data used to share TextureClient/ImageClient with the parent
/// process except if used with blob images (watch your step).
class WebRenderImageData : public WebRenderUserData {
 public:
  WebRenderImageData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem);
  virtual ~WebRenderImageData();

  virtual WebRenderImageData* AsImageData() override { return this; }
  virtual UserDataType GetType() override { return UserDataType::eImage; }
  static UserDataType Type() { return UserDataType::eImage; }
  virtual Maybe<wr::ImageKey> GetImageKey() { return mKey; }
  void SetImageKey(const wr::ImageKey& aKey);
  already_AddRefed<ImageClient> GetImageClient();

  Maybe<wr::ImageKey> UpdateImageKey(ImageContainer* aContainer,
                                     wr::IpcResourceUpdateQueue& aResources,
                                     bool aFallback = false);

  void CreateAsyncImageWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder, ImageContainer* aContainer,
      const StackingContextHelper& aSc, const LayoutDeviceRect& aBounds,
      const LayoutDeviceRect& aSCBounds, const gfx::Matrix4x4& aSCTransform,
      const gfx::MaybeIntSize& aScaleToSize, const wr::ImageRendering& aFilter,
      const wr::MixBlendMode& aMixBlendMode, bool aIsBackfaceVisible);

  void CreateImageClientIfNeeded();

  bool IsAsync() { return mPipelineId.isSome(); }

  bool IsAsyncAnimatedImage() const;

 protected:
  virtual void ClearImageKey();

  RefPtr<TextureClient> mTextureOfImage;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  Maybe<wr::PipelineId> mPipelineId;
  RefPtr<ImageContainer> mContainer;
  bool mOwnsKey;
};

/// Used for fallback rendering.
///
/// In most cases this uses blob images but it can also render on the content
/// side directly into a texture.
///
/// TODO(nical) It would be much better to separate the two use cases into
/// separate classes and not have the blob image related code inherit from
/// WebRenderImageData (the current code only works if we carefully use a subset
/// of the parent code).
class WebRenderFallbackData : public WebRenderImageData {
 public:
  WebRenderFallbackData(WebRenderLayerManager* aWRManager,
                        nsDisplayItem* aItem);
  virtual ~WebRenderFallbackData();

  virtual WebRenderFallbackData* AsFallbackData() override { return this; }
  virtual UserDataType GetType() override { return UserDataType::eFallback; }
  static UserDataType Type() { return UserDataType::eFallback; }
  nsDisplayItemGeometry* GetGeometry() override;
  void SetGeometry(nsAutoPtr<nsDisplayItemGeometry> aGeometry);
  nsRect GetBounds() { return mBounds; }
  void SetBounds(const nsRect& aRect) { mBounds = aRect; }
  void SetInvalid(bool aInvalid) { mInvalid = aInvalid; }
  void SetScale(gfx::Size aScale) { mScale = aScale; }
  gfx::Size GetScale() { return mScale; }
  bool IsInvalid() { return mInvalid; }
  void SetFonts(const std::vector<RefPtr<gfx::ScaledFont>>& aFonts) {
    mFonts = aFonts;
  }
  Maybe<wr::BlobImageKey> GetBlobImageKey() { return mBlobKey; }
  virtual Maybe<wr::ImageKey> GetImageKey() override;
  void SetBlobImageKey(const wr::BlobImageKey& aKey);

  RefPtr<BasicLayerManager> mBasicLayerManager;
  std::vector<RefPtr<gfx::SourceSurface>> mExternalSurfaces;

 protected:
  virtual void ClearImageKey() override;

  Maybe<wr::BlobImageKey> mBlobKey;
  nsAutoPtr<nsDisplayItemGeometry> mGeometry;
  nsRect mBounds;
  bool mInvalid;
  gfx::Size mScale;
  std::vector<RefPtr<gfx::ScaledFont>> mFonts;
};

class WebRenderAnimationData : public WebRenderUserData {
 public:
  WebRenderAnimationData(WebRenderLayerManager* aWRManager,
                         nsDisplayItem* aItem);
  virtual ~WebRenderAnimationData();

  virtual UserDataType GetType() override { return UserDataType::eAnimation; }
  static UserDataType Type() { return UserDataType::eAnimation; }
  AnimationInfo& GetAnimationInfo() { return mAnimationInfo; }

 protected:
  AnimationInfo mAnimationInfo;
};

class WebRenderCanvasData : public WebRenderUserData {
 public:
  WebRenderCanvasData(WebRenderLayerManager* aWRManager, nsDisplayItem* aItem);
  virtual ~WebRenderCanvasData();

  virtual WebRenderCanvasData* AsCanvasData() override { return this; }
  virtual UserDataType GetType() override { return UserDataType::eCanvas; }
  static UserDataType Type() { return UserDataType::eCanvas; }

  void ClearCanvasRenderer();
  WebRenderCanvasRendererAsync* GetCanvasRenderer();
  WebRenderCanvasRendererAsync* CreateCanvasRenderer();

 protected:
  UniquePtr<WebRenderCanvasRendererAsync> mCanvasRenderer;
};

extern void DestroyWebRenderUserDataTable(WebRenderUserDataTable* aTable);

struct WebRenderUserDataProperty {
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(Key, WebRenderUserDataTable,
                                      DestroyWebRenderUserDataTable)
};

template <class T>
already_AddRefed<T> GetWebRenderUserData(const nsIFrame* aFrame,
                                         uint32_t aPerFrameKey) {
  MOZ_ASSERT(aFrame);
  WebRenderUserDataTable* userDataTable =
      aFrame->GetProperty(WebRenderUserDataProperty::Key());
  if (!userDataTable) {
    return nullptr;
  }

  WebRenderUserData* data =
      userDataTable->GetWeak(WebRenderUserDataKey(aPerFrameKey, T::Type()));
  if (data) {
    RefPtr<T> result = static_cast<T*>(data);
    return result.forget();
  }

  return nullptr;
}

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERUSERDATA_H */
