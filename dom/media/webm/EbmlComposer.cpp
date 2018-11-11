/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EbmlComposer.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/EndianUtils.h"
#include "libmkv/EbmlIDs.h"
#include "libmkv/EbmlWriter.h"
#include "libmkv/WebMElement.h"
#include "prtime.h"
#include "limits.h"

namespace mozilla {

// Timecode scale in nanoseconds
static const unsigned long TIME_CODE_SCALE = 1000000;
// The WebM header size without audio CodecPrivateData
static const int32_t DEFAULT_HEADER_SIZE = 1024;

void EbmlComposer::GenerateHeader()
{
  // Write the EBML header.
  EbmlGlobal ebml;
  // The WEbM header default size usually smaller than 1k.
  auto buffer = MakeUnique<uint8_t[]>(DEFAULT_HEADER_SIZE +
                                      mCodecPrivateData.Length());
  ebml.buf = buffer.get();
  ebml.offset = 0;
  writeHeader(&ebml);
  {
    EbmlLoc segEbmlLoc, ebmlLocseg, ebmlLoc;
    Ebml_StartSubElement(&ebml, &segEbmlLoc, Segment);
    {
      Ebml_StartSubElement(&ebml, &ebmlLocseg, SeekHead);
      // Todo: We don't know the exact sizes of encoded data and
      // ignore this section.
      Ebml_EndSubElement(&ebml, &ebmlLocseg);
      writeSegmentInformation(&ebml, &ebmlLoc, TIME_CODE_SCALE, 0);
      {
        EbmlLoc trackLoc;
        Ebml_StartSubElement(&ebml, &trackLoc, Tracks);
        {
          // Video
          if (mWidth > 0 && mHeight > 0) {
            writeVideoTrack(&ebml, 0x1, 0, "V_VP8",
                            mWidth, mHeight,
                            mDisplayWidth, mDisplayHeight, mFrameRate);
          }
          // Audio
          if (mCodecPrivateData.Length() > 0) {
            // Extract the pre-skip from mCodecPrivateData
            // then convert it to nanoseconds.
            // Details in OpusTrackEncoder.cpp.
            mCodecDelay =
              (uint64_t)LittleEndian::readUint16(mCodecPrivateData.Elements() + 10)
              * PR_NSEC_PER_SEC / 48000;
            // Fixed 80ms, convert into nanoseconds.
            uint64_t seekPreRoll = 80 * PR_NSEC_PER_MSEC;
            writeAudioTrack(&ebml, 0x2, 0x0, "A_OPUS", mSampleFreq,
                            mChannels, mCodecDelay, seekPreRoll,
                            mCodecPrivateData.Elements(),
                            mCodecPrivateData.Length());
          }
        }
        Ebml_EndSubElement(&ebml, &trackLoc);
      }
    }
    // The Recording length is unknown and
    // ignore write the whole Segment element size
  }
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE + mCodecPrivateData.Length(),
             "write more data > EBML_BUFFER_SIZE");
  auto block = mClusterBuffs.AppendElement();
  block->SetLength(ebml.offset);
  memcpy(block->Elements(), ebml.buf, ebml.offset);
  mFlushState |= FLUSH_METADATA;
}

void EbmlComposer::FinishMetadata()
{
  if (mFlushState & FLUSH_METADATA) {
    // We don't remove the first element of mClusterBuffs because the
    // |mClusterHeaderIndex| may have value.
    mClusterCanFlushBuffs.AppendElement()->SwapElements(mClusterBuffs[0]);
    mFlushState &= ~FLUSH_METADATA;
  }
}

void EbmlComposer::FinishCluster()
{
  FinishMetadata();
  if (!(mFlushState & FLUSH_CLUSTER)) {
    // No completed cluster available.
    return;
  }

  MOZ_ASSERT(mClusterLengthLoc > 0);
  EbmlGlobal ebml;
  EbmlLoc ebmlLoc;
  ebmlLoc.offset = mClusterLengthLoc;
  ebml.offset = 0;
  for (uint32_t i = mClusterHeaderIndex; i < mClusterBuffs.Length(); i++) {
    ebml.offset += mClusterBuffs[i].Length();
  }
  ebml.buf = mClusterBuffs[mClusterHeaderIndex].Elements();
  Ebml_EndSubElement(&ebml, &ebmlLoc);
  // Move the mClusterBuffs data from mClusterHeaderIndex that we can skip
  // the metadata and the rest P-frames after ContainerWriter::FLUSH_NEEDED.
  for (uint32_t i = mClusterHeaderIndex; i < mClusterBuffs.Length(); i++) {
    mClusterCanFlushBuffs.AppendElement()->SwapElements(mClusterBuffs[i]);
  }

  mClusterHeaderIndex = 0;
  mClusterLengthLoc = 0;
  mClusterBuffs.Clear();
  mFlushState &= ~FLUSH_CLUSTER;
}

