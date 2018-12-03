/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A struct defining a media feature change. */

#ifndef mozilla_MediaFeatureChange_h__
#define mozilla_MediaFeatureChange_h__

#include "nsChangeHint.h"
#include "mozilla/Attributes.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {

enum class MediaFeatureChangeReason {
  // The viewport size the document has used has changed.
  //
  // This affects size media queries like min-width.
  ViewportChange = 1 << 0,
  // The effective text zoom has changed. This affects the meaning of em units,
  // and thus affects any media query that uses a Length.
  ZoomChange = 1 << 1,
  // The base min font size has changed. This can affect the meaning of em
  // units, if the previous default font-size has changed, and also zoom.
  MinFontSizeChange = 1 << 2,
  // The resolution has changed. This can affect device-pixel-ratio media
  // queries, for example.
  ResolutionChange = 1 << 3,
  // The medium has changed.
  MediumChange = 1 << 4,
  // The size-mode has changed.
  SizeModeChange = 1 << 5,
  // A system metric or multiple have changed. This affects all the media
  // features that expose the presence of a system metric directly.
  SystemMetricsChange = 1 << 6,
  // The fact of whether the device size is the page size has changed, thus
  // resolution media queries can change.
  DeviceSizeIsPageSizeChange = 1 << 7,
  // display-mode changed on the document, thus the display-mode media queries
  // may have changed.
  DisplayModeChange = 1 << 8,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MediaFeatureChangeReason)

struct MediaFeatureChange {
  nsRestyleHint mRestyleHint;
  nsChangeHint mChangeHint;
  MediaFeatureChangeReason mReason;

  MOZ_IMPLICIT MediaFeatureChange(MediaFeatureChangeReason aReason)
      : MediaFeatureChange(nsRestyleHint(0), nsChangeHint(0), aReason) {}

  MediaFeatureChange(nsRestyleHint aRestyleHint, nsChangeHint aChangeHint,
                     MediaFeatureChangeReason aReason)
      : mRestyleHint(aRestyleHint),
        mChangeHint(aChangeHint),
        mReason(aReason) {}

  inline MediaFeatureChange& operator|=(const MediaFeatureChange& aOther) {
    mRestyleHint |= aOther.mRestyleHint;
    mChangeHint |= aOther.mChangeHint;
    mReason |= aOther.mReason;
    return *this;
  }
};

}  // namespace mozilla

#endif
