/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_DMABUF_FORMATS_H__
#define __MOZ_DMABUF_FORMATS_H__

#include "nsTArray.h"

#ifdef MOZ_WAYLAND
struct zwp_linux_dmabuf_v1;
struct zwp_linux_dmabuf_feedback_v1;
struct wl_surface;
#endif

namespace mozilla::widget {

class DMABufFormatTable;
class DMABufFeedback;

// DRMFormat (fourcc) and available modifiers for it.
// Modifiers are sorted from the most preffered one.
class DRMFormat final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DRMFormat)

  explicit DRMFormat(uint32_t aFormat) : mFormat(aFormat) {};
  DRMFormat(uint32_t aFormat, uint64_t aModifier) : mFormat(aFormat) {
    mModifiers.AppendElement(aModifier);
  }
  DRMFormat(uint32_t aFormat, const nsTArray<uint64_t>& aModifiers)
      : mFormat(aFormat), mModifiers(aModifiers.Clone()) {};
  DRMFormat(const DRMFormat& aSrc)
      : mFormat(aSrc.mFormat), mModifiers(aSrc.mModifiers.Clone()) {}
  uint32_t GetFormat() const { return mFormat; }
  bool Matches(uint32_t aFormat) const { return mFormat == aFormat; }
  bool IsFormatModifierSupported(uint64_t aModifier) const {
    return mModifiers.Contains(aModifier);
  }
  void AddModifier(uint64_t aModifier) {
    MOZ_ASSERT(!IsFormatModifierSupported(aModifier), "Added modifier twice?");
    mModifiers.AppendElement(aModifier);
  }
  bool UseModifiers() const { return !mModifiers.IsEmpty(); }
  const uint64_t* GetModifiers(uint32_t& aModifiersNum) {
    aModifiersNum = mModifiers.Length();
    return mModifiers.Elements();
  }
  nsTArray<uint64_t>* GetModifiers() { return &mModifiers; }

 private:
  ~DRMFormat() = default;

  uint32_t mFormat = 0;
  AutoTArray<uint64_t, 15> mModifiers;
};

#ifdef MOZ_WAYLAND
class DMABufFormats;
using DMABufFormatsCallback = std::function<void(DMABufFormats*)>;

class DMABufFormats final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DMABufFormats)

  void InitFeedback(zwp_linux_dmabuf_v1* aDMABuf,
                    const DMABufFormatsCallback& aFormatRefreshCB,
                    wl_surface* aSurface = nullptr);
  void InitV3(zwp_linux_dmabuf_v1* aDMABuf);
  void InitV3Done();

  DMABufFeedback* GetDMABufFeedback() { return mDMABufFeedback.get(); }
  DMABufFeedback* GetPendingDMABufFeedback();
  void PendingDMABufFeedbackDone();

  DRMFormat* GetFormat(uint32_t aFormat, bool aRequestScanoutFormat = false);
  DMABufFormats();

 private:
  ~DMABufFormats();

  DMABufFormatsCallback mFormatRefreshCallback;
  zwp_linux_dmabuf_feedback_v1* mWaylandFeedback = nullptr;

  UniquePtr<DMABufFeedback> mDMABufFeedback;
  UniquePtr<DMABufFeedback> mPendingDMABufFeedback;
};

RefPtr<DMABufFormats> CreateDMABufFeedbackFormats(
    wl_surface* aSurface,
    const std::function<void(DMABufFormats*)>& aFormatRefreshCB = nullptr);

#endif

}  // namespace mozilla::widget

#endif  // __MOZ_DMABUF_FORMATS_H__