void
EbmlComposer::WriteSimpleBlock(EncodedFrame* aFrame)
{
  EbmlGlobal ebml;
  ebml.offset = 0;

  auto frameType = aFrame->GetFrameType();
  bool flush = false;
  bool isVP8IFrame = (frameType == EncodedFrame::FrameType::VP8_I_FRAME);
  if (isVP8IFrame) {
    FinishCluster();
    flush = true;
  } else {
    // Force it to calculate timecode using signed math via cast
    int64_t timeCode = (aFrame->GetTimeStamp() / ((int) PR_USEC_PER_MSEC) - mClusterTimecode) +
                       (mCodecDelay / PR_NSEC_PER_MSEC);
    if (timeCode < SHRT_MIN || timeCode > SHRT_MAX ) {
      // We're probably going to overflow (or underflow) the timeCode value later!
      FinishCluster();
      flush = true;
    }
  }

  auto block = mClusterBuffs.AppendElement();
  block->SetLength(aFrame->GetFrameData().Length() + DEFAULT_HEADER_SIZE);
  ebml.buf = block->Elements();

  if (flush) {
    EbmlLoc ebmlLoc;
    Ebml_StartSubElement(&ebml, &ebmlLoc, Cluster);
    MOZ_ASSERT(mClusterBuffs.Length() > 0);
    // current cluster header array index
    mClusterHeaderIndex = mClusterBuffs.Length() - 1;
    mClusterLengthLoc = ebmlLoc.offset;
    // if timeCode didn't under/overflow before, it shouldn't after this
    mClusterTimecode = aFrame->GetTimeStamp() / PR_USEC_PER_MSEC;
    Ebml_SerializeUnsigned(&ebml, Timecode, mClusterTimecode);
    mFlushState |= FLUSH_CLUSTER;
  }

  bool isOpus = (frameType == EncodedFrame::FrameType::OPUS_AUDIO_FRAME);
  // Can't underflow/overflow now
  int64_t timeCode = aFrame->GetTimeStamp() / ((int) PR_USEC_PER_MSEC) - mClusterTimecode;
  if (isOpus) {
    timeCode += mCodecDelay / PR_NSEC_PER_MSEC;
  }
  MOZ_ASSERT(timeCode >= SHRT_MIN && timeCode <= SHRT_MAX);
  writeSimpleBlock(&ebml, isOpus ? 0x2 : 0x1, static_cast<short>(timeCode), isVP8IFrame,
                   0, 0, (unsigned char*)aFrame->GetFrameData().Elements(),
                   aFrame->GetFrameData().Length());
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE +
             aFrame->GetFrameData().Length(),
             "write more data > EBML_BUFFER_SIZE");
  block->SetLength(ebml.offset);
}

void
EbmlComposer::SetVideoConfig(uint32_t aWidth, uint32_t aHeight,
                             uint32_t aDisplayWidth, uint32_t aDisplayHeight,
                             float aFrameRate)
{
  MOZ_ASSERT(aWidth > 0, "Width should > 0");
  MOZ_ASSERT(aHeight > 0, "Height should > 0");
  MOZ_ASSERT(aDisplayWidth > 0, "DisplayWidth should > 0");
  MOZ_ASSERT(aDisplayHeight > 0, "DisplayHeight should > 0");
  MOZ_ASSERT(aFrameRate > 0, "FrameRate should > 0");
  mWidth = aWidth;
  mHeight = aHeight;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;
  mFrameRate = aFrameRate;
}

void
EbmlComposer::SetAudioConfig(uint32_t aSampleFreq, uint32_t aChannels)
{
  MOZ_ASSERT(aSampleFreq > 0, "SampleFreq should > 0");
  MOZ_ASSERT(aChannels > 0, "Channels should > 0");
  mSampleFreq = aSampleFreq;
  mChannels = aChannels;
}

void
EbmlComposer::ExtractBuffer(nsTArray<nsTArray<uint8_t> >* aDestBufs,
                            uint32_t aFlag)
{
  if ((aFlag & ContainerWriter::FLUSH_NEEDED) ||
      (aFlag & ContainerWriter::GET_HEADER))
  {
    FinishMetadata();
  }
  if (aFlag & ContainerWriter::FLUSH_NEEDED)
  {
    FinishCluster();
  }
  // aDestBufs may have some element
  for (uint32_t i = 0; i < mClusterCanFlushBuffs.Length(); i++) {
    aDestBufs->AppendElement()->SwapElements(mClusterCanFlushBuffs[i]);
  }
  mClusterCanFlushBuffs.Clear();
}

EbmlComposer::EbmlComposer()
  : mFlushState(FLUSH_NONE)
  , mClusterHeaderIndex(0)
  , mClusterLengthLoc(0)
  , mCodecDelay(0)
  , mClusterTimecode(0)
  , mWidth(0)
  , mHeight(0)
  , mFrameRate(0)
  , mSampleFreq(0)
  , mChannels(0)
{}

} // namespace mozilla
