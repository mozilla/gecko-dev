/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_DECODER_VP9_DETOKENIZE_H_
#define VP9_DECODER_VP9_DETOKENIZE_H_

#include "vp9/decoder/vp9_decoder.h"
#include "vp9/decoder/vp9_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

int vp9_decode_block_tokens(MACROBLOCKD *xd,
                            int plane, int block,
                            BLOCK_SIZE plane_bsize, int x, int y,
                            TX_SIZE tx_size, vp9_reader *r,
                            int seg_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_DECODER_VP9_DETOKENIZE_H_
