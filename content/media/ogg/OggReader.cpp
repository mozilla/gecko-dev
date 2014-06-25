/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsError.h"
#include "MediaDecoderStateMachine.h"
#include "MediaDecoder.h"
#include "OggReader.h"
#include "VideoUtils.h"
#include "theora/theoradec.h"
#include <algorithm>
#ifdef MOZ_OPUS
#include "opus/opus.h"
extern "C" {
#include "opus/opus_multistream.h"
}
#endif
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/TimeStamp.h"
#include "VorbisUtils.h"
#include "MediaMetadataManager.h"
#include "nsISeekableStream.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;

namespace mozilla {

// On B2G estimate the buffered ranges rather than calculating them explicitly.
// This prevents us doing I/O on the main thread, which is prohibited in B2G.
#ifdef MOZ_WIDGET_GONK
#define OGG_ESTIMATE_BUFFERED 1
#endif

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#ifdef SEEK_LOGGING
#define SEEK_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define SEEK_LOG(type, msg)
#endif
#else
#define LOG(type, msg)
#define SEEK_LOG(type, msg)
#endif

// The number of microseconds of "fuzz" we use in a bisection search over
// HTTP. When we're seeking with fuzz, we'll stop the search if a bisection
// lands between the seek target and SEEK_FUZZ_USECS microseconds before the
// seek target.  This is becaue it's usually quicker to just keep downloading
// from an exisiting connection than to do another bisection inside that
// small range, which would open a new HTTP connetion.
static const uint32_t SEEK_FUZZ_USECS = 500000;

// The number of microseconds of "pre-roll" we use for Opus streams.
// The specification recommends 80 ms.
#ifdef MOZ_OPUS
static const int64_t SEEK_OPUS_PREROLL = 80 * USECS_PER_MS;
#endif /* MOZ_OPUS */

enum PageSyncResult {
  PAGE_SYNC_ERROR = 1,
  PAGE_SYNC_END_OF_RANGE= 2,
  PAGE_SYNC_OK = 3
};

// Reads a page from the media resource.
static PageSyncResult
PageSync(MediaResource* aResource,
         ogg_sync_state* aState,
         bool aCachedDataOnly,
         int64_t aOffset,
         int64_t aEndOffset,
         ogg_page* aPage,
         int& aSkippedBytes);

// Chunk size to read when reading Ogg files. Average Ogg page length
// is about 4300 bytes, so we read the file in chunks larger than that.
static const int PAGE_STEP = 8192;

OggReader::OggReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mMonitor("OggReader"),
    mTheoraState(nullptr),
    mVorbisState(nullptr),
#ifdef MOZ_OPUS
    mOpusState(nullptr),
    mOpusEnabled(MediaDecoder::IsOpusEnabled()),
#endif /* MOZ_OPUS */
    mSkeletonState(nullptr),
    mVorbisSerial(0),
    mOpusSerial(0),
    mTheoraSerial(0),
    mOpusPreSkip(0),
    mIsChained(false),
    mDecodedAudioFrames(0)
{
  MOZ_COUNT_CTOR(OggReader);
  memset(&mTheoraInfo, 0, sizeof(mTheoraInfo));
}

OggReader::~OggReader()
{
  ogg_sync_clear(&mOggState);
  MOZ_COUNT_DTOR(OggReader);
}

nsresult OggReader::Init(MediaDecoderReader* aCloneDonor) {
  int ret = ogg_sync_init(&mOggState);
  NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult OggReader::ResetDecode()
{
  return ResetDecode(false);
}

nsresult OggReader::ResetDecode(bool start)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult res = NS_OK;

  if (NS_FAILED(MediaDecoderReader::ResetDecode())) {
    res = NS_ERROR_FAILURE;
  }

  // Discard any previously buffered packets/pages.
  ogg_sync_reset(&mOggState);
  if (mVorbisState && NS_FAILED(mVorbisState->Reset())) {
    res = NS_ERROR_FAILURE;
  }
#ifdef MOZ_OPUS
  if (mOpusState && NS_FAILED(mOpusState->Reset(start))) {
    res = NS_ERROR_FAILURE;
  }
#endif /* MOZ_OPUS */
  if (mTheoraState && NS_FAILED(mTheoraState->Reset())) {
    res = NS_ERROR_FAILURE;
  }

  return res;
}

bool OggReader::ReadHeaders(OggCodecState* aState)
{
  while (!aState->DoneReadingHeaders()) {
    ogg_packet* packet = NextOggPacket(aState);
    // DecodeHeader is responsible for releasing packet.
    if (!packet || !aState->DecodeHeader(packet)) {
      aState->Deactivate();
      return false;
    }
  }
  return aState->Init();
}

void OggReader::BuildSerialList(nsTArray<uint32_t>& aTracks)
{
  if (HasVideo()) {
    aTracks.AppendElement(mTheoraState->mSerial);
  }
  if (HasAudio()) {
    if (mVorbisState) {
      aTracks.AppendElement(mVorbisState->mSerial);
#ifdef MOZ_OPUS
    } else if (mOpusState) {
      aTracks.AppendElement(mOpusState->mSerial);
#endif /* MOZ_OPUS */
    }
  }
}

nsresult OggReader::ReadMetadata(MediaInfo* aInfo,
                                 MetadataTags** aTags)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // We read packets until all bitstreams have read all their header packets.
  // We record the offset of the first non-header page so that we know
  // what page to seek to when seeking to the media start.

  NS_ASSERTION(aTags, "Called with null MetadataTags**.");
  *aTags = nullptr;

  ogg_page page;
  nsAutoTArray<OggCodecState*,4> bitstreams;
  bool readAllBOS = false;
  while (!readAllBOS) {
    if (!ReadOggPage(&page)) {
      // Some kind of error...
      break;
    }

    int serial = ogg_page_serialno(&page);
    OggCodecState* codecState = 0;

    if (!ogg_page_bos(&page)) {
      // We've encountered a non Beginning Of Stream page. No more BOS pages
      // can follow in this Ogg segment, so there will be no other bitstreams
      // in the Ogg (unless it's invalid).
      readAllBOS = true;
    } else if (!mCodecStore.Contains(serial)) {
      // We've not encountered a stream with this serial number before. Create
      // an OggCodecState to demux it, and map that to the OggCodecState
      // in mCodecStates.
      codecState = OggCodecState::Create(&page);
      mCodecStore.Add(serial, codecState);
      bitstreams.AppendElement(codecState);
      if (codecState &&
          codecState->GetType() == OggCodecState::TYPE_VORBIS &&
          !mVorbisState)
      {
        // First Vorbis bitstream, we'll play this one. Subsequent Vorbis
        // bitstreams will be ignored.
        mVorbisState = static_cast<VorbisState*>(codecState);
      }
      if (codecState &&
          codecState->GetType() == OggCodecState::TYPE_THEORA &&
          !mTheoraState)
      {
        // First Theora bitstream, we'll play this one. Subsequent Theora
        // bitstreams will be ignored.
        mTheoraState = static_cast<TheoraState*>(codecState);
      }
#ifdef MOZ_OPUS
      if (codecState &&
          codecState->GetType() == OggCodecState::TYPE_OPUS &&
          !mOpusState)
      {
        if (mOpusEnabled) {
          mOpusState = static_cast<OpusState*>(codecState);
        } else {
          NS_WARNING("Opus decoding disabled."
                     " See media.opus.enabled in about:config");
        }
      }
#endif /* MOZ_OPUS */
      if (codecState &&
          codecState->GetType() == OggCodecState::TYPE_SKELETON &&
          !mSkeletonState)
      {
        mSkeletonState = static_cast<SkeletonState*>(codecState);
      }
    }

    codecState = mCodecStore.Get(serial);
    NS_ENSURE_TRUE(codecState != nullptr, NS_ERROR_FAILURE);

    if (NS_FAILED(codecState->PageIn(&page))) {
      return NS_ERROR_FAILURE;
    }
  }

  // We've read all BOS pages, so we know the streams contained in the media.
  // Now process all available header packets in the active Theora, Vorbis and
  // Skeleton streams.

  // Deactivate any non-primary bitstreams.
  for (uint32_t i = 0; i < bitstreams.Length(); i++) {
    OggCodecState* s = bitstreams[i];
    if (s != mVorbisState &&
#ifdef MOZ_OPUS
        s != mOpusState &&
#endif /* MOZ_OPUS */
        s != mTheoraState && s != mSkeletonState) {
      s->Deactivate();
    }
  }

