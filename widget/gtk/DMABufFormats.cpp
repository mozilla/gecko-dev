/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <xf86drm.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "DMABufDevice.h"
#include "DMABufFormats.h"
#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#  include "WidgetUtilsGtk.h"
#  include "mozilla/widget/mozwayland.h"
#  include "mozilla/widget/linux-dmabuf-unstable-v1-client-protocol.h"
#endif
#include <gbm.h>
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ClearOnShutdown.h"

#include "mozilla/gfx/Logging.h"  // for gfxCriticalNote

using namespace mozilla::gfx;

#ifndef GBM_FORMAT_P010
#  define GBM_FORMAT_P010 \
    __gbm_fourcc_code('P', '0', '1', '0') /* 2x2 subsampled Cr:Cb plane */
#endif

// TODO: Provide fallback formats if beedback is not received yet
// Get from display?

namespace mozilla::widget {

// Table of all supported DRM formats, every format is stored as
// FOURCC format + modifier pair and
// one FOURCC format can be stored with more modifiers.
// The format table data are provided via. mapped memory
// so we don't copy it, just map fd and read.
class DMABufFormatTable final {
 public:
  bool IsSet() { return mData && mSize && mData != MAP_FAILED; }

  void Set(int32_t aFd, uint32_t aSize) {
    MOZ_DIAGNOSTIC_ASSERT(!mData && !mSize);
    mSize = aSize;
    mData =
        (DRMFormatTableEntry*)mmap(NULL, aSize, PROT_READ, MAP_PRIVATE, aFd, 0);
    close(aFd);
  }

  bool GetFormat(uint16_t aIndex, uint32_t* aFormat, uint64_t* aModifier) {
    if (aIndex >= mSize / sizeof(DRMFormatTableEntry)) {
      gfxCriticalNote << "Wrong DRM DMABuf format index!";
      return false;
    }
    *aFormat = mData[aIndex].mFormat;
    *aModifier = mData[aIndex].mModifier;
    return true;
  }

  ~DMABufFormatTable() {
    if (mData && mData != MAP_FAILED) {
      munmap(mData, mSize);
    }
  }

 private:
  unsigned int mSize = 0;
  struct DRMFormatTableEntry {
    uint32_t mFormat;
    uint32_t padding; /* unused */
    uint64_t mModifier;
  }* mData = nullptr;
};

class DMABufFeedbackTranche final {
 public:
#ifdef MOZ_WAYLAND
  void SetFormats(DMABufFormatTable* aFormatTable, wl_array* aIndices) {
    // Formats are reported as array with appropriate modifiers.
    // Modifiers are sorted from the most preffered.
    // There's a sample output of weston-simple-dmabuf-feedback utility
    // which prints the format table:
    //
    // format ABGR16161616F, modifier
    // AMD_GFX10_RBPLUS,64KB_R_X,PIPE_XOR_BITS=3... format ABGR16161616F,
    // modifier AMD_GFX10,64KB_S_X,PIPE_XOR_BITS=3 format ABGR16161616F,
    // modifier AMD_GFX9,64KB_D format ABGR16161616F, modifier AMD_GFX9,64KB_S
    // format ABGR16161616F, modifier LINEAR

    RefPtr<DRMFormat> currentDrmFormat;

    // We need to use such ugly constructions because wl_array_for_each
    // is not C++ compliant
    // (https://gitlab.freedesktop.org/wayland/wayland/-/issues/34)
    uint16_t* index = (uint16_t*)aIndices->data;
    uint16_t* lastIndex =
        (uint16_t*)((const char*)aIndices->data + aIndices->size);

    for (; index < lastIndex; index++) {
      uint32_t format;
      uint64_t modifier;
      if (!aFormatTable->GetFormat(*index, &format, &modifier)) {
        return;
      }
      LOGDMABUF(("DMABufFeedbackTranche [%p] format 0x%x modifier %" PRIx64,
                 this, format, modifier));
      if (!currentDrmFormat || !currentDrmFormat->Matches(format)) {
        currentDrmFormat = new DRMFormat(format, modifier);
        mFormats.AppendElement(currentDrmFormat);
        continue;
      }
      currentDrmFormat->AddModifier(modifier);
    }
  }
#endif
  void SetScanout(bool aIsScanout) { mIsScanout = aIsScanout; }
  bool IsScanout() { return mIsScanout; }

  void AddFormat(uint32_t aFormat, uint64_t aModifier) {
    DRMFormat* format = GetFormat(aFormat);
    if (format) {
      format->AddModifier(aModifier);
      return;
    }
    mFormats.AppendElement(new DRMFormat(aFormat, aModifier));
  }

