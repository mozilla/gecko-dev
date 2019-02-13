/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VPX_VP8CX_H_
#define VPX_VP8CX_H_

/*!\defgroup vp8_encoder WebM VP8/VP9 Encoder
 * \ingroup vp8
 *
 * @{
 */
#include "./vp8.h"
#include "./vpx_encoder.h"

/*!\file
 * \brief Provides definitions for using VP8 or VP9 encoder algorithm within the
 *        vpx Codec Interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*!\name Algorithm interface for VP8
 *
 * This interface provides the capability to encode raw VP8 streams.
 * @{
 */
extern vpx_codec_iface_t  vpx_codec_vp8_cx_algo;
extern vpx_codec_iface_t *vpx_codec_vp8_cx(void);
/*!@} - end algorithm interface member group*/

/*!\name Algorithm interface for VP9
 *
 * This interface provides the capability to encode raw VP9 streams.
 * @{
 */
extern vpx_codec_iface_t  vpx_codec_vp9_cx_algo;
extern vpx_codec_iface_t *vpx_codec_vp9_cx(void);
/*!@} - end algorithm interface member group*/


/*
 * Algorithm Flags
 */

/*!\brief Don't reference the last frame
 *
 * When this flag is set, the encoder will not use the last frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * last frame or not automatically.
 */
#define VP8_EFLAG_NO_REF_LAST      (1<<16)


/*!\brief Don't reference the golden frame
 *
 * When this flag is set, the encoder will not use the golden frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * golden frame or not automatically.
 */
#define VP8_EFLAG_NO_REF_GF        (1<<17)


/*!\brief Don't reference the alternate reference frame
 *
 * When this flag is set, the encoder will not use the alt ref frame as a
 * predictor. When not set, the encoder will choose whether to use the
 * alt ref frame or not automatically.
 */
#define VP8_EFLAG_NO_REF_ARF       (1<<21)


/*!\brief Don't update the last frame
 *
 * When this flag is set, the encoder will not update the last frame with
 * the contents of the current frame.
 */
#define VP8_EFLAG_NO_UPD_LAST      (1<<18)


/*!\brief Don't update the golden frame
 *
 * When this flag is set, the encoder will not update the golden frame with
 * the contents of the current frame.
 */
#define VP8_EFLAG_NO_UPD_GF        (1<<22)


/*!\brief Don't update the alternate reference frame
 *
 * When this flag is set, the encoder will not update the alt ref frame with
 * the contents of the current frame.
 */
#define VP8_EFLAG_NO_UPD_ARF       (1<<23)


/*!\brief Force golden frame update
 *
 * When this flag is set, the encoder copy the contents of the current frame
 * to the golden frame buffer.
 */
#define VP8_EFLAG_FORCE_GF         (1<<19)


/*!\brief Force alternate reference frame update
 *
 * When this flag is set, the encoder copy the contents of the current frame
 * to the alternate reference frame buffer.
 */
#define VP8_EFLAG_FORCE_ARF        (1<<24)


/*!\brief Disable entropy update
 *
 * When this flag is set, the encoder will not update its internal entropy
 * model based on the entropy of this frame.
 */
#define VP8_EFLAG_NO_UPD_ENTROPY   (1<<20)


/*!\brief VPx encoder control functions
 *
 * This set of macros define the control functions available for VPx
 * encoder interface.
 *
 * \sa #vpx_codec_control
 */
enum vp8e_enc_control_id {
  /*!\brief Codec control function to set mode of entropy update in encoder.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_UPD_ENTROPY           = 5,

  /*!\brief Codec control function to set reference update mode in encoder.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_UPD_REFERENCE,

  /*!\brief Codec control function to set which reference frame encoder can use.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_USE_REFERENCE,

  /*!\brief Codec control function to pass an ROI map to encoder.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_ROI_MAP,

  /*!\brief Codec control function to pass an Active map to encoder.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_ACTIVEMAP,

  /*!\brief Codec control function to set encoder scaling mode.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_SCALEMODE         = 11,

  /*!\brief Codec control function to set encoder internal speed settings.
   *
   * Changes in this value influences, among others, the encoder's selection
   * of motion estimation methods. Values greater than 0 will increase encoder
   * speed at the expense of quality.
   *
   * \note Valid range for VP8: -16..16
   * \note Valid range for VP9: -8..8
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_CPUUSED           = 13,

  /*!\brief Codec control function to enable automatic set and use alf frames.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_ENABLEAUTOALTREF,

  /*!\brief control function to set noise sensitivity
   *
   * 0: off, 1: OnYOnly, 2: OnYUV,
   * 3: OnYUVAggressive, 4: Adaptive
   *
   * Supported in codecs: VP8
   */
  VP8E_SET_NOISE_SENSITIVITY,