  if (mTheoraState && ReadHeaders(mTheoraState)) {
    nsIntRect picture = nsIntRect(mTheoraState->mInfo.pic_x,
                                  mTheoraState->mInfo.pic_y,
                                  mTheoraState->mInfo.pic_width,
                                  mTheoraState->mInfo.pic_height);

    nsIntSize displaySize = nsIntSize(mTheoraState->mInfo.pic_width,
                                      mTheoraState->mInfo.pic_height);

    // Apply the aspect ratio to produce the intrinsic display size we report
    // to the element.
    ScaleDisplayByAspectRatio(displaySize, mTheoraState->mPixelAspectRatio);

    nsIntSize frameSize(mTheoraState->mInfo.frame_width,
                        mTheoraState->mInfo.frame_height);
    if (IsValidVideoRegion(frameSize, picture, displaySize)) {
      // Video track's frame sizes will not overflow. Activate the video track.
      mInfo.mVideo.mHasVideo = true;
      mInfo.mVideo.mDisplay = displaySize;
      mPicture = picture;

      VideoFrameContainer* container = mDecoder->GetVideoFrameContainer();
      if (container) {
        container->SetCurrentFrame(gfxIntSize(displaySize.width, displaySize.height),
                                   nullptr,
                                   TimeStamp::Now());
      }

      // Copy Theora info data for time computations on other threads.
      memcpy(&mTheoraInfo, &mTheoraState->mInfo, sizeof(mTheoraInfo));
      mTheoraSerial = mTheoraState->mSerial;
    }
  }

  if (mVorbisState && ReadHeaders(mVorbisState)) {
    mInfo.mAudio.mHasAudio = true;
    mInfo.mAudio.mRate = mVorbisState->mInfo.rate;
    mInfo.mAudio.mChannels = mVorbisState->mInfo.channels;
    // Copy Vorbis info data for time computations on other threads.
    memcpy(&mVorbisInfo, &mVorbisState->mInfo, sizeof(mVorbisInfo));
    mVorbisInfo.codec_setup = nullptr;
    mVorbisSerial = mVorbisState->mSerial;
    *aTags = mVorbisState->GetTags();
  } else {
    memset(&mVorbisInfo, 0, sizeof(mVorbisInfo));
  }
#ifdef MOZ_OPUS
  if (mOpusState && ReadHeaders(mOpusState)) {
    mInfo.mAudio.mHasAudio = true;
    mInfo.mAudio.mRate = mOpusState->mRate;
    mInfo.mAudio.mChannels = mOpusState->mChannels;
    mOpusSerial = mOpusState->mSerial;
    mOpusPreSkip = mOpusState->mPreSkip;

    *aTags = mOpusState->GetTags();
  }
#endif
  if (mSkeletonState) {
    if (!HasAudio() && !HasVideo()) {
      // We have a skeleton track, but no audio or video, may as well disable
      // the skeleton, we can't do anything useful with this media.
      mSkeletonState->Deactivate();
    } else if (ReadHeaders(mSkeletonState) && mSkeletonState->HasIndex()) {
      // Extract the duration info out of the index, so we don't need to seek to
      // the end of resource to get it.
      nsAutoTArray<uint32_t, 2> tracks;
      BuildSerialList(tracks);
      int64_t duration = 0;
      if (NS_SUCCEEDED(mSkeletonState->GetDuration(tracks, duration))) {
        ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
        mDecoder->SetMediaDuration(duration);
        LOG(PR_LOG_DEBUG, ("Got duration from Skeleton index %lld", duration));
      }
    }
  }

  if (HasAudio() || HasVideo()) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

    MediaResource* resource = mDecoder->GetResource();
    if (mDecoder->GetMediaDuration() == -1 &&
        !mDecoder->IsShutdown() &&
        resource->GetLength() >= 0 &&
        mDecoder->IsMediaSeekable())
    {
      // We didn't get a duration from the index or a Content-Duration header.
      // Seek to the end of file to find the end time.
      mDecoder->GetResource()->StartSeekingForMetadata();
      int64_t length = resource->GetLength();

      NS_ASSERTION(length > 0, "Must have a content length to get end time");

      int64_t endTime = 0;
      {
        ReentrantMonitorAutoExit exitMon(mDecoder->GetReentrantMonitor());
        endTime = RangeEndTime(length);
      }
      if (endTime != -1) {
        mDecoder->SetMediaEndTime(endTime);
        LOG(PR_LOG_DEBUG, ("Got Ogg duration from seeking to end %lld", endTime));
      }
      mDecoder->GetResource()->EndSeekingForMetadata();
    }
  } else {
    return NS_ERROR_FAILURE;
  }
  *aInfo = mInfo;

  return NS_OK;
}

bool
OggReader::IsMediaSeekable()
{
  if (mIsChained) {
    return false;
  }
  return true;
}

nsresult OggReader::DecodeVorbis(ogg_packet* aPacket) {
  NS_ASSERTION(aPacket->granulepos != -1, "Must know vorbis granulepos!");

  if (vorbis_synthesis(&mVorbisState->mBlock, aPacket) != 0) {
    return NS_ERROR_FAILURE;
  }
  if (vorbis_synthesis_blockin(&mVorbisState->mDsp,
                               &mVorbisState->mBlock) != 0)
  {
    return NS_ERROR_FAILURE;
  }

  VorbisPCMValue** pcm = 0;
  int32_t frames = 0;
  uint32_t channels = mVorbisState->mInfo.channels;
  ogg_int64_t endFrame = aPacket->granulepos;
  while ((frames = vorbis_synthesis_pcmout(&mVorbisState->mDsp, &pcm)) > 0) {
    mVorbisState->ValidateVorbisPacketSamples(aPacket, frames);
    nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames * channels]);
    for (uint32_t j = 0; j < channels; ++j) {
      VorbisPCMValue* channel = pcm[j];
      for (uint32_t i = 0; i < uint32_t(frames); ++i) {
        buffer[i*channels + j] = MOZ_CONVERT_VORBIS_SAMPLE(channel[i]);
      }
    }

    // No channel mapping for more than 8 channels.
    if (channels > 8) {
      return NS_ERROR_FAILURE;
    }

    int64_t duration = mVorbisState->Time((int64_t)frames);
    int64_t startTime = mVorbisState->Time(endFrame - frames);
    mAudioQueue.Push(new AudioData(mDecoder->GetResource()->Tell(),
                                   startTime,
                                   duration,
                                   frames,
                                   buffer.forget(),
                                   channels));

    mDecodedAudioFrames += frames;

    endFrame -= frames;
    if (vorbis_synthesis_read(&mVorbisState->mDsp, frames) != 0) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}
#ifdef MOZ_OPUS
nsresult OggReader::DecodeOpus(ogg_packet* aPacket) {
  NS_ASSERTION(aPacket->granulepos != -1, "Must know opus granulepos!");

  // Maximum value is 63*2880, so there's no chance of overflow.
  int32_t frames_number = opus_packet_get_nb_frames(aPacket->packet,
                                                    aPacket->bytes);
  if (frames_number <= 0)
    return NS_ERROR_FAILURE; // Invalid packet header.
  int32_t samples = opus_packet_get_samples_per_frame(aPacket->packet,
                                                      (opus_int32) mOpusState->mRate);
  int32_t frames = frames_number*samples;

  // A valid Opus packet must be between 2.5 and 120 ms long.
  if (frames < 120 || frames > 5760)
    return NS_ERROR_FAILURE;
  uint32_t channels = mOpusState->mChannels;
  nsAutoArrayPtr<AudioDataValue> buffer(new AudioDataValue[frames * channels]);

  // Decode to the appropriate sample type.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  int ret = opus_multistream_decode_float(mOpusState->mDecoder,
                                          aPacket->packet, aPacket->bytes,
                                          buffer, frames, false);
#else
  int ret = opus_multistream_decode(mOpusState->mDecoder,
                                    aPacket->packet, aPacket->bytes,
                                    buffer, frames, false);
