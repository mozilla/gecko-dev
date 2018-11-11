/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_WRITER_H_
#define VP9_ENCODER_VP9_WRITER_H_

#include "vpx_ports/mem.h"

#include "vp9/common/vp9_prob.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vp9_writer {
  unsigned int lowvalue;
  unsigned int range;
  int count;
  unsigned int pos;
  uint8_t *buffer;
} vp9_writer;

void vp9_start_encode(vp9_writer *bc, uint8_t *buffer);
void vp9_stop_encode(vp9_writer *bc);

static INLINE void vp9_write(vp9_writer *br, int bit, int probability) {
  unsigned int split;
  int count = br->count;
  unsigned int range = br->range;
  unsigned int lowvalue = br->lowvalue;
  register unsigned int shift;

  split = 1 + (((range - 1) * probability) >> 8);

  range = split;

  if (bit) {
    lowvalue += split;
    range = br->range - split;
  }

  shift = vp9_norm[range];

  range <<= shift;
  count += shift;

  if (count >= 0) {
    int offset = shift - count;

    if ((lowvalue << (offset - 1)) & 0x80000000) {
      int x = br->pos - 1;

      while (x >= 0 && br->buffer[x] == 0xff) {
        br->buffer[x] = 0;
        x--;
      }

      br->buffer[x] += 1;
    }

    br->buffer[br->pos++] = (lowvalue >> (24 - offset));
    lowvalue <<= offset;
    shift = count;
    lowvalue &= 0xffffff;
    count -= 8;
  }

  lowvalue <<= shift;
  br->count = count;
  br->lowvalue = lowvalue;
  br->range = range;
}

static INLINE void vp9_write_bit(vp9_writer *w, int bit) {
  vp9_write(w, bit, 128);  // vp9_prob_half
}

static INLINE void vp9_write_literal(vp9_writer *w, int data, int bits) {
  int bit;

  for (bit = bits - 1; bit >= 0; bit--)
    vp9_write_bit(w, 1 & (data >> bit));
}

#define vp9_write_prob(w, v) vp9_write_literal((w), (v), 8)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_WRITER_H_