  DRMFormat* GetFormat(uint32_t aFormat) {
    for (const auto& format : mFormats) {
      if (format->Matches(aFormat)) {
        return format.get();
      }
    }
    return nullptr;
  }

 private:
  bool mIsScanout = false;
  nsTArray<RefPtr<DRMFormat>> mFormats;
};

class DMABufFeedback final {
 public:
  DMABufFormatTable* FormatTable() { return &mFormatTable; }
  DMABufFeedbackTranche* PendingTranche() {
    if (!mPendingTranche) {
      mPendingTranche = MakeUnique<DMABufFeedbackTranche>();
    }
    return mPendingTranche.get();
  }
  void PendingTrancheDone() {
    // It's possible that Wayland compositor doesn't send us any format,
    // so !mPendingTranche
    if (mPendingTranche) {
      mTranches.AppendElement(std::move(mPendingTranche));
    }
  }
  DRMFormat* GetFormat(uint32_t aFormat, bool aRequestScanoutFormat) {
    for (const auto& tranche : mTranches) {
      if (aRequestScanoutFormat && !tranche->IsScanout()) {
        continue;
      }
      if (DRMFormat* format = tranche->GetFormat(aFormat)) {
        return format;
      }
    }
    return nullptr;
  }

 private:
  DMABufFormatTable mFormatTable;
  UniquePtr<DMABufFeedbackTranche> mPendingTranche;
  nsTArray<UniquePtr<DMABufFeedbackTranche>> mTranches;
};

DMABufFeedback* DMABufFormats::GetPendingDMABufFeedback() {
  if (!mPendingDMABufFeedback) {
    mPendingDMABufFeedback = MakeUnique<DMABufFeedback>();
  }
  return mPendingDMABufFeedback.get();
}

void DMABufFormats::PendingDMABufFeedbackDone() {
  mDMABufFeedback = std::move(mPendingDMABufFeedback);
  if (mFormatRefreshCallback) {
    mFormatRefreshCallback(this);
  }
}

#ifdef MOZ_WAYLAND
static void dmabuf_feedback_format_table(
    void* data,
    struct zwp_linux_dmabuf_feedback_v1* zwp_linux_dmabuf_feedback_v1,
    int32_t fd, uint32_t size) {
  auto* dmabuf = static_cast<DMABufFormats*>(data);
  if (!dmabuf) {
    return;
  }
  DMABufFormatTable* formats =
      dmabuf->GetPendingDMABufFeedback()->FormatTable();
  formats->Set(fd, size);
}

static void dmabuf_feedback_tranche_target_device(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
    struct wl_array* dev) {
  // We're getting device from GL
}

static void dmabuf_feedback_tranche_formats(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
    struct wl_array* indices) {
  auto* dmabuf = static_cast<DMABufFormats*>(data);
  if (!dmabuf) {
    return;
  }
  DMABufFeedback* feedback = dmabuf->GetPendingDMABufFeedback();
  DMABufFormatTable* formatTable = feedback->FormatTable();
  if (!formatTable->IsSet()) {
    formatTable = dmabuf->GetDMABufFeedback()
                      ? dmabuf->GetDMABufFeedback()->FormatTable()
                      : nullptr;
    if (!formatTable->IsSet()) {
      gfxCriticalNote << "Missing DMABuf format table!";
      return;
    }
  }
  feedback->PendingTranche()->SetFormats(formatTable, indices);
}

static void dmabuf_feedback_tranche_flags(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
    uint32_t flags) {
  if (flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT) {
    auto* dmabuf = static_cast<DMABufFormats*>(data);
    if (!dmabuf) {
      return;
    }
    LOGDMABUF(("DMABufFeedbackTranche [%p] is scanout tranche",
               dmabuf->GetPendingDMABufFeedback()->PendingTranche()));
    dmabuf->GetPendingDMABufFeedback()->PendingTranche()->SetScanout(true);
  }
}

static void dmabuf_feedback_tranche_done(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
  auto* dmabuf = static_cast<DMABufFormats*>(data);
  if (!dmabuf) {
    return;
  }
  LOGDMABUF(("DMABufFeedbackTranche [%p] is done",
             dmabuf->GetPendingDMABufFeedback()));
  dmabuf->GetPendingDMABufFeedback()->PendingTrancheDone();
}

static void dmabuf_feedback_done(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback) {
  auto* dmabuf = static_cast<DMABufFormats*>(data);
  if (!dmabuf) {
    return;
  }
  dmabuf->PendingDMABufFeedbackDone();
}

static void dmabuf_feedback_main_device(
    void* data, struct zwp_linux_dmabuf_feedback_v1* dmabuf_feedback,
    struct wl_array* dev) {
  // We're getting DRM device from GL.
}

static const struct zwp_linux_dmabuf_feedback_v1_listener
    dmabuf_feedback_listener = {
        .done = dmabuf_feedback_done,
        .format_table = dmabuf_feedback_format_table,
        .main_device = dmabuf_feedback_main_device,
        .tranche_done = dmabuf_feedback_tranche_done,
        .tranche_target_device = dmabuf_feedback_tranche_target_device,
        .tranche_formats = dmabuf_feedback_tranche_formats,
        .tranche_flags = dmabuf_feedback_tranche_flags,
};

static void dmabuf_v3_modifiers(void* data,
                                struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                uint32_t format, uint32_t modifier_hi,
                                uint32_t modifier_lo) {
  // skip modifiers marked as invalid
  if (modifier_hi == (DRM_FORMAT_MOD_INVALID >> 32) &&
      modifier_lo == (DRM_FORMAT_MOD_INVALID & 0xffffffff)) {
    return;
  }
  auto* dmabuf = static_cast<DMABufFormats*>(data);
  if (!dmabuf) {
    return;
  }

  uint64_t modifier = ((uint64_t)modifier_hi << 32) | modifier_lo;
  LOGDMABUF(("DMABuf format 0x%x modifier %" PRIx64, format, modifier));

  dmabuf->GetPendingDMABufFeedback()->PendingTranche()->AddFormat(format,
                                                                  modifier);
}

static void dmabuf_v3_format(void* data,
                             struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                             uint32_t format) {
  // XXX: deprecated
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_v3_listener = {
    dmabuf_v3_format, dmabuf_v3_modifiers};

void DMABufFormats::InitFeedback(zwp_linux_dmabuf_v1* aDMABuf,
                                 const DMABufFormatsCallback& aFormatRefreshCB,
                                 wl_surface* aSurface) {
  LOGDMABUF(("DMABufFormats::Init() feedback wl_surface %p", aSurface));
  if (aSurface) {
    mWaylandFeedback =
        zwp_linux_dmabuf_v1_get_surface_feedback(aDMABuf, aSurface);
  } else {
    mWaylandFeedback = zwp_linux_dmabuf_v1_get_default_feedback(aDMABuf);
  }
  zwp_linux_dmabuf_feedback_v1_add_listener(mWaylandFeedback,
                                            &dmabuf_feedback_listener, this);
  mFormatRefreshCallback = aFormatRefreshCB;
}

void DMABufFormats::InitV3(zwp_linux_dmabuf_v1* aDMABuf) {
  LOGDMABUF(("DMABufFormats::Init() v.3"));
  zwp_linux_dmabuf_v1_add_listener(aDMABuf, &dmabuf_v3_listener, this);
}

void DMABufFormats::InitV3Done() {
  LOGDMABUF(("DMABufFormats::Init() v.3 Done"));
  GetPendingDMABufFeedback()->PendingTrancheDone();
  PendingDMABufFeedbackDone();
}
#endif

DRMFormat* DMABufFormats::GetFormat(uint32_t aFormat,
                                    bool aRequestScanoutFormat) {
  MOZ_DIAGNOSTIC_ASSERT(mDMABufFeedback);
  return mDMABufFeedback->GetFormat(aFormat, aRequestScanoutFormat);
}

void DMABufFormats::EnsureBasicFormats() {
  MOZ_DIAGNOSTIC_ASSERT(!mPendingDMABufFeedback,
                        "Can't add extra formats during init!");
  if (!mDMABufFeedback) {
    mDMABufFeedback = MakeUnique<DMABufFeedback>();
  }
  if (!GetFormat(GBM_FORMAT_XRGB8888)) {
    LOGDMABUF(
        ("DMABufFormats::EnsureBasicFormats(): GBM_FORMAT_XRGB8888 is missing, "
         "adding."));
    mDMABufFeedback->PendingTranche()->AddFormat(GBM_FORMAT_XRGB8888,
                                                 DRM_FORMAT_MOD_INVALID);
  }
  if (!GetFormat(GBM_FORMAT_ARGB8888)) {
    LOGDMABUF(
        ("DMABufFormats::EnsureBasicFormats(): GBM_FORMAT_ARGB8888 is missing, "
         "adding."));
    mDMABufFeedback->PendingTranche()->AddFormat(GBM_FORMAT_ARGB8888,
                                                 DRM_FORMAT_MOD_INVALID);
  }
  mDMABufFeedback->PendingTrancheDone();
}

DMABufFormats::DMABufFormats() {}

DMABufFormats::~DMABufFormats() {
#ifdef MOZ_WAYLAND
  if (mWaylandFeedback) {
    zwp_linux_dmabuf_feedback_v1_destroy(mWaylandFeedback);
  }
#endif
}

#ifdef MOZ_WAYLAND
RefPtr<DMABufFormats> CreateDMABufFeedbackFormats(
    wl_surface* aSurface, const DMABufFormatsCallback& aFormatRefreshCB) {
  if (!WaylandDisplayGet()->HasDMABufFeedback()) {
    return nullptr;
  }
  RefPtr<DMABufFormats> formats = new DMABufFormats();
  formats->InitFeedback(WaylandDisplayGet()->GetDmabuf(), aFormatRefreshCB,
                        aSurface);
  return formats.forget();
}
#endif

void GlobalDMABufFormats::SetModifiersToGfxVars() {
  RefPtr<DMABufFormats> formats;
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    formats = WaylandDisplayGet()->GetDMABufFormats();
  }
#endif
  if (!formats) {
    formats = new DMABufFormats();
  }
  formats->EnsureBasicFormats();

  DRMFormat* format = formats->GetFormat(GBM_FORMAT_XRGB8888);
  MOZ_DIAGNOSTIC_ASSERT(format, "Missing GBM_FORMAT_XRGB8888 dmabuf format!");
  mFormatRGBX = new DRMFormat(*format);
  gfxVars::SetDMABufModifiersXRGB(*format->GetModifiers());

  format = formats->GetFormat(GBM_FORMAT_ARGB8888);
  MOZ_DIAGNOSTIC_ASSERT(format, "Missing GBM_FORMAT_ARGB8888 dmabuf format!");
  mFormatRGBA = new DRMFormat(*format);
  gfxVars::SetDMABufModifiersARGB(*format->GetModifiers());

  format = formats->GetFormat(GBM_FORMAT_P010);
  if (format) {
    mFormatP010 = new DRMFormat(*format);
    gfxVars::SetDMABufModifiersP010(*format->GetModifiers());
  }

  format = formats->GetFormat(GBM_FORMAT_NV12);
  if (format) {
    mFormatNV12 = new DRMFormat(*format);
    gfxVars::SetDMABufModifiersNV12(*format->GetModifiers());
  }
}

void GlobalDMABufFormats::GetModifiersFromGfxVars() {
  mFormatRGBX =
      new DRMFormat(GBM_FORMAT_XRGB8888, gfxVars::DMABufModifiersXRGB());
  mFormatRGBA =
      new DRMFormat(GBM_FORMAT_ARGB8888, gfxVars::DMABufModifiersARGB());
  mFormatP010 = new DRMFormat(GBM_FORMAT_P010, gfxVars::DMABufModifiersP010());
  mFormatNV12 = new DRMFormat(GBM_FORMAT_NV12, gfxVars::DMABufModifiersNV12());
}

DRMFormat* GlobalDMABufFormats::GetDRMFormat(int32_t aFOURCCFormat) {
  switch (aFOURCCFormat) {
    case GBM_FORMAT_XRGB8888:
      MOZ_DIAGNOSTIC_ASSERT(mFormatRGBX, "Missing RGBX dmabuf format!");
      return mFormatRGBX;
    case GBM_FORMAT_ARGB8888:
      MOZ_DIAGNOSTIC_ASSERT(mFormatRGBA, "Missing RGBA dmabuf format!");
      return mFormatRGBA;
    case GBM_FORMAT_P010:
      return mFormatP010;
    case GBM_FORMAT_NV12:
      return mFormatNV12;
    default:
      gfxCriticalNoteOnce << "DMABufDevice::GetDRMFormat() unknow format: "
                          << aFOURCCFormat;
      return nullptr;
  }
}

void GlobalDMABufFormats::LoadFormatModifiers() {
  if (XRE_IsParentProcess()) {
    MOZ_ASSERT(NS_IsMainThread());
    SetModifiersToGfxVars();
  } else {
    GetModifiersFromGfxVars();
  }
}

GlobalDMABufFormats::GlobalDMABufFormats() { LoadFormatModifiers(); }

GlobalDMABufFormats* GetGlobalDMABufFormats() {
  static StaticAutoPtr<GlobalDMABufFormats> sGlobalDMABufFormats;
  static std::once_flag onceFlag;
  std::call_once(onceFlag, [] {
    sGlobalDMABufFormats = new GlobalDMABufFormats();
    if (NS_IsMainThread()) {
      ClearOnShutdown(&sGlobalDMABufFormats);
    } else {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "ClearGlobalDMABufFormats",
          [] { ClearOnShutdown(&sGlobalDMABufFormats); }));
    }
  });
  return sGlobalDMABufFormats.get();
}

}  // namespace mozilla::widget