#endif
  if (ret < 0)
    return NS_ERROR_FAILURE;
  NS_ASSERTION(ret == frames, "Opus decoded too few audio samples");

  int64_t endFrame = aPacket->granulepos;
  int64_t startFrame;
  // If this is the last packet, perform end trimming.
  if (aPacket->e_o_s && mOpusState->mPrevPacketGranulepos != -1) {
    startFrame = mOpusState->mPrevPacketGranulepos;
    frames = static_cast<int32_t>(std::max(static_cast<int64_t>(0),
                                         std::min(endFrame - startFrame,
                                                static_cast<int64_t>(frames))));
  } else {
    startFrame = endFrame - frames;
  }

  // Trim the initial frames while the decoder is settling.
  if (mOpusState->mSkip > 0) {
    int32_t skipFrames = std::min(mOpusState->mSkip, frames);
    if (skipFrames == frames) {
      // discard the whole packet
      mOpusState->mSkip -= frames;
      LOG(PR_LOG_DEBUG, ("Opus decoder skipping %d frames"
                         " (whole packet)", frames));
      return NS_OK;
    }
    int32_t keepFrames = frames - skipFrames;
    int samples = keepFrames * channels;
    nsAutoArrayPtr<AudioDataValue> trimBuffer(new AudioDataValue[samples]);
    for (int i = 0; i < samples; i++)
      trimBuffer[i] = buffer[skipFrames*channels + i];

    startFrame = endFrame - keepFrames;
    frames = keepFrames;
    buffer = trimBuffer;

    mOpusState->mSkip -= skipFrames;
    LOG(PR_LOG_DEBUG, ("Opus decoder skipping %d frames", skipFrames));
  }
  // Save this packet's granule position in case we need to perform end
  // trimming on the next packet.
  mOpusState->mPrevPacketGranulepos = endFrame;

  // Apply the header gain if one was specified.
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  if (mOpusState->mGain != 1.0f) {
    float gain = mOpusState->mGain;
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
      buffer[i] *= gain;
    }
  }
#else
  if (mOpusState->mGain_Q16 != 65536) {
    int64_t gain_Q16 = mOpusState->mGain_Q16;
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
      int32_t val = static_cast<int32_t>((gain_Q16*buffer[i] + 32768)>>16);
      buffer[i] = static_cast<AudioDataValue>(MOZ_CLIP_TO_15(val));
    }
  }
#endif

  // No channel mapping for more than 8 channels.
  if (channels > 8) {
    return NS_ERROR_FAILURE;
  }

  LOG(PR_LOG_DEBUG, ("Opus decoder pushing %d frames", frames));
  int64_t startTime = mOpusState->Time(startFrame);
  int64_t endTime = mOpusState->Time(endFrame);
  mAudioQueue.Push(new AudioData(mDecoder->GetResource()->Tell(),
                                 startTime,
                                 endTime - startTime,
                                 frames,
                                 buffer.forget(),
                                 channels));

  mDecodedAudioFrames += frames;

  return NS_OK;
}
#endif /* MOZ_OPUS */

bool OggReader::DecodeAudioData()
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  DebugOnly<bool> haveCodecState = mVorbisState != nullptr
#ifdef MOZ_OPUS
    || mOpusState != nullptr
#endif /* MOZ_OPUS */
    ;
  NS_ASSERTION(haveCodecState, "Need audio codec state to decode audio");

  // Read the next data packet. Skip any non-data packets we encounter.
  ogg_packet* packet = 0;
  OggCodecState* codecState;
  if (mVorbisState)
    codecState = static_cast<OggCodecState*>(mVorbisState);
#ifdef MOZ_OPUS
  else
    codecState = static_cast<OggCodecState*>(mOpusState);
#endif /* MOZ_OPUS */
  do {
    if (packet) {
      OggCodecState::ReleasePacket(packet);
    }
    packet = NextOggPacket(codecState);
  } while (packet && codecState->IsHeader(packet));

  if (!packet) {
    return false;
  }

  NS_ASSERTION(packet && packet->granulepos != -1,
    "Must have packet with known granulepos");
  nsAutoRef<ogg_packet> autoRelease(packet);
  if (mVorbisState) {
    DecodeVorbis(packet);
#ifdef MOZ_OPUS
  } else if (mOpusState) {
    DecodeOpus(packet);
#endif
  }

  if ((packet->e_o_s) && (!ReadOggChain())) {
    // We've encountered an end of bitstream packet, or we've hit the end of
    // file while trying to decode, so inform the audio queue that there'll
    // be no more samples.
    return false;
  }

  return true;
}

void OggReader::SetChained(bool aIsChained) {
  {
    ReentrantMonitorAutoEnter mon(mMonitor);
    mIsChained = aIsChained;
  }
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaSeekable(false);
  }
}

bool OggReader::ReadOggChain()
{
  bool chained = false;
#ifdef MOZ_OPUS
  OpusState* newOpusState = nullptr;
#endif /* MOZ_OPUS */
  VorbisState* newVorbisState = nullptr;
  int channels = 0;
  long rate = 0;
  MetadataTags* tags = nullptr;

  if (HasVideo() || HasSkeleton() || !HasAudio()) {
    return false;
  }

  ogg_page page;
  if (!ReadOggPage(&page) || !ogg_page_bos(&page)) {
    return false;
  }

  int serial = ogg_page_serialno(&page);
  if (mCodecStore.Contains(serial)) {
    return false;
  }

  nsAutoPtr<OggCodecState> codecState;
  codecState = OggCodecState::Create(&page);
  if (!codecState) {
    return false;
  }

  if (mVorbisState && (codecState->GetType() == OggCodecState::TYPE_VORBIS)) {
    newVorbisState = static_cast<VorbisState*>(codecState.get());
  }
#ifdef MOZ_OPUS
  else if (mOpusState && (codecState->GetType() == OggCodecState::TYPE_OPUS)) {
    newOpusState = static_cast<OpusState*>(codecState.get());
  }
#endif
  else {
    return false;
  }
  OggCodecState* state;

  mCodecStore.Add(serial, codecState.forget());
  state = mCodecStore.Get(serial);

  NS_ENSURE_TRUE(state != nullptr, false);

  if (NS_FAILED(state->PageIn(&page))) {
    return false;
  }

  if ((newVorbisState && ReadHeaders(newVorbisState)) &&
      (mVorbisState->mInfo.rate == newVorbisState->mInfo.rate) &&
      (mVorbisState->mInfo.channels == newVorbisState->mInfo.channels)) {
    mVorbisState->Reset();
    mVorbisState = newVorbisState;
    mVorbisSerial = mVorbisState->mSerial;
    LOG(PR_LOG_DEBUG, ("New vorbis ogg link, serial=%d\n", mVorbisSerial));
    chained = true;
    rate = mVorbisState->mInfo.rate;
    channels = mVorbisState->mInfo.channels;
    tags = mVorbisState->GetTags();
  }

#ifdef MOZ_OPUS
  if ((newOpusState && ReadHeaders(newOpusState)) &&
      (mOpusState->mRate == newOpusState->mRate) &&
      (mOpusState->mChannels == newOpusState->mChannels)) {
    mOpusState->Reset();
    mOpusState = newOpusState;
    mOpusSerial = mOpusState->mSerial;
    chained = true;
    rate = mOpusState->mRate;
    channels = mOpusState->mChannels;
    tags = mOpusState->GetTags();
  }
#endif

  if (chained) {
    SetChained(true);
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->QueueMetadata((mDecodedAudioFrames * USECS_PER_S) / rate,
                               channels,
                               rate,
                               HasAudio(),
                               HasVideo(),
                               tags);
    }
    return true;
  }

  return false;
}

