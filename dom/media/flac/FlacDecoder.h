/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FLAC_DECODER_H_
#define FLAC_DECODER_H_

#include "MediaDecoder.h"

namespace mozilla {

class FlacDecoder : public MediaDecoder {
public:
  // MediaDecoder interface.
  explicit FlacDecoder(MediaDecoderOwner* aOwner) : MediaDecoder(aOwner) {}
  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override;
  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if the Flac backend is pref'ed on, and we're running on a
  // platform that is likely to have decoders for the format.
  static bool IsEnabled();
  static bool CanHandleMediaType(const nsACString& aType,
                                 const nsAString& aCodecs);
};

} // namespace mozilla

#endif // !FLAC_DECODER_H_
