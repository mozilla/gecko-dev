/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegUtils.h"

#include "FFmpegLibWrapper.h"
#include "mozilla/Assertions.h"
#include "nsString.h"

namespace mozilla {

nsCString MakeErrorString(const FFmpegLibWrapper* aLib, int aErrNum) {
  MOZ_ASSERT(aLib);

  char errStr[FFmpegErrorMaxStringSize];
  aLib->av_strerror(aErrNum, errStr, FFmpegErrorMaxStringSize);
  return nsCString(errStr);
}

#define ENUM_TO_STR(enumVal) \
  if (aCodec == (enumVal)) { \
    return #enumVal;         \
  }

const char* AVCodecToString(const AVCodecID& aCodec) {
  ENUM_TO_STR(AV_CODEC_ID_AV1);
  ENUM_TO_STR(AV_CODEC_ID_VP9);
  return "unknown";
}

#undef ENUM_TO_STR

}  // namespace mozilla