nsresult OggReader::DecodeTheora(ogg_packet* aPacket, int64_t aTimeThreshold)
{
  NS_ASSERTION(aPacket->granulepos >= TheoraVersion(&mTheoraState->mInfo,3,2,1),
    "Packets must have valid granulepos and packetno");

  int ret = th_decode_packetin(mTheoraState->mCtx, aPacket, 0);
  if (ret != 0 && ret != TH_DUPFRAME) {
    return NS_ERROR_FAILURE;
  }
  int64_t time = mTheoraState->StartTime(aPacket->granulepos);

  // Don't use the frame if it's outside the bounds of the presentation
  // start time in the skeleton track. Note we still must submit the frame
  // to the decoder (via th_decode_packetin), as the frames which are
  // presentable may depend on this frame's data.
  if (mSkeletonState && !mSkeletonState->IsPresentable(time)) {
    return NS_OK;
  }

  int64_t endTime = mTheoraState->Time(aPacket->granulepos);
  if (endTime < aTimeThreshold) {
    // The end time of this frame is already before the current playback
    // position. It will never be displayed, don't bother enqueing it.
    return NS_OK;
  }

  if (ret == TH_DUPFRAME) {
    VideoData* v = VideoData::CreateDuplicate(mDecoder->GetResource()->Tell(),
                                              time,
                                              endTime - time,
                                              aPacket->granulepos);
    mVideoQueue.Push(v);
  } else if (ret == 0) {
    th_ycbcr_buffer buffer;
    ret = th_decode_ycbcr_out(mTheoraState->mCtx, buffer);
    NS_ASSERTION(ret == 0, "th_decode_ycbcr_out failed");
    bool isKeyframe = th_packet_iskeyframe(aPacket) == 1;
    VideoData::YCbCrBuffer b;
    for (uint32_t i=0; i < 3; ++i) {
      b.mPlanes[i].mData = buffer[i].data;
      b.mPlanes[i].mHeight = buffer[i].height;
      b.mPlanes[i].mWidth = buffer[i].width;
      b.mPlanes[i].mStride = buffer[i].stride;
      b.mPlanes[i].mOffset = b.mPlanes[i].mSkip = 0;
    }

    VideoData *v = VideoData::Create(mInfo.mVideo,
                                     mDecoder->GetImageContainer(),
                                     mDecoder->GetResource()->Tell(),
                                     time,
                                     endTime - time,
                                     b,
                                     isKeyframe,
                                     aPacket->granulepos,
                                     ToIntRect(mPicture));
    if (!v) {
      // There may be other reasons for this error, but for
      // simplicity just assume the worst case: out of memory.
      NS_WARNING("Failed to allocate memory for video frame");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mVideoQueue.Push(v);
  }
  return NS_OK;
}

bool OggReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                     int64_t aTimeThreshold)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  // Record number of frames decoded and parsed. Automatically update the
  // stats counters using the AutoNotifyDecoded stack-based class.
  uint32_t parsed = 0, decoded = 0;
  AbstractMediaDecoder::AutoNotifyDecoded autoNotify(mDecoder, parsed, decoded);

  // Read the next data packet. Skip any non-data packets we encounter.
  ogg_packet* packet = 0;
  do {
    if (packet) {
      OggCodecState::ReleasePacket(packet);
    }
    packet = NextOggPacket(mTheoraState);
  } while (packet && mTheoraState->IsHeader(packet));
  if (!packet) {
    return false;
  }
  nsAutoRef<ogg_packet> autoRelease(packet);

  parsed++;
  NS_ASSERTION(packet && packet->granulepos != -1,
                "Must know first packet's granulepos");
  bool eos = packet->e_o_s;
  int64_t frameEndTime = mTheoraState->Time(packet->granulepos);
  if (!aKeyframeSkip ||
     (th_packet_iskeyframe(packet) && frameEndTime >= aTimeThreshold))
  {
    aKeyframeSkip = false;
    nsresult res = DecodeTheora(packet, aTimeThreshold);
    decoded++;
    if (NS_FAILED(res)) {
      return false;
    }
  }

  if (eos) {
    // We've encountered an end of bitstream packet. Inform the queue that
    // there will be no more frames.
    return false;
  }

  return true;
}

bool OggReader::ReadOggPage(ogg_page* aPage)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  int ret = 0;
  while((ret = ogg_sync_pageseek(&mOggState, aPage)) <= 0) {
    if (ret < 0) {
      // Lost page sync, have to skip up to next page.
      continue;
    }
    // Returns a buffer that can be written too
    // with the given size. This buffer is stored
    // in the ogg synchronisation structure.
    char* buffer = ogg_sync_buffer(&mOggState, 4096);
    NS_ASSERTION(buffer, "ogg_sync_buffer failed");

    // Read from the resource into the buffer
    uint32_t bytesRead = 0;

    nsresult rv = mDecoder->GetResource()->Read(buffer, 4096, &bytesRead);
    if (NS_FAILED(rv) || (bytesRead == 0 && ret == 0)) {
      // End of file.
      return false;
    }

    // Update the synchronisation layer with the number
    // of bytes written to the buffer
    ret = ogg_sync_wrote(&mOggState, bytesRead);
    NS_ENSURE_TRUE(ret == 0, false);
  }

  return true;
}

ogg_packet* OggReader::NextOggPacket(OggCodecState* aCodecState)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");

  if (!aCodecState || !aCodecState->mActive) {
    return nullptr;
  }

  ogg_packet* packet;
  while ((packet = aCodecState->PacketOut()) == nullptr) {
    // The codec state does not have any buffered pages, so try to read another
    // page from the channel.
    ogg_page page;
    if (!ReadOggPage(&page)) {
      return nullptr;
    }

    uint32_t serial = ogg_page_serialno(&page);
    OggCodecState* codecState = nullptr;
    codecState = mCodecStore.Get(serial);
    if (codecState && NS_FAILED(codecState->PageIn(&page))) {
      return nullptr;
    }
  }

  return packet;
}

// Returns an ogg page's checksum.
static ogg_uint32_t
GetChecksum(ogg_page* page)
{
  if (page == 0 || page->header == 0 || page->header_len < 25) {
    return 0;
  }
  const unsigned char* p = page->header + 22;
  uint32_t c =  p[0] +
               (p[1] << 8) +
               (p[2] << 16) +
               (p[3] << 24);
  return c;
}

int64_t OggReader::RangeStartTime(int64_t aOffset)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource != nullptr, 0);
  nsresult res = resource->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, 0);
  int64_t startTime = 0;
  MediaDecoderReader::FindStartTime(startTime);
  return startTime;
}

struct nsAutoOggSyncState {
  nsAutoOggSyncState() {
    ogg_sync_init(&mState);
  }
  ~nsAutoOggSyncState() {
    ogg_sync_clear(&mState);
  }
  ogg_sync_state mState;
};

int64_t OggReader::RangeEndTime(int64_t aEndOffset)
{
  NS_ASSERTION(mDecoder->OnStateMachineThread() || mDecoder->OnDecodeThread(),
               "Should be on state machine or decode thread.");

  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource != nullptr, -1);
  int64_t position = resource->Tell();
  int64_t endTime = RangeEndTime(0, aEndOffset, false);
  nsresult res = resource->Seek(nsISeekableStream::NS_SEEK_SET, position);
  NS_ENSURE_SUCCESS(res, -1);
  return endTime;
}