  /*!\brief Codec control function to set sharpness.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_SHARPNESS,

  /*!\brief Codec control function to set the threshold for MBs treated static.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_STATIC_THRESHOLD,

  /*!\brief Codec control function to set the number of token partitions.
   *
   * Supported in codecs: VP8
   */
  VP8E_SET_TOKEN_PARTITIONS,

  /*!\brief Codec control function to get last quantizer chosen by the encoder.
   *
   * Return value uses internal quantizer scale defined by the codec.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_GET_LAST_QUANTIZER,

  /*!\brief Codec control function to get last quantizer chosen by the encoder.
   *
   * Return value uses the 0..63 scale as used by the rc_*_quantizer config
   * parameters.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_GET_LAST_QUANTIZER_64,

  /*!\brief Codec control function to set the max no of frames to create arf.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_ARNR_MAXFRAMES,

  /*!\brief Codec control function to set the filter strength for the arf.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_ARNR_STRENGTH,

  /*!\deprecated control function to set the filter type to use for the arf. */
  VP8E_SET_ARNR_TYPE,

  /*!\brief Codec control function to set visual tuning.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_TUNING,

  /*!\brief Codec control function to set constrained quality level.
   *
   * \attention For this value to be used vpx_codec_enc_cfg_t::g_usage must be
   *            set to #VPX_CQ.
   * \note Valid range: 0..63
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_CQ_LEVEL,

  /*!\brief Codec control function to set Max data rate for Intra frames.
   *
   * This value controls additional clamping on the maximum size of a
   * keyframe. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allocate no more than 4.5 frames worth of bitrate
   * to a keyframe, set this to 450.
   *
   * Supported in codecs: VP8, VP9
   */
  VP8E_SET_MAX_INTRA_BITRATE_PCT,

  /*!\brief Codec control function to set reference and update frame flags.
   *
   *  Supported in codecs: VP8
   */
  VP8E_SET_FRAME_FLAGS,

  /*!\brief Codec control function to set max data rate for Inter frames.
   *
   * This value controls additional clamping on the maximum size of an
   * inter frame. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * unlimited, or no additional clamping beyond the codec's built-in
   * algorithm.
   *
   * For example, to allow no more than 4.5 frames worth of bitrate
   * to an inter frame, set this to 450.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_MAX_INTER_BITRATE_PCT,

  /*!\brief Boost percentage for Golden Frame in CBR mode.
   *
   * This value controls the amount of boost given to Golden Frame in
   * CBR mode. It is expressed as a percentage of the average
   * per-frame bitrate, with the special (and default) value 0 meaning
   * the feature is off, i.e., no golden frame boost in CBR mode and
   * average bitrate target is used.
   *
   * For example, to allow 100% more bits, i.e, 2X, in a golden frame
   * than average frame, set this to 100.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_GF_CBR_BOOST_PCT,

  /*!\brief Codec control function to set the temporal layer id.
   *
   * For temporal scalability: this control allows the application to set the
   * layer id for each frame to be encoded. Note that this control must be set
   * for every frame prior to encoding. The usage of this control function
   * supersedes the internal temporal pattern counter, which is now deprecated.
   *
   * Supported in codecs: VP8
   */
  VP8E_SET_TEMPORAL_LAYER_ID,

  /*!\brief Codec control function to set encoder screen content mode.
   *
   * 0: off, 1: On, 2: On with more aggressive rate control.
   *
   * Supported in codecs: VP8
   */
  VP8E_SET_SCREEN_CONTENT_MODE,

