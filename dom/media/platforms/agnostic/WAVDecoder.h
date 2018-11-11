/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WaveDecoder_h_)
#define WaveDecoder_h_

#include "PlatformDecoderModule.h"
#include "mp4_demuxer/ByteReader.h"

namespace mozilla {

class WaveDataDecoder : public MediaDataDecoder
{
public:
  explicit WaveDataDecoder(const CreateDecoderParams& aParams);

  // Return true if mimetype is Wave
  static bool IsWave(const nsACString& aMimeType);

  RefPtr<InitPromise> Init() override;
  void Input(MediaRawData* aSample) override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "wave audio decoder";
  }

private:
  MediaResult DoDecode(MediaRawData* aSample);

  const AudioInfo& mInfo;
  MediaDataDecoderCallback* mCallback;
};

} // namespace mozilla
#endif