int64_t OggReader::RangeEndTime(int64_t aStartOffset,
                                  int64_t aEndOffset,
                                  bool aCachedDataOnly)
{
  MediaResource* resource = mDecoder->GetResource();
  nsAutoOggSyncState sync;

  // We need to find the last page which ends before aEndOffset that
  // has a granulepos that we can convert to a timestamp. We do this by
  // backing off from aEndOffset until we encounter a page on which we can
  // interpret the granulepos. If while backing off we encounter a page which
  // we've previously encountered before, we'll either backoff again if we
  // haven't found an end time yet, or return the last end time found.
  const int step = 5000;
  const int maxOggPageSize = 65306;
  int64_t readStartOffset = aEndOffset;
  int64_t readLimitOffset = aEndOffset;
  int64_t readHead = aEndOffset;
  int64_t endTime = -1;
  uint32_t checksumAfterSeek = 0;
  uint32_t prevChecksumAfterSeek = 0;
  bool mustBackOff = false;
  while (true) {
    ogg_page page;
    int ret = ogg_sync_pageseek(&sync.mState, &page);
    if (ret == 0) {
      // We need more data if we've not encountered a page we've seen before,
      // or we've read to the end of file.
      if (mustBackOff || readHead == aEndOffset || readHead == aStartOffset) {
        if (endTime != -1 || readStartOffset == 0) {
          // We have encountered a page before, or we're at the end of file.
          break;
        }
        mustBackOff = false;
        prevChecksumAfterSeek = checksumAfterSeek;
        checksumAfterSeek = 0;
        ogg_sync_reset(&sync.mState);
        readStartOffset = std::max(static_cast<int64_t>(0), readStartOffset - step);
        // There's no point reading more than the maximum size of
        // an Ogg page into data we've previously scanned. Any data
        // between readLimitOffset and aEndOffset must be garbage
        // and we can ignore it thereafter.
        readLimitOffset = std::min(readLimitOffset,
                                 readStartOffset + maxOggPageSize);
        readHead = std::max(aStartOffset, readStartOffset);
      }

      int64_t limit = std::min(static_cast<int64_t>(UINT32_MAX),
                             aEndOffset - readHead);
      limit = std::max(static_cast<int64_t>(0), limit);
      limit = std::min(limit, static_cast<int64_t>(step));
      uint32_t bytesToRead = static_cast<uint32_t>(limit);
      uint32_t bytesRead = 0;
      char* buffer = ogg_sync_buffer(&sync.mState, bytesToRead);
      NS_ASSERTION(buffer, "Must have buffer");
      nsresult res;
      if (aCachedDataOnly) {
        res = resource->ReadFromCache(buffer, readHead, bytesToRead);
        NS_ENSURE_SUCCESS(res, -1);
        bytesRead = bytesToRead;
      } else {
        NS_ASSERTION(readHead < aEndOffset,
                     "resource pos must be before range end");
        res = resource->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(res, -1);
        res = resource->Read(buffer, bytesToRead, &bytesRead);
        NS_ENSURE_SUCCESS(res, -1);
      }
      readHead += bytesRead;
      if (readHead > readLimitOffset) {
        mustBackOff = true;
      }

      // Update the synchronisation layer with the number
      // of bytes written to the buffer
      ret = ogg_sync_wrote(&sync.mState, bytesRead);
      if (ret != 0) {
        endTime = -1;
        break;
      }

      continue;
    }

    if (ret < 0 || ogg_page_granulepos(&page) < 0) {
      continue;
    }

    uint32_t checksum = GetChecksum(&page);
    if (checksumAfterSeek == 0) {
      // This is the first page we've decoded after a backoff/seek. Remember
      // the page checksum. If we backoff further and encounter this page
      // again, we'll know that we won't find a page with an end time after
      // this one, so we'll know to back off again.
      checksumAfterSeek = checksum;
    }
    if (checksum == prevChecksumAfterSeek) {
      // This page has the same checksum as the first page we encountered
      // after the last backoff/seek. Since we've already scanned after this
      // page and failed to find an end time, we may as well backoff again and
      // try to find an end time from an earlier page.
      mustBackOff = true;
      continue;
    }

    int64_t granulepos = ogg_page_granulepos(&page);
    int serial = ogg_page_serialno(&page);

    OggCodecState* codecState = nullptr;
    codecState = mCodecStore.Get(serial);

    if (!codecState) {
      // This page is from a bitstream which we haven't encountered yet.
      // It's probably from a new "link" in a "chained" ogg. Don't
      // bother even trying to find a duration...
      SetChained(true);
      endTime = -1;
      break;
    }

    int64_t t = codecState->Time(granulepos);
    if (t != -1) {
      endTime = t;
    }
  }

  return endTime;
}

nsresult OggReader::GetSeekRanges(nsTArray<SeekRange>& aRanges)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsTArray<MediaByteRange> cached;
  nsresult res = mDecoder->GetResource()->GetCachedRanges(cached);
  NS_ENSURE_SUCCESS(res, res);

  for (uint32_t index = 0; index < cached.Length(); index++) {
    MediaByteRange& range = cached[index];
    int64_t startTime = -1;
    int64_t endTime = -1;
    if (NS_FAILED(ResetDecode())) {
      return NS_ERROR_FAILURE;
    }
    int64_t startOffset = range.mStart;
    int64_t endOffset = range.mEnd;
    startTime = RangeStartTime(startOffset);
    if (startTime != -1 &&
        ((endTime = RangeEndTime(endOffset)) != -1))
    {
      NS_WARN_IF_FALSE(startTime < endTime,
                       "Start time must be before end time");
      aRanges.AppendElement(SeekRange(startOffset,
                                      endOffset,
                                      startTime,
                                      endTime));
     }
  }
  if (NS_FAILED(ResetDecode())) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

OggReader::SeekRange
OggReader::SelectSeekRange(const nsTArray<SeekRange>& ranges,
                             int64_t aTarget,
                             int64_t aStartTime,
                             int64_t aEndTime,
                             bool aExact)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  int64_t so = 0;
  int64_t eo = mDecoder->GetResource()->GetLength();
  int64_t st = aStartTime;
  int64_t et = aEndTime;
  for (uint32_t i = 0; i < ranges.Length(); i++) {
    const SeekRange &r = ranges[i];
    if (r.mTimeStart < aTarget) {
      so = r.mOffsetStart;
      st = r.mTimeStart;
    }
    if (r.mTimeEnd >= aTarget && r.mTimeEnd < et) {
      eo = r.mOffsetEnd;
      et = r.mTimeEnd;
    }

    if (r.mTimeStart < aTarget && aTarget <= r.mTimeEnd) {
      // Target lies exactly in this range.
      return ranges[i];
    }
  }
  if (aExact || eo == -1) {
    return SeekRange();
  }
  return SeekRange(so, eo, st, et);
}

OggReader::IndexedSeekResult OggReader::RollbackIndexedSeek(int64_t aOffset)
{
  mSkeletonState->Deactivate();
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource != nullptr, SEEK_FATAL_ERROR);
  nsresult res = resource->Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);
  return SEEK_INDEX_FAIL;
}

OggReader::IndexedSeekResult OggReader::SeekToKeyframeUsingIndex(int64_t aTarget)
{
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource != nullptr, SEEK_FATAL_ERROR);
  if (!HasSkeleton() || !mSkeletonState->HasIndex()) {
    return SEEK_INDEX_FAIL;
  }
  // We have an index from the Skeleton track, try to use it to seek.
  nsAutoTArray<uint32_t, 2> tracks;
  BuildSerialList(tracks);
  SkeletonState::nsSeekTarget keyframe;
  if (NS_FAILED(mSkeletonState->IndexedSeekTarget(aTarget,
                                                  tracks,
                                                  keyframe)))
  {
    // Could not locate a keypoint for the target in the index.
    return SEEK_INDEX_FAIL;
  }

  // Remember original resource read cursor position so we can rollback on failure.
  int64_t tell = resource->Tell();

  // Seek to the keypoint returned by the index.
  if (keyframe.mKeyPoint.mOffset > resource->GetLength() ||
      keyframe.mKeyPoint.mOffset < 0)
  {
    // Index must be invalid.
    return RollbackIndexedSeek(tell);
  }
  LOG(PR_LOG_DEBUG, ("Seeking using index to keyframe at offset %lld\n",
                     keyframe.mKeyPoint.mOffset));
  nsresult res = resource->Seek(nsISeekableStream::NS_SEEK_SET,
                              keyframe.mKeyPoint.mOffset);
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // We've moved the read set, so reset decode.
  res = ResetDecode();
  NS_ENSURE_SUCCESS(res, SEEK_FATAL_ERROR);

  // Check that the page the index thinks is exactly here is actually exactly
  // here. If not, the index is invalid.
  ogg_page page;
  int skippedBytes = 0;
  PageSyncResult syncres = PageSync(resource,
                                    &mOggState,
                                    false,
                                    keyframe.mKeyPoint.mOffset,
                                    resource->GetLength(),
                                    &page,
                                    skippedBytes);
  NS_ENSURE_TRUE(syncres != PAGE_SYNC_ERROR, SEEK_FATAL_ERROR);
  if (syncres != PAGE_SYNC_OK || skippedBytes != 0) {
    LOG(PR_LOG_DEBUG, ("Indexed-seek failure: Ogg Skeleton Index is invalid "
                       "or sync error after seek"));
    return RollbackIndexedSeek(tell);
  }
  uint32_t serial = ogg_page_serialno(&page);
  if (serial != keyframe.mSerial) {
    // Serialno of page at offset isn't what the index told us to expect.
    // Assume the index is invalid.
    return RollbackIndexedSeek(tell);
  }
  OggCodecState* codecState = mCodecStore.Get(serial);
  if (codecState &&
      codecState->mActive &&
      ogg_stream_pagein(&codecState->mState, &page) != 0)
  {
    // Couldn't insert page into the ogg resource, or somehow the resource
    // is no longer active.
    return RollbackIndexedSeek(tell);
  }
  return SEEK_OK;
}