  /*!\brief Codec control function to set lossless encoding mode.
   *
   * VP9 can operate in lossless encoding mode, in which the bitstream
   * produced will be able to decode and reconstruct a perfect copy of
   * input source. This control function provides a mean to switch encoder
   * into lossless coding mode(1) or normal coding mode(0) that may be lossy.
   *                          0 = lossy coding mode
   *                          1 = lossless coding mode
   *
   *  By default, encoder operates in normal coding mode (maybe lossy).
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_LOSSLESS,

  /*!\brief Codec control function to set number of tile columns.
   *
   * In encoding and decoding, VP9 allows an input image frame be partitioned
   * into separated vertical tile columns, which can be encoded or decoded
   * independently. This enables easy implementation of parallel encoding and
   * decoding. This control requests the encoder to use column tiles in
   * encoding an input frame, with number of tile columns (in Log2 unit) as
   * the parameter:
   *             0 = 1 tile column
   *             1 = 2 tile columns
   *             2 = 4 tile columns
   *             .....
   *             n = 2**n tile columns
   * The requested tile columns will be capped by encoder based on image size
   * limitation (The minimum width of a tile column is 256 pixel, the maximum
   * is 4096).
   *
   * By default, the value is 0, i.e. one single column tile for entire image.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_TILE_COLUMNS,

  /*!\brief Codec control function to set number of tile rows.
   *
   * In encoding and decoding, VP9 allows an input image frame be partitioned
   * into separated horizontal tile rows. Tile rows are encoded or decoded
   * sequentially. Even though encoding/decoding of later tile rows depends on
   * earlier ones, this allows the encoder to output data packets for tile rows
   * prior to completely processing all tile rows in a frame, thereby reducing
   * the latency in processing between input and output. The parameter
   * for this control describes the number of tile rows, which has a valid
   * range [0, 2]:
   *            0 = 1 tile row
   *            1 = 2 tile rows
   *            2 = 4 tile rows
   *
   * By default, the value is 0, i.e. one single row tile for entire image.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_TILE_ROWS,

  /*!\brief Codec control function to enable frame parallel decoding feature.
   *
   * VP9 has a bitstream feature to reduce decoding dependency between frames
   * by turning off backward update of probability context used in encoding
   * and decoding. This allows staged parallel processing of more than one
   * video frames in the decoder. This control function provides a mean to
   * turn this feature on or off for bitstreams produced by encoder.
   *
   * By default, this feature is off.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_FRAME_PARALLEL_DECODING,

  /*!\brief Codec control function to set adaptive quantization mode.
   *
   * VP9 has a segment based feature that allows encoder to adaptively change
   * quantization parameter for each segment within a frame to improve the
   * subjective quality. This control makes encoder operate in one of the
   * several AQ_modes supported.
   *
   * By default, encoder operates with AQ_Mode 0(adaptive quantization off).
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_AQ_MODE,

  /*!\brief Codec control function to enable/disable periodic Q boost.
   *
   * One VP9 encoder speed feature is to enable quality boost by lowering
   * frame level Q periodically. This control function provides a mean to
   * turn on/off this feature.
   *               0 = off
   *               1 = on
   *
   * By default, the encoder is allowed to use this feature for appropriate
   * encoding modes.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_FRAME_PERIODIC_BOOST,

  /*!\brief Codec control function to set noise sensitivity.
   *
   *  0: off, 1: On(YOnly)
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_NOISE_SENSITIVITY,

  /*!\brief Codec control function to turn on/off SVC in encoder.
   * \note Return value is VPX_CODEC_INVALID_PARAM if the encoder does not
   *       support SVC in its current encoding mode
   *  0: off, 1: on
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_SVC,

  /*!\brief Codec control function to set parameters for SVC.
   * \note Parameters contain min_q, max_q, scaling factor for each of the
   *       SVC layers.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_SVC_PARAMETERS,

  /*!\brief Codec control function to set svc layer for spatial and temporal.
   * \note Valid ranges: 0..#vpx_codec_enc_cfg::ss_number_layers for spatial
   *                     layer and 0..#vpx_codec_enc_cfg::ts_number_layers for
   *                     temporal layer.
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_SVC_LAYER_ID,

  /*!\brief Codec control function to set content type.
   * \note Valid parameter range:
   *              VP9E_CONTENT_DEFAULT = Regular video content (Default)
   *              VP9E_CONTENT_SCREEN  = Screen capture content
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_TUNE_CONTENT,

  /*!\brief Codec control function to get svc layer ID.
   * \note The layer ID returned is for the data packet from the registered
   *       callback function.
   *
   * Supported in codecs: VP9
   */
  VP9E_GET_SVC_LAYER_ID,

  /*!\brief Codec control function to register callback to get per layer packet.
   * \note Parameter for this control function is a structure with a callback
   *       function and a pointer to private data used by the callback.
   *
   * Supported in codecs: VP9
   */
  VP9E_REGISTER_CX_CALLBACK,

