/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_NativeFontResourceMac_h
#define mozilla_gfx_NativeFontResourceMac_h

#include "2D.h"
#include "mozilla/AlreadyAddRefed.h"
#include "ScaledFontMac.h"

namespace mozilla {
namespace gfx {

class NativeFontResourceMac final : public NativeFontResource {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(NativeFontResourceMac, override)

  static already_AddRefed<NativeFontResourceMac> Create(uint8_t* aFontData,
                                                        uint32_t aDataLength,
                                                        bool aNeedsCairo);

  already_AddRefed<UnscaledFont> CreateUnscaledFont(
      uint32_t aIndex, const uint8_t* aInstanceData,
      uint32_t aInstanceDataLength) final;

  ~NativeFontResourceMac() { CFRelease(mFontRef); }

 private:
  NativeFontResourceMac(CGFontRef aFontRef, bool aNeedsCairo)
      : mFontRef(aFontRef), mNeedsCairo(aNeedsCairo) {}

  CGFontRef mFontRef;
  bool mNeedsCairo;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_NativeFontResourceMac_h