nsresult OggReader::SeekInBufferedRange(int64_t aTarget,
                                          int64_t aAdjustedTarget,
                                          int64_t aStartTime,
                                          int64_t aEndTime,
                                          const nsTArray<SeekRange>& aRanges,
                                          const SeekRange& aRange)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in buffered data to %lld using bisection search", mDecoder, aTarget));
  nsresult res = NS_OK;
  if (HasVideo() || aAdjustedTarget >= aTarget) {
    // We know the exact byte range in which the target must lie. It must
    // be buffered in the media cache. Seek there.
    nsresult res = SeekBisection(aTarget, aRange, 0);
    if (NS_FAILED(res) || !HasVideo()) {
      return res;
    }

    // We have an active Theora bitstream. Decode the next Theora frame, and
    // extract its keyframe's time.
    bool eof;
    do {
      bool skip = false;
      eof = !DecodeVideoFrame(skip, 0);
      {
        ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
        if (mDecoder->IsShutdown()) {
          return NS_ERROR_FAILURE;
        }
      }
    } while (!eof &&
             mVideoQueue.GetSize() == 0);

    VideoData* video = mVideoQueue.PeekFront();
    if (video && !video->mKeyframe) {
      // First decoded frame isn't a keyframe, seek back to previous keyframe,
      // otherwise we'll get visual artifacts.
      NS_ASSERTION(video->mTimecode != -1, "Must have a granulepos");
      int shift = mTheoraState->mInfo.keyframe_granule_shift;
      int64_t keyframeGranulepos = (video->mTimecode >> shift) << shift;
      int64_t keyframeTime = mTheoraState->StartTime(keyframeGranulepos);
      SEEK_LOG(PR_LOG_DEBUG, ("Keyframe for %lld is at %lld, seeking back to it",
                              video->mTime, keyframeTime));
      aAdjustedTarget = std::min(aAdjustedTarget, keyframeTime);
    }
  }
  if (aAdjustedTarget < aTarget) {
    SeekRange k = SelectSeekRange(aRanges,
                                  aAdjustedTarget,
                                  aStartTime,
                                  aEndTime,
                                  false);
    res = SeekBisection(aAdjustedTarget, k, SEEK_FUZZ_USECS);
  }
  return res;
}

nsresult OggReader::SeekInUnbuffered(int64_t aTarget,
                                       int64_t aStartTime,
                                       int64_t aEndTime,
                                       const nsTArray<SeekRange>& aRanges)
{
  LOG(PR_LOG_DEBUG, ("%p Seeking in unbuffered data to %lld using bisection search", mDecoder, aTarget));

  // If we've got an active Theora bitstream, determine the maximum possible
  // time in usecs which a keyframe could be before a given interframe. We
  // subtract this from our seek target, seek to the new target, and then
  // will decode forward to the original seek target. We should encounter a
  // keyframe in that interval. This prevents us from needing to run two
  // bisections; one for the seek target frame, and another to find its
  // keyframe. It's usually faster to just download this extra data, rather
  // tham perform two bisections to find the seek target's keyframe. We
  // don't do this offsetting when seeking in a buffered range,
  // as the extra decoding causes a noticeable speed hit when all the data
  // is buffered (compared to just doing a bisection to exactly find the
  // keyframe).
  int64_t keyframeOffsetMs = 0;
  if (HasVideo() && mTheoraState) {
    keyframeOffsetMs = mTheoraState->MaxKeyframeOffset();
  }
#ifdef MOZ_OPUS
  // Add in the Opus pre-roll if necessary, as well.
  if (HasAudio() && mOpusState) {
    keyframeOffsetMs = std::max(keyframeOffsetMs, SEEK_OPUS_PREROLL);
  }
#endif /* MOZ_OPUS */
  int64_t seekTarget = std::max(aStartTime, aTarget - keyframeOffsetMs);
  // Minimize the bisection search space using the known timestamps from the
  // buffered ranges.
  SeekRange k = SelectSeekRange(aRanges, seekTarget, aStartTime, aEndTime, false);
  return SeekBisection(seekTarget, k, SEEK_FUZZ_USECS);
}

nsresult OggReader::Seek(int64_t aTarget,
                         int64_t aStartTime,
                         int64_t aEndTime,
                         int64_t aCurrentTime)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  if (mIsChained)
    return NS_ERROR_FAILURE;
  LOG(PR_LOG_DEBUG, ("%p About to seek to %lld", mDecoder, aTarget));
  nsresult res;
  MediaResource* resource = mDecoder->GetResource();
  NS_ENSURE_TRUE(resource != nullptr, NS_ERROR_FAILURE);
  int64_t adjustedTarget = aTarget;
#ifdef MOZ_OPUS
  if (HasAudio() && mOpusState){
    adjustedTarget = std::max(aStartTime, aTarget - SEEK_OPUS_PREROLL);
  }
#endif /* MOZ_OPUS */

  if (adjustedTarget == aStartTime) {
    // We've seeked to the media start. Just seek to the offset of the first
    // content page.
    res = resource->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);

    res = ResetDecode(true);
    NS_ENSURE_SUCCESS(res,res);

    NS_ASSERTION(aStartTime != -1, "mStartTime should be known");
    {
      ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
      mDecoder->UpdatePlaybackPosition(aStartTime);
    }
  } else {
    // TODO: This may seek back unnecessarily far in the video, but we don't
    // have a way of asking Skeleton to seek to a different target for each
    // stream yet. Using adjustedTarget here is at least correct, if slow.
    IndexedSeekResult sres = SeekToKeyframeUsingIndex(adjustedTarget);
    NS_ENSURE_TRUE(sres != SEEK_FATAL_ERROR, NS_ERROR_FAILURE);
    if (sres == SEEK_INDEX_FAIL) {
      // No index or other non-fatal index-related failure. Try to seek
      // using a bisection search. Determine the already downloaded data
      // in the media cache, so we can try to seek in the cached data first.
      nsAutoTArray<SeekRange, 16> ranges;
      res = GetSeekRanges(ranges);
      NS_ENSURE_SUCCESS(res,res);

      // Figure out if the seek target lies in a buffered range.
      SeekRange r = SelectSeekRange(ranges, aTarget, aStartTime, aEndTime, true);

      if (!r.IsNull()) {
        // We know the buffered range in which the seek target lies, do a
        // bisection search in that buffered range.
        res = SeekInBufferedRange(aTarget, adjustedTarget, aStartTime, aEndTime, ranges, r);
        NS_ENSURE_SUCCESS(res,res);
      } else {
        // The target doesn't lie in a buffered range. Perform a bisection
        // search over the whole media, using the known buffered ranges to
        // reduce the search space.
        res = SeekInUnbuffered(aTarget, aStartTime, aEndTime, ranges);
        NS_ENSURE_SUCCESS(res,res);
      }
    }
  }

  if (HasVideo()) {
    // Decode forwards until we find the next keyframe. This is required,
    // as although the seek should finish on a page containing a keyframe,
    // there may be non-keyframes in the page before the keyframe.
    // When doing fastSeek we display the first frame after the seek, so
    // we need to advance the decode to the keyframe otherwise we'll get
    // visual artifacts in the first frame output after the seek.
    // First, we must check to see if there's already a keyframe in the frames
    // that we may have already decoded, and discard frames up to the
    // keyframe.
    VideoData* v;
    while ((v = mVideoQueue.PeekFront()) && !v->mKeyframe) {
      delete mVideoQueue.PopFront();
    }
    if (mVideoQueue.GetSize() == 0) {
      // We didn't find a keyframe in the frames already here, so decode
      // forwards until we find a keyframe.
      bool skip = true;
      while (DecodeVideoFrame(skip, 0) && skip) {
        ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
        if (mDecoder->IsShutdown()) {
          return NS_ERROR_FAILURE;
        }
      }
    }
#ifdef DEBUG
    v = mVideoQueue.PeekFront();
    if (!v || !v->mKeyframe) {
      NS_WARNING("Ogg seek didn't end up before a key frame!");
    }
#endif
  }
  return NS_OK;
}