  /*!\brief Codec control function to set color space info.
   * \note Valid ranges: 0..7, default is "UNKNOWN".
   *                     0 = UNKNOWN,
   *                     1 = BT_601
   *                     2 = BT_709
   *                     3 = SMPTE_170
   *                     4 = SMPTE_240
   *                     5 = BT_2020
   *                     6 = RESERVED
   *                     7 = SRGB
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_COLOR_SPACE,

  /*!\brief Codec control function to set temporal layering mode.
   * \note Valid ranges: 0..3, default is "0" (VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING).
   *                     0 = VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING
   *                     1 = VP9E_TEMPORAL_LAYERING_MODE_BYPASS
   *                     2 = VP9E_TEMPORAL_LAYERING_MODE_0101
   *                     3 = VP9E_TEMPORAL_LAYERING_MODE_0212
   *
   * Supported in codecs: VP9
   */
  VP9E_SET_TEMPORAL_LAYERING_MODE,

  /*!\brief Codec control function to get an Active map back from the encoder.
   *
   * Supported in codecs: VP9
   */
  VP9E_GET_ACTIVEMAP,
};

/*!\brief vpx 1-D scaling mode
 *
 * This set of constants define 1-D vpx scaling modes
 */
typedef enum vpx_scaling_mode_1d {
  VP8E_NORMAL      = 0,
  VP8E_FOURFIVE    = 1,
  VP8E_THREEFIVE   = 2,
  VP8E_ONETWO      = 3
} VPX_SCALING_MODE;

/*!\brief Temporal layering mode enum for VP9 SVC.
 *
 * This set of macros define the different temporal layering modes.
 * Supported codecs: VP9 (in SVC mode)
 *
 */
typedef enum vp9e_temporal_layering_mode {
  /*!\brief No temporal layering.
   * Used when only spatial layering is used.
   */
  VP9E_TEMPORAL_LAYERING_MODE_NOLAYERING   = 0,

  /*!\brief Bypass mode.
   * Used when application needs to control temporal layering.
   * This will only work when the number of spatial layers equals 1.
   */
  VP9E_TEMPORAL_LAYERING_MODE_BYPASS       = 1,

  /*!\brief 0-1-0-1... temporal layering scheme with two temporal layers.
   */
  VP9E_TEMPORAL_LAYERING_MODE_0101         = 2,

  /*!\brief 0-2-1-2... temporal layering scheme with three temporal layers.
   */
  VP9E_TEMPORAL_LAYERING_MODE_0212         = 3
} VP9E_TEMPORAL_LAYERING_MODE;

/*!\brief  vpx region of interest map
 *
 * These defines the data structures for the region of interest map
 *
 */

typedef struct vpx_roi_map {
  /*! An id between 0 and 3 for each 16x16 region within a frame. */
  unsigned char *roi_map;
  unsigned int rows;       /**< Number of rows. */
  unsigned int cols;       /**< Number of columns. */
  // TODO(paulwilkins): broken for VP9 which has 8 segments
  // q and loop filter deltas for each segment
  // (see MAX_MB_SEGMENTS)
  int delta_q[4];          /**< Quantizer deltas. */
  int delta_lf[4];         /**< Loop filter deltas. */
  /*! Static breakout threshold for each segment. */
  unsigned int static_threshold[4];
} vpx_roi_map_t;

/*!\brief  vpx active region map
 *
 * These defines the data structures for active region map
 *
 */


typedef struct vpx_active_map {
  unsigned char  *active_map; /**< specify an on (1) or off (0) each 16x16 region within a frame */
  unsigned int    rows;       /**< number of rows */
  unsigned int    cols;       /**< number of cols */
} vpx_active_map_t;

/*!\brief  vpx image scaling mode
 *
 * This defines the data structure for image scaling mode
 *
 */
typedef struct vpx_scaling_mode {
  VPX_SCALING_MODE    h_scaling_mode;  /**< horizontal scaling mode */
  VPX_SCALING_MODE    v_scaling_mode;  /**< vertical scaling mode   */
} vpx_scaling_mode_t;

/*!\brief VP8 token partition mode
 *
 * This defines VP8 partitioning mode for compressed data, i.e., the number of
 * sub-streams in the bitstream. Used for parallelized decoding.
 *
 */

typedef enum {
  VP8_ONE_TOKENPARTITION   = 0,
  VP8_TWO_TOKENPARTITION   = 1,
  VP8_FOUR_TOKENPARTITION  = 2,
  VP8_EIGHT_TOKENPARTITION = 3
} vp8e_token_partitions;

/*!brief VP9 encoder content type */
typedef enum {
  VP9E_CONTENT_DEFAULT,
  VP9E_CONTENT_SCREEN,
  VP9E_CONTENT_INVALID
} vp9e_tune_content;

/*!\brief VP8 model tuning parameters
 *
 * Changes the encoder to tune for certain types of input material.
 *
 */
