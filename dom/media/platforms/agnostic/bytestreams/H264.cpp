/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ResultExtensions.h"
#include "BitReader.h"
#include "BitWriter.h"
#include "BufferReader.h"
#include "ByteWriter.h"
#include "AnnexB.h"
#include "H264.h"
#include <limits>
#include <cmath>

#define READSE(var, min, max)     \
  {                               \
    int32_t val = br.ReadSE();    \
    if (val < min || val > max) { \
      return false;               \
    }                             \
    aDest.var = val;              \
  }

#define READUE(var, max)         \
  {                              \
    uint32_t uval = br.ReadUE(); \
    if (uval > max) {            \
      return false;              \
    }                            \
    aDest.var = uval;            \
  }

namespace mozilla {

// Default scaling lists (per spec).
// ITU H264:
// Table 7-2 – Assignment of mnemonic names to scaling list indices and
// specification of fall-back rule
static const uint8_t Default_4x4_Intra[16] = {6,  13, 13, 20, 20, 20, 28, 28,
                                              28, 28, 32, 32, 32, 37, 37, 42};

static const uint8_t Default_4x4_Inter[16] = {10, 14, 14, 20, 20, 20, 24, 24,
                                              24, 24, 27, 27, 27, 30, 30, 34};

static const uint8_t Default_8x8_Intra[64] = {
    6,  10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
    23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
    27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
    31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42};

static const uint8_t Default_8x8_Inter[64] = {
    9,  13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
    21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
    24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
    27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35};

namespace detail {
static void scaling_list(BitReader& aBr, uint8_t* aScalingList,
                         int aSizeOfScalingList, const uint8_t* aDefaultList,
                         const uint8_t* aFallbackList) {
  int32_t lastScale = 8;
  int32_t nextScale = 8;
  int32_t deltaScale;

  // (pic|seq)_scaling_list_present_flag[i]
  if (!aBr.ReadBit()) {
    if (aFallbackList) {
      memcpy(aScalingList, aFallbackList, aSizeOfScalingList);
    }
    return;
  }

  for (int i = 0; i < aSizeOfScalingList; i++) {
    if (nextScale != 0) {
      deltaScale = aBr.ReadSE();
      nextScale = (lastScale + deltaScale + 256) % 256;
      if (!i && !nextScale) {
        memcpy(aScalingList, aDefaultList, aSizeOfScalingList);
        return;
      }
    }
    aScalingList[i] = (nextScale == 0) ? lastScale : nextScale;
    lastScale = aScalingList[i];
  }
}
}  // namespace detail.

template <size_t N>
static void scaling_list(BitReader& aBr, uint8_t (&aScalingList)[N],
                         const uint8_t (&aDefaultList)[N],
                         const uint8_t (&aFallbackList)[N]) {
  detail::scaling_list(aBr, aScalingList, N, aDefaultList, aFallbackList);
}

template <size_t N>
static void scaling_list(BitReader& aBr, uint8_t (&aScalingList)[N],
                         const uint8_t (&aDefaultList)[N]) {
  detail::scaling_list(aBr, aScalingList, N, aDefaultList, nullptr);
}

SPSData::SPSData() {
  PodZero(this);
  // Default values when they aren't defined as per ITU-T H.264 (2014/02).
  chroma_format_idc = 1;
  video_format = 5;
  colour_primaries = 2;
  transfer_characteristics = 2;
  sample_ratio = 1.0;
  memset(scaling_matrix4x4, 16, sizeof(scaling_matrix4x4));
  memset(scaling_matrix8x8, 16, sizeof(scaling_matrix8x8));
}

bool SPSData::operator==(const SPSData& aOther) const {
  return this->valid && aOther.valid && !memcmp(this, &aOther, sizeof(SPSData));
}

bool SPSData::operator!=(const SPSData& aOther) const {
  return !(operator==(aOther));
}

// SPSNAL and SPSNALIterator do not own their data.
class SPSNAL {
 public:
  SPSNAL(const uint8_t* aPtr, size_t aLength) {
    MOZ_ASSERT(aPtr);

    if (aLength == 0 || (*aPtr & 0x1f) != H264_NAL_SPS) {
      return;
    }
    mDecodedNAL = H264::DecodeNALUnit(aPtr, aLength);
    if (mDecodedNAL) {
      mLength = BitReader::GetBitLength(mDecodedNAL);
    }
  }

  SPSNAL() {}

  bool IsValid() const { return mDecodedNAL; }

