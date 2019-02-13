/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\defgroup vp8 VP8
 * \ingroup codecs
 * VP8 is vpx's newest video compression algorithm that uses motion
 * compensated prediction, Discrete Cosine Transform (DCT) coding of the
 * prediction error signal and context dependent entropy coding techniques
 * based on arithmetic principles. It features:
 *  - YUV 4:2:0 image format
 *  - Macro-block based coding (16x16 luma plus two 8x8 chroma)
 *  - 1/4 (1/8) pixel accuracy motion compensated prediction
 *  - 4x4 DCT transform
 *  - 128 level linear quantizer
 *  - In loop deblocking filter
 *  - Context-based entropy coding
 *
 * @{
 */
/*!\file
 * \brief Provides controls common to both the VP8 encoder and decoder.
 */
#ifndef VPX_VP8_H_
#define VPX_VP8_H_

#include "./vpx_codec.h"
#include "./vpx_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Control functions
 *
 * The set of macros define the control functions of VP8 interface
 */
enum vp8_com_control_id {
  VP8_SET_REFERENCE           = 1,    /**< pass in an external frame into decoder to be used as reference frame */
  VP8_COPY_REFERENCE          = 2,    /**< get a copy of reference frame from the decoder */
  VP8_SET_POSTPROC            = 3,    /**< set the decoder's post processing settings  */
  VP8_SET_DBG_COLOR_REF_FRAME = 4,    /**< set the reference frames to color for each macroblock */
  VP8_SET_DBG_COLOR_MB_MODES  = 5,    /**< set which macro block modes to color */
  VP8_SET_DBG_COLOR_B_MODES   = 6,    /**< set which blocks modes to color */
  VP8_SET_DBG_DISPLAY_MV      = 7,    /**< set which motion vector modes to draw */

  /* TODO(jkoleszar): The encoder incorrectly reuses some of these values (5+)
   * for its control ids. These should be migrated to something like the
   * VP8_DECODER_CTRL_ID_START range next time we're ready to break the ABI.
   */
  VP9_GET_REFERENCE           = 128,  /**< get a pointer to a reference frame */
  VP8_COMMON_CTRL_ID_MAX,
  VP8_DECODER_CTRL_ID_START   = 256
};

/*!\brief post process flags
 *
 * The set of macros define VP8 decoder post processing flags
 */
enum vp8_postproc_level {
  VP8_NOFILTERING             = 0,
  VP8_DEBLOCK                 = 1 << 0,
  VP8_DEMACROBLOCK            = 1 << 1,
  VP8_ADDNOISE                = 1 << 2,
  VP8_DEBUG_TXT_FRAME_INFO    = 1 << 3, /**< print frame information */
  VP8_DEBUG_TXT_MBLK_MODES    = 1 << 4, /**< print macro block modes over each macro block */
  VP8_DEBUG_TXT_DC_DIFF       = 1 << 5, /**< print dc diff for each macro block */
  VP8_DEBUG_TXT_RATE_INFO     = 1 << 6, /**< print video rate info (encoder only) */
  VP8_MFQE                    = 1 << 10
};

/*!\brief post process flags
 *
 * This define a structure that describe the post processing settings. For
 * the best objective measure (using the PSNR metric) set post_proc_flag
 * to VP8_DEBLOCK and deblocking_level to 1.
 */

typedef struct vp8_postproc_cfg {
  int post_proc_flag;         /**< the types of post processing to be done, should be combination of "vp8_postproc_level" */
  int deblocking_level;       /**< the strength of deblocking, valid range [0, 16] */
  int noise_level;            /**< the strength of additive noise, valid range [0, 16] */
} vp8_postproc_cfg_t;

/*!\brief reference frame type
 *
 * The set of macros define the type of VP8 reference frames
 */
typedef enum vpx_ref_frame_type {
  VP8_LAST_FRAME = 1,
  VP8_GOLD_FRAME = 2,
  VP8_ALTR_FRAME = 4
} vpx_ref_frame_type_t;

/*!\brief reference frame data struct
 *
 * Define the data struct to access vp8 reference frames.
 */
typedef struct vpx_ref_frame {
  vpx_ref_frame_type_t  frame_type;   /**< which reference frame */
  vpx_image_t           img;          /**< reference frame data in image format */
} vpx_ref_frame_t;

/*!\brief VP9 specific reference frame data struct
 *
 * Define the data struct to access vp9 reference frames.
 */
typedef struct vp9_ref_frame {
  int idx; /**< frame index to get (input) */
  vpx_image_t  img; /**< img structure to populate (output) */
} vp9_ref_frame_t;

/*!\brief vp8 decoder control function parameter type
 *
 * defines the data type for each of VP8 decoder control function requires
 */
VPX_CTRL_USE_TYPE(VP8_SET_REFERENCE,           vpx_ref_frame_t *)
VPX_CTRL_USE_TYPE(VP8_COPY_REFERENCE,          vpx_ref_frame_t *)
VPX_CTRL_USE_TYPE(VP8_SET_POSTPROC,            vp8_postproc_cfg_t *)
VPX_CTRL_USE_TYPE(VP8_SET_DBG_COLOR_REF_FRAME, int)
VPX_CTRL_USE_TYPE(VP8_SET_DBG_COLOR_MB_MODES,  int)
VPX_CTRL_USE_TYPE(VP8_SET_DBG_COLOR_B_MODES,   int)
VPX_CTRL_USE_TYPE(VP8_SET_DBG_DISPLAY_MV,      int)
VPX_CTRL_USE_TYPE(VP9_GET_REFERENCE,           vp9_ref_frame_t *)

/*! @} - end defgroup vp8 */

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_VP8_H_
