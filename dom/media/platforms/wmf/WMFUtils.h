/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMFUtils_h
#define WMFUtils_h

#include <hstring.h>
#include <winstring.h>

#include "ImageTypes.h"
#include "PlatformDecoderModule.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include "WMF.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/gfx/Rect.h"
#include "nsString.h"

// Various utilities shared by WMF backend files.

namespace mozilla {

namespace gfx {
enum class ColorDepth : uint8_t;
}
extern LazyLogModule sPDMLog;

#define LOG_AND_WARNING_PDM(msg, ...)                            \
  do {                                                           \
    NS_WARNING(nsPrintfCString(msg, rv).get());                  \
    MOZ_LOG(sPDMLog, LogLevel::Debug,                            \
            ("%s:%d, " msg, __FILE__, __LINE__, ##__VA_ARGS__)); \
  } while (false)

#ifndef RETURN_IF_FAILED
#  define RETURN_IF_FAILED(x)                               \
    do {                                                    \
      HRESULT rv = x;                                       \
      if (MOZ_UNLIKELY(FAILED(rv))) {                       \
        LOG_AND_WARNING_PDM("(" #x ") failed, rv=%lx", rv); \
        return rv;                                          \
      }                                                     \
    } while (false)
#endif

#ifndef RETURN_PARAM_IF_FAILED
#  define RETURN_PARAM_IF_FAILED(x, defaultOut)             \
    do {                                                    \
      HRESULT rv = x;                                       \
      if (MOZ_UNLIKELY(FAILED(rv))) {                       \
        LOG_AND_WARNING_PDM("(" #x ") failed, rv=%lx", rv); \
        return defaultOut;                                  \
      }                                                     \
    } while (false)
#endif

static const GUID CLSID_MSOpusDecoder = {
    0x63e17c10,
    0x2d43,
    0x4c42,
    {0x8f, 0xe3, 0x8d, 0x8b, 0x63, 0xe4, 0x6a, 0x6a}};

// Media types supported by Media Foundation.
MOZ_DEFINE_ENUM_CLASS_WITH_TOSTRING(WMFStreamType,
                                    (Unknown, H264, VP8, VP9, AV1, HEVC, MP3,
                                     AAC, OPUS, VORBIS, SENTINEL));

bool StreamTypeIsVideo(const WMFStreamType& aType);

bool StreamTypeIsAudio(const WMFStreamType& aType);

WMFStreamType GetStreamTypeFromMimeType(const nsCString& aMimeType);

GUID GetOutputSubType(const gfx::ColorDepth& aColorDepth,
                      bool aIsHardwareDecoding);

nsCString GetSubTypeStr(const GUID& aSubtype);

// Converts from microseconds to hundreds of nanoseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t UsecsToHNs(int64_t aUsecs) { return aUsecs * 10; }

// Converts from hundreds of nanoseconds to microseconds.
// We use microseconds for our timestamps, whereas WMF uses
// hundreds of nanoseconds.
inline int64_t HNsToUsecs(int64_t hNanoSecs) { return hNanoSecs / 10; }

HRESULT HNsToFrames(int64_t aHNs, uint32_t aRate, int64_t* aOutFrames);

HRESULT
GetDefaultStride(IMFMediaType* aType, uint32_t aWidth, uint32_t* aOutStride);

Maybe<gfx::YUVColorSpace> GetYUVColorSpace(IMFMediaType* aType);

int32_t MFOffsetToInt32(const MFOffset& aOffset);

// Gets the sub-region of the video frame that should be displayed.
// See:
// http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx
HRESULT
GetPictureRegion(IMFMediaType* aMediaType, gfx::IntRect& aOutPictureRegion);

// Returns the duration of a IMFSample in TimeUnit.
// Returns media::TimeUnit::Invalid() on failure.
media::TimeUnit GetSampleDuration(IMFSample* aSample);

// Returns the presentation time of a IMFSample in TimeUnit.
// Returns media::TimeUnit::Invalid() on failure.
media::TimeUnit GetSampleTime(IMFSample* aSample);

inline bool IsFlagSet(DWORD flags, DWORD pattern) {
  return (flags & pattern) == pattern;
}

// Will return %ProgramW6432% value as per:
// https://msdn.microsoft.com/library/windows/desktop/aa384274.aspx
nsString GetProgramW6432Path();

const char* MFTMessageTypeToStr(MFT_MESSAGE_TYPE aMsg);

GUID AudioMimeTypeToMediaFoundationSubtype(const nsACString& aMimeType);

GUID VideoMimeTypeToMediaFoundationSubtype(const nsACString& aMimeType);

void AACAudioSpecificConfigToUserData(uint8_t aAACProfileLevelIndication,
                                      const uint8_t* aAudioSpecConfig,
                                      uint32_t aConfigLength,
                                      nsTArray<BYTE>& aOutUserData);

class ScopedHString final {
 public:
  explicit ScopedHString(const nsAString& aStr) {
    WindowsCreateString(PromiseFlatString(aStr).get(), aStr.Length(), &mString);
  }
  explicit ScopedHString(const WCHAR aCharArray[]) {
    WindowsCreateString(aCharArray, wcslen(aCharArray), &mString);
  }
  ~ScopedHString() { WindowsDeleteString(mString); }
  const HSTRING& Get() { return mString; }

 private:
  HSTRING mString;
};

}  // namespace mozilla

#endif
