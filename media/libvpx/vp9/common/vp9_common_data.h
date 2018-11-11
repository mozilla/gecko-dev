/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_COMMON_DATA_H_
#define VP9_COMMON_VP9_COMMON_DATA_H_

#include "vp9/common/vp9_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const int b_width_log2_lookup[BLOCK_SIZES];
extern const int b_height_log2_lookup[BLOCK_SIZES];
extern const int mi_width_log2_lookup[BLOCK_SIZES];
extern const int num_8x8_blocks_wide_lookup[BLOCK_SIZES];
extern const int num_8x8_blocks_high_lookup[BLOCK_SIZES];
extern const int num_4x4_blocks_high_lookup[BLOCK_SIZES];
extern const int num_4x4_blocks_wide_lookup[BLOCK_SIZES];
extern const int size_group_lookup[BLOCK_SIZES];
extern const int num_pels_log2_lookup[BLOCK_SIZES];
extern const PARTITION_TYPE partition_lookup[][BLOCK_SIZES];
extern const BLOCK_SIZE subsize_lookup[PARTITION_TYPES][BLOCK_SIZES];
extern const TX_SIZE max_txsize_lookup[BLOCK_SIZES];
extern const BLOCK_SIZE txsize_to_bsize[TX_SIZES];
extern const TX_SIZE tx_mode_to_biggest_tx_size[TX_MODES];
extern const BLOCK_SIZE ss_size_lookup[BLOCK_SIZES][2][2];

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_COMMON_VP9_COMMON_DATA_H_
