/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string.h>
#include "g711.h"
#include "g711_interface.h"
#include "webrtc/typedefs.h"

int16_t WebRtcG711_EncodeA(const int16_t* speechIn,
                           int16_t len,
                           uint8_t* encoded) {
  int n;
  for (n = 0; n < len; n++)
    encoded[n] = linear_to_alaw(speechIn[n]);
  return len;
}

int16_t WebRtcG711_EncodeU(const int16_t* speechIn,
                           int16_t len,
                           uint8_t* encoded) {
  int n;
  for (n = 0; n < len; n++)
    encoded[n] = linear_to_ulaw(speechIn[n]);
  return len;
}

int16_t WebRtcG711_DecodeA(const uint8_t* encoded,
                           int16_t len,
                           int16_t* decoded,
                           int16_t* speechType) {
  int n;
  for (n = 0; n < len; n++)
    decoded[n] = alaw_to_linear(encoded[n]);
  *speechType = 1;
  return len;
}

int16_t WebRtcG711_DecodeU(const uint8_t* encoded,
                           int16_t len,
                           int16_t* decoded,
                           int16_t* speechType) {
  int n;
  for (n = 0; n < len; n++)
    decoded[n] = ulaw_to_linear(encoded[n]);
  *speechType = 1;
  return len;
}

int WebRtcG711_DurationEst(const uint8_t* payload,
                           int payload_length_bytes) {
  (void) payload;
  /* G.711 is one byte per sample, so we can just return the number of bytes. */
  return payload_length_bytes;
}

int16_t WebRtcG711_Version(char* version, int16_t lenBytes) {
  strncpy(version, "2.0.0", lenBytes);
  return 0;
}