// Reads a page from the media resource.
static PageSyncResult
PageSync(MediaResource* aResource,
         ogg_sync_state* aState,
         bool aCachedDataOnly,
         int64_t aOffset,
         int64_t aEndOffset,
         ogg_page* aPage,
         int& aSkippedBytes)
{
  aSkippedBytes = 0;
  // Sync to the next page.
  int ret = 0;
  uint32_t bytesRead = 0;
  int64_t readHead = aOffset;
  while (ret <= 0) {
    ret = ogg_sync_pageseek(aState, aPage);
    if (ret == 0) {
      char* buffer = ogg_sync_buffer(aState, PAGE_STEP);
      NS_ASSERTION(buffer, "Must have a buffer");

      // Read from the file into the buffer
      int64_t bytesToRead = std::min(static_cast<int64_t>(PAGE_STEP),
                                   aEndOffset - readHead);
      NS_ASSERTION(bytesToRead <= UINT32_MAX, "bytesToRead range check");
      if (bytesToRead <= 0) {
        return PAGE_SYNC_END_OF_RANGE;
      }
      nsresult rv = NS_OK;
      if (aCachedDataOnly) {
        rv = aResource->ReadFromCache(buffer, readHead,
                                      static_cast<uint32_t>(bytesToRead));
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        bytesRead = static_cast<uint32_t>(bytesToRead);
      } else {
        rv = aResource->Seek(nsISeekableStream::NS_SEEK_SET, readHead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
        rv = aResource->Read(buffer,
                             static_cast<uint32_t>(bytesToRead),
                             &bytesRead);
        NS_ENSURE_SUCCESS(rv,PAGE_SYNC_ERROR);
      }
      if (bytesRead == 0 && NS_SUCCEEDED(rv)) {
        // End of file.
        return PAGE_SYNC_END_OF_RANGE;
      }
      readHead += bytesRead;

      // Update the synchronisation layer with the number
      // of bytes written to the buffer
      ret = ogg_sync_wrote(aState, bytesRead);
      NS_ENSURE_TRUE(ret == 0, PAGE_SYNC_ERROR);
      continue;
    }

    if (ret < 0) {
      NS_ASSERTION(aSkippedBytes >= 0, "Offset >= 0");
      aSkippedBytes += -ret;
      NS_ASSERTION(aSkippedBytes >= 0, "Offset >= 0");
      continue;
    }
  }

  return PAGE_SYNC_OK;
}

nsresult OggReader::SeekBisection(int64_t aTarget,
                                    const SeekRange& aRange,
                                    uint32_t aFuzz)
{
  NS_ASSERTION(mDecoder->OnDecodeThread(), "Should be on decode thread.");
  nsresult res;
  MediaResource* resource = mDecoder->GetResource();

  if (aTarget == aRange.mTimeStart) {
    if (NS_FAILED(ResetDecode())) {
      return NS_ERROR_FAILURE;
    }
    res = resource->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(res,res);
    return NS_OK;
  }

  // Bisection search, find start offset of last page with end time less than
  // the seek target.
  ogg_int64_t startOffset = aRange.mOffsetStart;
  ogg_int64_t startTime = aRange.mTimeStart;
  ogg_int64_t startLength = 0; // Length of the page at startOffset.
  ogg_int64_t endOffset = aRange.mOffsetEnd;
  ogg_int64_t endTime = aRange.mTimeEnd;

  ogg_int64_t seekTarget = aTarget;
  int64_t seekLowerBound = std::max(static_cast<int64_t>(0), aTarget - aFuzz);
  int hops = 0;
  DebugOnly<ogg_int64_t> previousGuess = -1;
  int backsteps = 0;
  const int maxBackStep = 10;
  NS_ASSERTION(static_cast<uint64_t>(PAGE_STEP) * pow(2.0, maxBackStep) < INT32_MAX,
               "Backstep calculation must not overflow");

  // Seek via bisection search. Loop until we find the offset where the page
  // before the offset is before the seek target, and the page after the offset
  // is after the seek target.
  while (true) {
    ogg_int64_t duration = 0;
    double target = 0;
    ogg_int64_t interval = 0;
    ogg_int64_t guess = 0;
    ogg_page page;
    int skippedBytes = 0;
    ogg_int64_t pageOffset = 0;
    ogg_int64_t pageLength = 0;
    ogg_int64_t granuleTime = -1;
    bool mustBackoff = false;

    // Guess where we should bisect to, based on the bit rate and the time
    // remaining in the interval. Loop until we can determine the time at
    // the guess offset.
    while (true) {

      // Discard any previously buffered packets/pages.
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }

      interval = endOffset - startOffset - startLength;
      if (interval == 0) {
        // Our interval is empty, we've found the optimal seek point, as the
        // page at the start offset is before the seek target, and the page
        // at the end offset is after the seek target.
        SEEK_LOG(PR_LOG_DEBUG, ("Interval narrowed, terminating bisection."));
        break;
      }

      // Guess bisection point.
      duration = endTime - startTime;
      target = (double)(seekTarget - startTime) / (double)duration;
      guess = startOffset + startLength +
              static_cast<ogg_int64_t>((double)interval * target);
      guess = std::min(guess, endOffset - PAGE_STEP);
      if (mustBackoff) {
        // We previously failed to determine the time at the guess offset,
        // probably because we ran out of data to decode. This usually happens
        // when we guess very close to the end offset. So reduce the guess
        // offset using an exponential backoff until we determine the time.
        SEEK_LOG(PR_LOG_DEBUG, ("Backing off %d bytes, backsteps=%d",
          static_cast<int32_t>(PAGE_STEP * pow(2.0, backsteps)), backsteps));
        guess -= PAGE_STEP * static_cast<ogg_int64_t>(pow(2.0, backsteps));

        if (guess <= startOffset) {
          // We've tried to backoff to before the start offset of our seek
          // range. This means we couldn't find a seek termination position
          // near the end of the seek range, so just set the seek termination
          // condition, and break out of the bisection loop. We'll begin
          // decoding from the start of the seek range.
          interval = 0;
          break;
        }

        backsteps = std::min(backsteps + 1, maxBackStep);
        // We reset mustBackoff. If we still need to backoff further, it will
        // be set to true again.
        mustBackoff = false;
      } else {
        backsteps = 0;
      }
      guess = std::max(guess, startOffset + startLength);

      SEEK_LOG(PR_LOG_DEBUG, ("Seek loop start[o=%lld..%lld t=%lld] "
                              "end[o=%lld t=%lld] "
                              "interval=%lld target=%lf guess=%lld",
                              startOffset, (startOffset+startLength), startTime,
                              endOffset, endTime, interval, target, guess));

      NS_ASSERTION(guess >= startOffset + startLength, "Guess must be after range start");
      NS_ASSERTION(guess < endOffset, "Guess must be before range end");
      NS_ASSERTION(guess != previousGuess, "Guess should be different to previous");
      previousGuess = guess;

      hops++;

      // Locate the next page after our seek guess, and then figure out the
      // granule time of the audio and video bitstreams there. We can then
      // make a bisection decision based on our location in the media.
      PageSyncResult res = PageSync(resource,
                                    &mOggState,
                                    false,
                                    guess,
                                    endOffset,
                                    &page,
                                    skippedBytes);
      NS_ENSURE_TRUE(res != PAGE_SYNC_ERROR, NS_ERROR_FAILURE);

      if (res == PAGE_SYNC_END_OF_RANGE) {
        // Our guess was too close to the end, we've ended up reading the end
        // page. Backoff exponentially from the end point, in case the last
        // page/frame/sample is huge.
        mustBackoff = true;
        SEEK_LOG(PR_LOG_DEBUG, ("Hit the end of range, backing off"));
        continue;
      }

      // We've located a page of length |ret| at |guess + skippedBytes|.
      // Remember where the page is located.
      pageOffset = guess + skippedBytes;
      pageLength = page.header_len + page.body_len;

      // Read pages until we can determine the granule time of the audio and
      // video bitstream.
      ogg_int64_t audioTime = -1;
      ogg_int64_t videoTime = -1;
      do {
        // Add the page to its codec state, determine its granule time.
        uint32_t serial = ogg_page_serialno(&page);
        OggCodecState* codecState = mCodecStore.Get(serial);
        if (codecState && codecState->mActive) {
          int ret = ogg_stream_pagein(&codecState->mState, &page);
          NS_ENSURE_TRUE(ret == 0, NS_ERROR_FAILURE);
        }

        ogg_int64_t granulepos = ogg_page_granulepos(&page);

        if (HasAudio() && granulepos > 0 && audioTime == -1) {
          if (mVorbisState && serial == mVorbisState->mSerial) {
            audioTime = mVorbisState->Time(granulepos);
#ifdef MOZ_OPUS
          } else if (mOpusState && serial == mOpusState->mSerial) {
            audioTime = mOpusState->Time(granulepos);
#endif
          }
        }

        if (HasVideo() &&
            granulepos > 0 &&
            serial == mTheoraState->mSerial &&
            videoTime == -1) {
          videoTime = mTheoraState->Time(granulepos);
        }

        if (pageOffset + pageLength >= endOffset) {
          // Hit end of readable data.
          break;
        }

        if (!ReadOggPage(&page)) {
          break;
        }

      } while ((HasAudio() && audioTime == -1) ||
               (HasVideo() && videoTime == -1));


      if ((HasAudio() && audioTime == -1) ||
          (HasVideo() && videoTime == -1))
      {
        // We don't have timestamps for all active tracks...
        if (pageOffset == startOffset + startLength &&
            pageOffset + pageLength >= endOffset) {
          // We read the entire interval without finding timestamps for all
          // active tracks. We know the interval start offset is before the seek
          // target, and the interval end is after the seek target, and we can't
          // terminate inside the interval, so we terminate the seek at the
          // start of the interval.
          interval = 0;
          break;
        }

        // We should backoff; cause the guess to back off from the end, so
        // that we've got more room to capture.
        mustBackoff = true;
        continue;
      }

      // We've found appropriate time stamps here. Proceed to bisect
      // the search space.
      granuleTime = std::max(audioTime, videoTime);
      NS_ASSERTION(granuleTime > 0, "Must get a granuletime");
      break;
    } // End of "until we determine time at guess offset" loop.

    if (interval == 0) {
      // Seek termination condition; we've found the page boundary of the
      // last page before the target, and the first page after the target.
      SEEK_LOG(PR_LOG_DEBUG, ("Terminating seek at offset=%lld", startOffset));
      NS_ASSERTION(startTime < aTarget, "Start time must always be less than target");
      res = resource->Seek(nsISeekableStream::NS_SEEK_SET, startOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      break;
    }

    SEEK_LOG(PR_LOG_DEBUG, ("Time at offset %lld is %lld", guess, granuleTime));
    if (granuleTime < seekTarget && granuleTime > seekLowerBound) {
      // We're within the fuzzy region in which we want to terminate the search.
      res = resource->Seek(nsISeekableStream::NS_SEEK_SET, pageOffset);
      NS_ENSURE_SUCCESS(res,res);
      if (NS_FAILED(ResetDecode())) {
        return NS_ERROR_FAILURE;
      }
      SEEK_LOG(PR_LOG_DEBUG, ("Terminating seek at offset=%lld", pageOffset));
      break;
    }

    if (granuleTime >= seekTarget) {
      // We've landed after the seek target.
      NS_ASSERTION(pageOffset < endOffset, "offset_end must decrease");
      endOffset = pageOffset;
      endTime = granuleTime;
    } else if (granuleTime < seekTarget) {
      // Landed before seek target.
      NS_ASSERTION(pageOffset >= startOffset + startLength,
        "Bisection point should be at or after end of first page in interval");
      startOffset = pageOffset;
      startLength = pageLength;
      startTime = granuleTime;
    }
    NS_ASSERTION(startTime < seekTarget, "Must be before seek target");
    NS_ASSERTION(endTime >= seekTarget, "End must be after seek target");
  }

  SEEK_LOG(PR_LOG_DEBUG, ("Seek complete in %d bisections.", hops));

  return NS_OK;
}

nsresult OggReader::GetBuffered(dom::TimeRanges* aBuffered, int64_t aStartTime)
{
  {
    mozilla::ReentrantMonitorAutoEnter mon(mMonitor);
    if (mIsChained)
      return NS_ERROR_FAILURE;
  }
#ifdef OGG_ESTIMATE_BUFFERED
  return MediaDecoderReader::GetBuffered(aBuffered, aStartTime);
#else
  // HasAudio and HasVideo are not used here as they take a lock and cause
  // a deadlock. Accessing mInfo doesn't require a lock - it doesn't change
  // after metadata is read.
  if (!mInfo.HasValidMedia()) {
    // No need to search through the file if there are no audio or video tracks
    return NS_OK;
  }

  MediaResource* resource = mDecoder->GetResource();
  nsTArray<MediaByteRange> ranges;
  nsresult res = resource->GetCachedRanges(ranges);
  NS_ENSURE_SUCCESS(res, res);

  // Traverse across the buffered byte ranges, determining the time ranges
  // they contain. MediaResource::GetNextCachedData(offset) returns -1 when
  // offset is after the end of the media resource, or there's no more cached
  // data after the offset. This loop will run until we've checked every
  // buffered range in the media, in increasing order of offset.
  nsAutoOggSyncState sync;
  for (uint32_t index = 0; index < ranges.Length(); index++) {
    // Ensure the offsets are after the header pages.
    int64_t startOffset = ranges[index].mStart;
    int64_t endOffset = ranges[index].mEnd;

    // Because the granulepos time is actually the end time of the page,
    // we special-case (startOffset == 0) so that the first
    // buffered range always appears to be buffered from the media start
    // time, rather than from the end-time of the first page.
    int64_t startTime = (startOffset == 0) ? aStartTime : -1;

    // Find the start time of the range. Read pages until we find one with a
    // granulepos which we can convert into a timestamp to use as the time of
    // the start of the buffered range.
    ogg_sync_reset(&sync.mState);
    while (startTime == -1) {
      ogg_page page;
      int32_t discard;
      PageSyncResult res = PageSync(resource,
                                    &sync.mState,
                                    true,
                                    startOffset,
                                    endOffset,
                                    &page,
                                    discard);
      if (res == PAGE_SYNC_ERROR) {
        return NS_ERROR_FAILURE;
      } else if (res == PAGE_SYNC_END_OF_RANGE) {
        // Hit the end of range without reading a page, give up trying to
        // find a start time for this buffered range, skip onto the next one.
        break;
      }

      int64_t granulepos = ogg_page_granulepos(&page);
      if (granulepos == -1) {
        // Page doesn't have an end time, advance to the next page
        // until we find one.
        startOffset += page.header_len + page.body_len;
        continue;
      }

      uint32_t serial = ogg_page_serialno(&page);
      if (mVorbisState && serial == mVorbisSerial) {
        startTime = VorbisState::Time(&mVorbisInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
#ifdef MOZ_OPUS
      else if (mOpusState && serial == mOpusSerial) {
        startTime = OpusState::Time(mOpusPreSkip, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
#endif /* MOZ_OPUS */
      else if (mTheoraState && serial == mTheoraSerial) {
        startTime = TheoraState::Time(&mTheoraInfo, granulepos);
        NS_ASSERTION(startTime > 0, "Must have positive start time");
      }
      else if (mCodecStore.Contains(serial)) {
        // Stream is not the theora or vorbis stream we're playing,
        // but is one that we have header data for.
        startOffset += page.header_len + page.body_len;
        continue;
      }
      else {
        // Page is for a stream we don't know about (possibly a chained
        // ogg), return OK to abort the finding any further ranges. This
        // prevents us searching through the rest of the media when we
        // may not be able to extract timestamps from it.
        SetChained(true);
        return NS_OK;
      }
    }

    if (startTime != -1) {
      // We were able to find a start time for that range, see if we can
      // find an end time.
      int64_t endTime = RangeEndTime(startOffset, endOffset, true);
      if (endTime != -1) {
        aBuffered->Add((startTime - aStartTime) / static_cast<double>(USECS_PER_S),
                       (endTime - aStartTime) / static_cast<double>(USECS_PER_S));
      }
    }
  }

  return NS_OK;
#endif
}

OggCodecStore::OggCodecStore()
: mMonitor("CodecStore")
{
}

void OggCodecStore::Add(uint32_t serial, OggCodecState* codecState)
{
  MonitorAutoLock mon(mMonitor);
  mCodecStates.Put(serial, codecState);
}

bool OggCodecStore::Contains(uint32_t serial)
{
  MonitorAutoLock mon(mMonitor);
  return mCodecStates.Get(serial, nullptr);
}

OggCodecState* OggCodecStore::Get(uint32_t serial)
{
  MonitorAutoLock mon(mMonitor);
  return mCodecStates.Get(serial);
}

} // namespace mozilla

