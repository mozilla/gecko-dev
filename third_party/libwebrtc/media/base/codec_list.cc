/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/codec_list.h"

#include "media/base/codec.h"
#include "rtc_base/checks.h"

namespace cricket {

void CodecList::CheckConsistency() {
#if RTC_DCHECK_IS_ON
  for (Codec codec : codecs_) {
    // Do some checking
  }
#endif
}

}  // namespace cricket