typedef enum {
  VP8_TUNE_PSNR,
  VP8_TUNE_SSIM
} vp8e_tuning;

/*!\brief  vp9 svc layer parameters
 *
 * This defines the spatial and temporal layer id numbers for svc encoding.
 * This is used with the #VP9E_SET_SVC_LAYER_ID control to set the spatial and
 * temporal layer id for the current frame.
 *
 */
typedef struct vpx_svc_layer_id {
  int spatial_layer_id;       /**< Spatial layer id number. */
  int temporal_layer_id;      /**< Temporal layer id number. */
} vpx_svc_layer_id_t;

/*!\brief VP8 encoder control function parameter type
 *
 * Defines the data types that VP8E control functions take. Note that
 * additional common controls are defined in vp8.h
 *
 */


/* These controls have been deprecated in favor of the flags parameter to
 * vpx_codec_encode(). See the definition of VP8_EFLAG_* above.
 */
VPX_CTRL_USE_TYPE_DEPRECATED(VP8E_UPD_ENTROPY,            int)
VPX_CTRL_USE_TYPE_DEPRECATED(VP8E_UPD_REFERENCE,          int)
VPX_CTRL_USE_TYPE_DEPRECATED(VP8E_USE_REFERENCE,          int)

VPX_CTRL_USE_TYPE(VP8E_SET_FRAME_FLAGS,        int)
VPX_CTRL_USE_TYPE(VP8E_SET_TEMPORAL_LAYER_ID,  int)
VPX_CTRL_USE_TYPE(VP8E_SET_ROI_MAP,            vpx_roi_map_t *)
VPX_CTRL_USE_TYPE(VP8E_SET_ACTIVEMAP,          vpx_active_map_t *)
VPX_CTRL_USE_TYPE(VP8E_SET_SCALEMODE,          vpx_scaling_mode_t *)

VPX_CTRL_USE_TYPE(VP9E_SET_SVC,                int)
VPX_CTRL_USE_TYPE(VP9E_SET_SVC_PARAMETERS,     void *)
VPX_CTRL_USE_TYPE(VP9E_REGISTER_CX_CALLBACK,   void *)
VPX_CTRL_USE_TYPE(VP9E_SET_SVC_LAYER_ID,       vpx_svc_layer_id_t *)

VPX_CTRL_USE_TYPE(VP8E_SET_CPUUSED,            int)
VPX_CTRL_USE_TYPE(VP8E_SET_ENABLEAUTOALTREF,   unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_NOISE_SENSITIVITY,  unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_SHARPNESS,          unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_STATIC_THRESHOLD,   unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_TOKEN_PARTITIONS,   int) /* vp8e_token_partitions */

VPX_CTRL_USE_TYPE(VP8E_SET_ARNR_MAXFRAMES,     unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_ARNR_STRENGTH,     unsigned int)
VPX_CTRL_USE_TYPE_DEPRECATED(VP8E_SET_ARNR_TYPE,     unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_TUNING,             int) /* vp8e_tuning */
VPX_CTRL_USE_TYPE(VP8E_SET_CQ_LEVEL,      unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_TILE_COLUMNS,  int)
VPX_CTRL_USE_TYPE(VP9E_SET_TILE_ROWS,  int)

VPX_CTRL_USE_TYPE(VP8E_GET_LAST_QUANTIZER,     int *)
VPX_CTRL_USE_TYPE(VP8E_GET_LAST_QUANTIZER_64,  int *)
VPX_CTRL_USE_TYPE(VP9E_GET_SVC_LAYER_ID,  vpx_svc_layer_id_t *)

VPX_CTRL_USE_TYPE(VP8E_SET_MAX_INTRA_BITRATE_PCT, unsigned int)
VPX_CTRL_USE_TYPE(VP8E_SET_MAX_INTER_BITRATE_PCT, unsigned int)

VPX_CTRL_USE_TYPE(VP8E_SET_SCREEN_CONTENT_MODE, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_GF_CBR_BOOST_PCT, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_LOSSLESS, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_FRAME_PARALLEL_DECODING, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_AQ_MODE, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_FRAME_PERIODIC_BOOST, unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_NOISE_SENSITIVITY,  unsigned int)

VPX_CTRL_USE_TYPE(VP9E_SET_TUNE_CONTENT, int) /* vp9e_tune_content */

VPX_CTRL_USE_TYPE(VP9E_SET_COLOR_SPACE, int)

VPX_CTRL_USE_TYPE(VP9E_GET_ACTIVEMAP, vpx_active_map_t *)
/*! @} - end defgroup vp8_encoder */
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_VP8CX_H_