  bool operator==(const SPSNAL& aOther) const {
    if (!mDecodedNAL || !aOther.mDecodedNAL) {
      return false;
    }

    SPSData decodedSPS1;
    SPSData decodedSPS2;
    if (!GetSPSData(decodedSPS1) || !aOther.GetSPSData(decodedSPS2)) {
      // Couldn't decode one SPS, perform a binary comparison
      if (mLength != aOther.mLength) {
        return false;
      }
      MOZ_ASSERT(mLength / 8 <= mDecodedNAL->Length());

      if (memcmp(mDecodedNAL->Elements(), aOther.mDecodedNAL->Elements(),
                 mLength / 8)) {
        return false;
      }

      uint32_t remaining = mLength - (mLength & ~7);

      BitReader b1(mDecodedNAL->Elements() + mLength / 8, remaining);
      BitReader b2(aOther.mDecodedNAL->Elements() + mLength / 8, remaining);
      for (uint32_t i = 0; i < remaining; i++) {
        if (b1.ReadBit() != b2.ReadBit()) {
          return false;
        }
      }
      return true;
    }

    return decodedSPS1 == decodedSPS2;
  }

  bool operator!=(const SPSNAL& aOther) const { return !(operator==(aOther)); }

  bool GetSPSData(SPSData& aDest) const {
    return H264::DecodeSPS(mDecodedNAL, aDest);
  }

 private:
  RefPtr<mozilla::MediaByteBuffer> mDecodedNAL;
  uint32_t mLength = 0;
};

class SPSNALIterator {
 public:
  explicit SPSNALIterator(const mozilla::MediaByteBuffer* aExtraData)
      : mExtraDataPtr(aExtraData->Elements()), mReader(aExtraData) {
    if (!mReader.Read(5)) {
      return;
    }

    auto res = mReader.ReadU8();
    mNumSPS = res.isOk() ? res.unwrap() & 0x1f : 0;
    if (mNumSPS == 0) {
      return;
    }
    mValid = true;
  }

  SPSNALIterator& operator++() {
    if (mEOS || !mValid) {
      return *this;
    }
    if (--mNumSPS == 0) {
      mEOS = true;
    }
    auto res = mReader.ReadU16();
    uint16_t length = res.isOk() ? res.unwrap() : 0;
    if (length == 0 || !mReader.Read(length)) {
      mEOS = true;
    }
    return *this;
  }

  explicit operator bool() const { return mValid && !mEOS; }

  SPSNAL operator*() const {
    MOZ_ASSERT(bool(*this));
    BufferReader reader(mExtraDataPtr + mReader.Offset(), mReader.Remaining());

    auto res = reader.ReadU16();
    uint16_t length = res.isOk() ? res.unwrap() : 0;
    const uint8_t* ptr = reader.Read(length);
    if (!ptr || !length) {
      return SPSNAL();
    }
    return SPSNAL(ptr, length);
  }

 private:
  const uint8_t* mExtraDataPtr;
  BufferReader mReader;
  bool mValid = false;
  bool mEOS = false;
  uint8_t mNumSPS = 0;
};

/* static */ already_AddRefed<mozilla::MediaByteBuffer> H264::DecodeNALUnit(
    const uint8_t* aNAL, size_t aLength) {
  MOZ_ASSERT(aNAL);

  if (aLength < 4) {
    return nullptr;
  }

  RefPtr<mozilla::MediaByteBuffer> rbsp = new mozilla::MediaByteBuffer;
  BufferReader reader(aNAL, aLength);
  auto res = reader.ReadU8();
  if (res.isErr()) {
    return nullptr;
  }
  uint8_t nal_unit_type = res.unwrap() & 0x1f;
  uint32_t nalUnitHeaderBytes = 1;
  if (nal_unit_type == H264_NAL_PREFIX || nal_unit_type == H264_NAL_SLICE_EXT ||
      nal_unit_type == H264_NAL_SLICE_EXT_DVC) {
    bool svc_extension_flag = false;
    bool avc_3d_extension_flag = false;
    if (nal_unit_type != H264_NAL_SLICE_EXT_DVC) {
      res = reader.PeekU8();
      if (res.isErr()) {
        return nullptr;
      }
      svc_extension_flag = res.unwrap() & 0x80;
    } else {
      res = reader.PeekU8();
      if (res.isErr()) {
        return nullptr;
      }
      avc_3d_extension_flag = res.unwrap() & 0x80;
    }
    if (svc_extension_flag) {
      nalUnitHeaderBytes += 3;
    } else if (avc_3d_extension_flag) {
      nalUnitHeaderBytes += 2;
    } else {
      nalUnitHeaderBytes += 3;
    }
  }
  if (!reader.Read(nalUnitHeaderBytes - 1)) {
    return nullptr;
  }
  uint32_t lastbytes = 0xffff;
  while (reader.Remaining()) {
    auto res = reader.ReadU8();
    if (res.isErr()) {
      return nullptr;
    }
    uint8_t byte = res.unwrap();
    if ((lastbytes & 0xffff) == 0 && byte == 0x03) {
      // reset last two bytes, to detect the 0x000003 sequence again.
      lastbytes = 0xffff;
    } else {
      rbsp->AppendElement(byte);
    }
    lastbytes = (lastbytes << 8) | byte;
  }
  return rbsp.forget();
}

// The reverse of DecodeNALUnit. To allow the distinction between Annex B (that
// uses 0x000001 as marker) and AVCC, the pattern 0x00 0x00 0x0n (where n is
// between 0 and 3) can't be found in the bytestream. A 0x03 byte is inserted
// after the second 0. Eg. 0x00 0x00 0x00 becomes 0x00 0x00 0x03 0x00
/* static */ already_AddRefed<mozilla::MediaByteBuffer> H264::EncodeNALUnit(
    const uint8_t* aNAL, size_t aLength) {
  MOZ_ASSERT(aNAL);
  RefPtr<MediaByteBuffer> rbsp = new MediaByteBuffer();
  BufferReader reader(aNAL, aLength);

  auto res = reader.ReadU8();
  if (res.isErr()) {
    return rbsp.forget();
  }
  rbsp->AppendElement(res.unwrap());

  res = reader.ReadU8();
  if (res.isErr()) {
    return rbsp.forget();
  }
  rbsp->AppendElement(res.unwrap());

  while ((res = reader.ReadU8()).isOk()) {
    uint8_t val = res.unwrap();
    if (val <= 0x03 && rbsp->ElementAt(rbsp->Length() - 2) == 0 &&
        rbsp->ElementAt(rbsp->Length() - 1) == 0) {
      rbsp->AppendElement(0x03);
    }
    rbsp->AppendElement(val);
  }
  return rbsp.forget();
}

static int32_t ConditionDimension(float aValue) {
  // This will exclude NaNs and too-big values.
  if (aValue > 1.0 && aValue <= INT32_MAX) return int32_t(aValue);
  return 0;
}

/* static */ bool H264::DecodeSPS(const mozilla::MediaByteBuffer* aSPS,
                                  SPSData& aDest) {
  if (!aSPS) {
    return false;
  }
  BitReader br(aSPS, BitReader::GetBitLength(aSPS));

  aDest.profile_idc = br.ReadBits(8);
  aDest.constraint_set0_flag = br.ReadBit();
  aDest.constraint_set1_flag = br.ReadBit();
  aDest.constraint_set2_flag = br.ReadBit();
  aDest.constraint_set3_flag = br.ReadBit();
  aDest.constraint_set4_flag = br.ReadBit();
  aDest.constraint_set5_flag = br.ReadBit();
  br.ReadBits(2);  // reserved_zero_2bits
  aDest.level_idc = br.ReadBits(8);
  READUE(seq_parameter_set_id, MAX_SPS_COUNT - 1);

  if (aDest.profile_idc == 100 || aDest.profile_idc == 110 ||
      aDest.profile_idc == 122 || aDest.profile_idc == 244 ||
      aDest.profile_idc == 44 || aDest.profile_idc == 83 ||
      aDest.profile_idc == 86 || aDest.profile_idc == 118 ||
      aDest.profile_idc == 128 || aDest.profile_idc == 138 ||
      aDest.profile_idc == 139 || aDest.profile_idc == 134) {
    READUE(chroma_format_idc, 3);
    if (aDest.chroma_format_idc == 3) {
      aDest.separate_colour_plane_flag = br.ReadBit();
    }
    READUE(bit_depth_luma_minus8, 6);
    READUE(bit_depth_chroma_minus8, 6);
    br.ReadBit();  // qpprime_y_zero_transform_bypass_flag
    aDest.seq_scaling_matrix_present_flag = br.ReadBit();
    if (aDest.seq_scaling_matrix_present_flag) {
      scaling_list(br, aDest.scaling_matrix4x4[0], Default_4x4_Intra,
                   Default_4x4_Intra);
      scaling_list(br, aDest.scaling_matrix4x4[1], Default_4x4_Intra,
                   aDest.scaling_matrix4x4[0]);
      scaling_list(br, aDest.scaling_matrix4x4[2], Default_4x4_Intra,
                   aDest.scaling_matrix4x4[1]);
      scaling_list(br, aDest.scaling_matrix4x4[3], Default_4x4_Inter,
                   Default_4x4_Inter);
      scaling_list(br, aDest.scaling_matrix4x4[4], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[3]);
      scaling_list(br, aDest.scaling_matrix4x4[5], Default_4x4_Inter,
                   aDest.scaling_matrix4x4[4]);

      scaling_list(br, aDest.scaling_matrix8x8[0], Default_8x8_Intra,
                   Default_8x8_Intra);
      scaling_list(br, aDest.scaling_matrix8x8[1], Default_8x8_Inter,
                   Default_8x8_Inter);
      if (aDest.chroma_format_idc == 3) {
        scaling_list(br, aDest.scaling_matrix8x8[2], Default_8x8_Intra,
                     aDest.scaling_matrix8x8[0]);
        scaling_list(br, aDest.scaling_matrix8x8[3], Default_8x8_Inter,
                     aDest.scaling_matrix8x8[1]);
        scaling_list(br, aDest.scaling_matrix8x8[4], Default_8x8_Intra,
                     aDest.scaling_matrix8x8[2]);
        scaling_list(br, aDest.scaling_matrix8x8[5], Default_8x8_Inter,
                     aDest.scaling_matrix8x8[3]);
      }
    }
  } else if (aDest.profile_idc == 183) {
    aDest.chroma_format_idc = 0;
  } else {
    // default value if chroma_format_idc isn't set.
    aDest.chroma_format_idc = 1;
  }
  READUE(log2_max_frame_num, 12);
  aDest.log2_max_frame_num += 4;
  READUE(pic_order_cnt_type, 2);
  if (aDest.pic_order_cnt_type == 0) {
    READUE(log2_max_pic_order_cnt_lsb, 12);
    aDest.log2_max_pic_order_cnt_lsb += 4;
  } else if (aDest.pic_order_cnt_type == 1) {
    aDest.delta_pic_order_always_zero_flag = br.ReadBit();
    READSE(offset_for_non_ref_pic, -231, 230);
    READSE(offset_for_top_to_bottom_field, -231, 230);
    uint32_t num_ref_frames_in_pic_order_cnt_cycle = br.ReadUE();
    for (uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
      br.ReadSE();  // offset_for_ref_frame[i]
    }
  }
  aDest.max_num_ref_frames = br.ReadUE();
  aDest.gaps_in_frame_num_allowed_flag = br.ReadBit();
  aDest.pic_width_in_mbs = br.ReadUE() + 1;
  aDest.pic_height_in_map_units = br.ReadUE() + 1;
  aDest.frame_mbs_only_flag = br.ReadBit();
  if (!aDest.frame_mbs_only_flag) {
    aDest.pic_height_in_map_units *= 2;
    aDest.mb_adaptive_frame_field_flag = br.ReadBit();
  }
  aDest.direct_8x8_inference_flag = br.ReadBit();
  aDest.frame_cropping_flag = br.ReadBit();
  if (aDest.frame_cropping_flag) {
    aDest.frame_crop_left_offset = br.ReadUE();
    aDest.frame_crop_right_offset = br.ReadUE();
    aDest.frame_crop_top_offset = br.ReadUE();
    aDest.frame_crop_bottom_offset = br.ReadUE();
  }

  aDest.sample_ratio = 1.0f;
  aDest.vui_parameters_present_flag = br.ReadBit();
  if (aDest.vui_parameters_present_flag) {
    if (!vui_parameters(br, aDest)) {
      return false;
    }
  }

  // Calculate common values.

  uint8_t ChromaArrayType =
      aDest.separate_colour_plane_flag ? 0 : aDest.chroma_format_idc;
  // Calculate width.
  uint32_t CropUnitX = 1;
  uint32_t SubWidthC = aDest.chroma_format_idc == 3 ? 1 : 2;
  if (ChromaArrayType != 0) {
    CropUnitX = SubWidthC;
  }

  // Calculate Height
  uint32_t CropUnitY = 2 - aDest.frame_mbs_only_flag;
  uint32_t SubHeightC = aDest.chroma_format_idc <= 1 ? 2 : 1;
  if (ChromaArrayType != 0) {
    CropUnitY *= SubHeightC;
  }

  uint32_t width = aDest.pic_width_in_mbs * 16;
  uint32_t height = aDest.pic_height_in_map_units * 16;
  if (aDest.frame_crop_left_offset <=
          std::numeric_limits<int32_t>::max() / 4 / CropUnitX &&
      aDest.frame_crop_right_offset <=
          std::numeric_limits<int32_t>::max() / 4 / CropUnitX &&
      aDest.frame_crop_top_offset <=
          std::numeric_limits<int32_t>::max() / 4 / CropUnitY &&
      aDest.frame_crop_bottom_offset <=
          std::numeric_limits<int32_t>::max() / 4 / CropUnitY &&
      (aDest.frame_crop_left_offset + aDest.frame_crop_right_offset) *
              CropUnitX <
          width &&
      (aDest.frame_crop_top_offset + aDest.frame_crop_bottom_offset) *
              CropUnitY <
          height) {
    aDest.crop_left = aDest.frame_crop_left_offset * CropUnitX;
    aDest.crop_right = aDest.frame_crop_right_offset * CropUnitX;
    aDest.crop_top = aDest.frame_crop_top_offset * CropUnitY;
    aDest.crop_bottom = aDest.frame_crop_bottom_offset * CropUnitY;
  } else {
    // Nonsensical value, ignore them.
    aDest.crop_left = aDest.crop_right = aDest.crop_top = aDest.crop_bottom = 0;
  }

  aDest.pic_width = width - aDest.crop_left - aDest.crop_right;
  aDest.pic_height = height - aDest.crop_top - aDest.crop_bottom;

  aDest.interlaced = !aDest.frame_mbs_only_flag;

  // Determine display size.
  if (aDest.sample_ratio > 1.0) {
    // Increase the intrinsic width
    aDest.display_width =
        ConditionDimension(aDest.pic_width * aDest.sample_ratio);
    aDest.display_height = aDest.pic_height;
  } else {
    // Increase the intrinsic height
    aDest.display_width = aDest.pic_width;
    aDest.display_height =
        ConditionDimension(aDest.pic_height / aDest.sample_ratio);
  }

  aDest.valid = true;

  return true;
}

/* static */ bool H264::vui_parameters(BitReader& aBr, SPSData& aDest) {
  aDest.aspect_ratio_info_present_flag = aBr.ReadBit();
  if (aDest.aspect_ratio_info_present_flag) {
    aDest.aspect_ratio_idc = aBr.ReadBits(8);
    aDest.sar_width = aDest.sar_height = 0;

    // From E.2.1 VUI parameters semantics (ITU-T H.264 02/2014)
    switch (aDest.aspect_ratio_idc) {
      case 0:
        // Unspecified
        break;
      case 1:
        /*
          1:1
         7680x4320 16:9 frame without horizontal overscan
         3840x2160 16:9 frame without horizontal overscan
         1280x720 16:9 frame without horizontal overscan
         1920x1080 16:9 frame without horizontal overscan (cropped from
         1920x1088) 640x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 1.0f;
        break;
      case 2:
        /*
          12:11
         720x576 4:3 frame with horizontal overscan
         352x288 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 12.0 / 11.0;
        break;
      case 3:
        /*
          10:11
         720x480 4:3 frame with horizontal overscan
         352x240 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 10.0 / 11.0;
        break;
      case 4:
        /*
          16:11
         720x576 16:9 frame with horizontal overscan
         528x576 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 16.0 / 11.0;
        break;
      case 5:
        /*
          40:33
         720x480 16:9 frame with horizontal overscan
         528x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 40.0 / 33.0;
        break;
      case 6:
        /*
          24:11
         352x576 4:3 frame without horizontal overscan
         480x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 24.0 / 11.0;
        break;
      case 7:
        /*
          20:11
         352x480 4:3 frame without horizontal overscan
         480x480 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 20.0 / 11.0;
        break;
      case 8:
        /*
          32:11
         352x576 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 32.0 / 11.0;
        break;
      case 9:
        /*
          80:33
         352x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 80.0 / 33.0;
        break;
      case 10:
        /*
          18:11
         480x576 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 18.0 / 11.0;
        break;
      case 11:
        /*
          15:11
         480x480 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 15.0 / 11.0;
        break;
      case 12:
        /*
          64:33
         528x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 64.0 / 33.0;
        break;
      case 13:
        /*
          160:99
         528x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 160.0 / 99.0;
        break;
      case 14:
        /*
          4:3
         1440x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 4.0 / 3.0;
        break;
      case 15:
        /*
          3:2
         1280x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 3.2 / 2.0;
        break;
      case 16:
        /*
          2:1
         960x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 2.0 / 1.0;
        break;
      case 255:
        /* Extended_SAR */
        aDest.sar_width = aBr.ReadBits(16);
        aDest.sar_height = aBr.ReadBits(16);
        if (aDest.sar_width && aDest.sar_height) {
          aDest.sample_ratio = float(aDest.sar_width) / float(aDest.sar_height);
        }
        break;
      default:
        break;
    }
  }

  if (aBr.ReadBit()) {  // overscan_info_present_flag
    aDest.overscan_appropriate_flag = aBr.ReadBit();
  }

  if (aBr.ReadBit()) {  // video_signal_type_present_flag
    aDest.video_format = aBr.ReadBits(3);
    aDest.video_full_range_flag = aBr.ReadBit();
    aDest.colour_description_present_flag = aBr.ReadBit();
    if (aDest.colour_description_present_flag) {
      aDest.colour_primaries = aBr.ReadBits(8);
      aDest.transfer_characteristics = aBr.ReadBits(8);
      aDest.matrix_coefficients = aBr.ReadBits(8);
    }
  }

  aDest.chroma_loc_info_present_flag = aBr.ReadBit();
  if (aDest.chroma_loc_info_present_flag) {
    BitReader& br = aBr;  // so that macro READUE works
    READUE(chroma_sample_loc_type_top_field, 5);
    READUE(chroma_sample_loc_type_bottom_field, 5);
  }

  bool timing_info_present_flag = aBr.ReadBit();
  if (timing_info_present_flag) {
    aBr.ReadBits(32);  // num_units_in_tick
    aBr.ReadBits(32);  // time_scale
    aBr.ReadBit();     // fixed_frame_rate_flag
  }
  return true;
}

/* static */ bool H264::DecodeSPSFromExtraData(
    const mozilla::MediaByteBuffer* aExtraData, SPSData& aDest) {
  SPSNALIterator it(aExtraData);
  if (!it) {
    return false;
  }
  return (*it).GetSPSData(aDest);
}

/* static */ bool H264::EnsureSPSIsSane(SPSData& aSPS) {
  bool valid = true;
  static const float default_aspect = 4.0f / 3.0f;
  if (aSPS.sample_ratio <= 0.0f || aSPS.sample_ratio > 6.0f) {
    if (aSPS.pic_width && aSPS.pic_height) {
      aSPS.sample_ratio = (float)aSPS.pic_width / (float)aSPS.pic_height;
    } else {
      aSPS.sample_ratio = default_aspect;
    }
    aSPS.display_width = aSPS.pic_width;
    aSPS.display_height = aSPS.pic_height;
    valid = false;
  }
  if (aSPS.max_num_ref_frames > 16) {
    aSPS.max_num_ref_frames = 16;
    valid = false;
  }
  return valid;
}

/* static */ uint32_t H264::ComputeMaxRefFrames(
    const mozilla::MediaByteBuffer* aExtraData) {
  uint32_t maxRefFrames = 4;
  // Retrieve video dimensions from H264 SPS NAL.
  SPSData spsdata;
  if (DecodeSPSFromExtraData(aExtraData, spsdata)) {
    // max_num_ref_frames determines the size of the sliding window
    // we need to queue that many frames in order to guarantee proper
    // pts frames ordering. Use a minimum of 4 to ensure proper playback of
    // non compliant videos.
    maxRefFrames =
        std::min(std::max(maxRefFrames, spsdata.max_num_ref_frames + 1), 16u);
  }
  return maxRefFrames;
}

/* static */ H264::FrameType H264::GetFrameType(
    const mozilla::MediaRawData* aSample) {
  if (!AnnexB::IsAVCC(aSample)) {
    // We must have a valid AVCC frame with extradata.
    return FrameType::INVALID;
  }
  MOZ_ASSERT(aSample->Data());

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;

  BufferReader reader(aSample->Data(), aSample->Size());

  while (reader.Remaining() >= nalLenSize) {
    uint32_t nalLen = 0;
    switch (nalLenSize) {
      case 1:
        nalLen = reader.ReadU8().unwrapOr(0);
        break;
      case 2:
        nalLen = reader.ReadU16().unwrapOr(0);
        break;
      case 3:
        nalLen = reader.ReadU24().unwrapOr(0);
        break;
      case 4:
        nalLen = reader.ReadU32().unwrapOr(0);
        break;
    }
    if (!nalLen) {
      continue;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return FrameType::INVALID;
    }
    int8_t nalType = *p & 0x1f;
    if (nalType == H264_NAL_IDR_SLICE) {
      // IDR NAL.
      return FrameType::I_FRAME;
    } else if (nalType == H264_NAL_SEI) {
      RefPtr<mozilla::MediaByteBuffer> decodedNAL = DecodeNALUnit(p, nalLen);
      SEIRecoveryData data;
      if (DecodeRecoverySEI(decodedNAL, data)) {
        return FrameType::I_FRAME;
      }
    }
  }

  return FrameType::OTHER;
}

/* static */ already_AddRefed<mozilla::MediaByteBuffer> H264::ExtractExtraData(
    const mozilla::MediaRawData* aSample) {
  MOZ_ASSERT(AnnexB::IsAVCC(aSample));

  RefPtr<mozilla::MediaByteBuffer> extradata = new mozilla::MediaByteBuffer;

  // SPS content
  nsTArray<uint8_t> sps;
  ByteWriter spsw(sps);
  int numSps = 0;
  // PPS content
  nsTArray<uint8_t> pps;
  ByteWriter ppsw(pps);
  int numPps = 0;

  int nalLenSize = ((*aSample->mExtraData)[4] & 3) + 1;

  size_t sampleSize = aSample->Size();
  if (aSample->mCrypto.mValid) {
    // The content is encrypted, we can only parse the non-encrypted data.
    MOZ_ASSERT(aSample->mCrypto.mPlainSizes.Length() > 0);
    if (aSample->mCrypto.mPlainSizes.Length() == 0 ||
        aSample->mCrypto.mPlainSizes[0] > sampleSize) {
      // This is invalid content.
      return nullptr;
    }
    sampleSize = aSample->mCrypto.mPlainSizes[0];
  }

  BufferReader reader(aSample->Data(), sampleSize);

  nsTArray<SPSData> SPSTable;
  // If we encounter SPS with the same id but different content, we will stop
  // attempting to detect duplicates.
  bool checkDuplicate = true;

  // Find SPS and PPS NALUs in AVCC data
  while (reader.Remaining() > nalLenSize) {
    uint32_t nalLen = 0;
    switch (nalLenSize) {
      case 1:
        Unused << reader.ReadU8().map(
            [&](uint8_t x) mutable { return nalLen = x; });
        break;
      case 2:
        Unused << reader.ReadU16().map(
            [&](uint16_t x) mutable { return nalLen = x; });
        break;
      case 3:
        Unused << reader.ReadU24().map(
            [&](uint32_t x) mutable { return nalLen = x; });
        break;
      case 4:
        Unused << reader.ReadU32().map(
            [&](uint32_t x) mutable { return nalLen = x; });
        break;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return extradata.forget();
    }
    uint8_t nalType = *p & 0x1f;

    if (nalType == H264_NAL_SPS) {
      RefPtr<mozilla::MediaByteBuffer> sps = DecodeNALUnit(p, nalLen);
      SPSData data;
      if (!DecodeSPS(sps, data)) {
        // Invalid SPS, ignore.
        continue;
      }
      uint8_t spsId = data.seq_parameter_set_id;
      if (spsId >= SPSTable.Length()) {
        if (!SPSTable.SetLength(spsId + 1, fallible)) {
          // OOM.
          return nullptr;
        }
      }
      if (checkDuplicate && SPSTable[spsId].valid && SPSTable[spsId] == data) {
        // Duplicate ignore.
        continue;
      }
      if (SPSTable[spsId].valid) {
        // We already have detected a SPS with this Id. Just to be safe we
        // disable SPS duplicate detection.
        checkDuplicate = false;
      } else {
        SPSTable[spsId] = data;
      }
      numSps++;
      if (!spsw.WriteU16(nalLen) || !spsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    } else if (nalType == H264_NAL_PPS) {
      numPps++;
      if (!ppsw.WriteU16(nalLen) || !ppsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    }
  }

  // We ignore PPS data if we didn't find a SPS as we would be unable to
  // decode it anyway.
  numPps = numSps ? numPps : 0;

  if (numSps && sps.Length() > 5) {
    extradata->AppendElement(1);         // version
    extradata->AppendElement(sps[3]);    // profile
    extradata->AppendElement(sps[4]);    // profile compat
    extradata->AppendElement(sps[5]);    // level
    extradata->AppendElement(0xfc | 3);  // nal size - 1
    extradata->AppendElement(0xe0 | numSps);
    extradata->AppendElements(sps.Elements(), sps.Length());
    extradata->AppendElement(numPps);
    if (numPps) {
      extradata->AppendElements(pps.Elements(), pps.Length());
    }
  }

  return extradata.forget();
}

/* static */ bool H264::HasSPS(const mozilla::MediaByteBuffer* aExtraData) {
  return NumSPS(aExtraData) > 0;
}

/* static */ uint8_t H264::NumSPS(const mozilla::MediaByteBuffer* aExtraData) {
  if (!aExtraData || aExtraData->IsEmpty()) {
    return 0;
  }

  BufferReader reader(aExtraData);
  if (!reader.Read(5)) {
    return 0;
  }
  auto res = reader.ReadU8();
  if (res.isErr()) {
    return 0;
  }
  return res.unwrap() & 0x1f;
}

/* static */ bool H264::CompareExtraData(
    const mozilla::MediaByteBuffer* aExtraData1,
    const mozilla::MediaByteBuffer* aExtraData2) {
  if (aExtraData1 == aExtraData2) {
    return true;
  }
  uint8_t numSPS = NumSPS(aExtraData1);
  if (numSPS == 0 || numSPS != NumSPS(aExtraData2)) {
    return false;
  }

  // We only compare if the SPS are the same as the various H264 decoders can
  // deal with in-band change of PPS.

  SPSNALIterator it1(aExtraData1);
  SPSNALIterator it2(aExtraData2);

  while (it1 && it2) {
    if (*it1 != *it2) {
      return false;
    }
    ++it1;
    ++it2;
  }
  return true;
}

static inline Result<Ok, nsresult> ReadSEIInt(BufferReader& aBr,
                                              uint32_t& aOutput) {
  uint8_t tmpByte;

  aOutput = 0;
  MOZ_TRY_VAR(tmpByte, aBr.ReadU8());
  while (tmpByte == 0xFF) {
    aOutput += 255;
    MOZ_TRY_VAR(tmpByte, aBr.ReadU8());
  }
  aOutput += tmpByte;  // this is the last byte
  return Ok();
}

/* static */ bool H264::DecodeRecoverySEI(const mozilla::MediaByteBuffer* aSEI,
                                          SEIRecoveryData& aDest) {
  if (!aSEI) {
    return false;
  }
  // sei_rbsp() as per 7.3.2.3 Supplemental enhancement information RBSP syntax
  BufferReader br(aSEI);

  do {
    // sei_message() as per
    // 7.3.2.3.1 Supplemental enhancement information message syntax
    uint32_t payloadType = 0;
    if (ReadSEIInt(br, payloadType).isErr()) {
      return false;
    }

    uint32_t payloadSize = 0;
    if (ReadSEIInt(br, payloadSize).isErr()) {
      return false;
    }

    // sei_payload(payloadType, payloadSize) as per
    // D.1 SEI payload syntax.
    const uint8_t* p = br.Read(payloadSize);
    if (!p) {
      return false;
    }
    if (payloadType == 6) {  // SEI_RECOVERY_POINT
      if (payloadSize == 0) {
        // Invalid content, ignore.
        continue;
      }
      // D.1.7 Recovery point SEI message syntax
      BitReader br(p, payloadSize * 8);
      aDest.recovery_frame_cnt = br.ReadUE();
      aDest.exact_match_flag = br.ReadBit();
      aDest.broken_link_flag = br.ReadBit();
      aDest.changing_slice_group_idc = br.ReadBits(2);
      return true;
    }
  } while (br.PeekU8().isOk() &&
           br.PeekU8().unwrap() !=
               0x80);  // more_rbsp_data() msg[offset] != 0x80
  // ignore the trailing bits rbsp_trailing_bits();
  return false;
}

/*static */ already_AddRefed<mozilla::MediaByteBuffer> H264::CreateExtraData(
    uint8_t aProfile, uint8_t aConstraints, uint8_t aLevel,
    const gfx::IntSize& aSize) {
  // SPS of a 144p video.
  const uint8_t originSPS[] = {0x4d, 0x40, 0x0c, 0xe8, 0x80, 0x80, 0x9d,
                               0x80, 0xb5, 0x01, 0x01, 0x01, 0x40, 0x00,
                               0x00, 0x00, 0x40, 0x00, 0x00, 0x0f, 0x03,
                               0xc5, 0x0a, 0x44, 0x80};

  RefPtr<MediaByteBuffer> extraData = new MediaByteBuffer();
  extraData->AppendElements(originSPS, sizeof(originSPS));
  BitReader br(extraData, BitReader::GetBitLength(extraData));

  RefPtr<MediaByteBuffer> sps = new MediaByteBuffer();
  BitWriter bw(sps);

  br.ReadBits(8);  // Skip original profile_idc
  bw.WriteU8(aProfile);
  br.ReadBits(8);  // Skip original constraint flags && reserved_zero_2bits
  aConstraints =
      aConstraints & ~0x3;  // Ensure reserved_zero_2bits are set to 0
  bw.WriteBits(aConstraints, 8);
  br.ReadBits(8);  // Skip original level_idc
  bw.WriteU8(aLevel);
  bw.WriteUE(br.ReadUE());  // seq_parameter_set_id (0 stored on 1 bit)

  if (aProfile == 100 || aProfile == 110 || aProfile == 122 ||
      aProfile == 244 || aProfile == 44 || aProfile == 83 || aProfile == 86 ||
      aProfile == 118 || aProfile == 128 || aProfile == 138 ||
      aProfile == 139 || aProfile == 134) {
    bw.WriteUE(1);  // chroma_format_idc -> always set to 4:2:0 chroma format
    bw.WriteUE(0);  // bit_depth_luma_minus8 -> always 8 bits here
    bw.WriteUE(0);  // bit_depth_chroma_minus8 -> always 8 bits here
  }

  bw.WriteBits(br.ReadBits(11),
               11);  // log2_max_frame_num to gaps_in_frame_num_allowed_flag

  // skip over original exp-golomb encoded width/height
  br.ReadUE();  // skip width
  br.ReadUE();  // skip height
  uint32_t width = aSize.width;
  uint32_t widthNeeded = width % 16 != 0 ? (width / 16 + 1) * 16 : width;
  uint32_t height = aSize.height;
  uint32_t heightNeeded = height % 16 != 0 ? (height / 16 + 1) * 16 : height;
  bw.WriteUE(widthNeeded / 16 - 1);
  bw.WriteUE(heightNeeded / 16 - 1);
  bw.WriteBit(br.ReadBit());  // write frame_mbs_only_flag
  bw.WriteBit(br.ReadBit());  // write direct_8x8_inference_flag;
  if (widthNeeded != width || heightNeeded != height) {
    // Write cropping value
    bw.WriteBit(true);                        // skip frame_cropping_flag
    bw.WriteUE(0);                            // frame_crop_left_offset
    bw.WriteUE((widthNeeded - width) / 2);    // frame_crop_right_offset
    bw.WriteUE(0);                            // frame_crop_top_offset
    bw.WriteUE((heightNeeded - height) / 2);  // frame_crop_bottom_offset
  } else {
    bw.WriteBit(false);  // skip frame_cropping_flag
  }
  br.ReadBit();  // skip frame_cropping_flag;
  // Write the remainings of the original sps (vui_parameters which sets an
  // aspect ration of 1.0)
  while (br.BitsLeft()) {
    bw.WriteBit(br.ReadBit());
  }
  bw.CloseWithRbspTrailing();

  RefPtr<MediaByteBuffer> encodedSPS =
      EncodeNALUnit(sps->Elements(), sps->Length());
  extraData->Clear();
  extraData->AppendElement(1);
  extraData->AppendElement(aProfile);
  extraData->AppendElement(aConstraints);
  extraData->AppendElement(aLevel);
  extraData->AppendElement(3);  // nalLENSize-1
  extraData->AppendElement(1);  // numPPS
  uint8_t c[2];
  mozilla::BigEndian::writeUint16(&c[0], encodedSPS->Length() + 1);
  extraData->AppendElements(c, 2);
  extraData->AppendElement((0x00 << 7) | (0x3 << 5) | H264_NAL_SPS);
  extraData->AppendElements(*encodedSPS);

  const uint8_t PPS[] = {0xeb, 0xef, 0x20};

  extraData->AppendElement(1);  // numPPS
  mozilla::BigEndian::writeUint16(&c[0], sizeof(PPS) + 1);
  extraData->AppendElements(c, 2);
  extraData->AppendElement((0x00 << 7) | (0x3 << 5) | H264_NAL_PPS);
  extraData->AppendElements(PPS, sizeof(PPS));

  return extraData.forget();
}

#undef READUE
#undef READSE

}  // namespace mozilla
