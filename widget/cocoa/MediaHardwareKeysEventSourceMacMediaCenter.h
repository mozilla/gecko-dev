/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_
#define WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_

#include "mozilla/dom/FetchImageHelper.h"
#include "mozilla/dom/MediaControlKeySource.h"

#ifdef __OBJC__
@class MPRemoteCommandEvent;
#else
typedef struct objc_object MPRemoteCommandEvent;
#endif
enum MPRemoteCommandHandlerStatus : long;

namespace mozilla {
namespace widget {

typedef MPRemoteCommandHandlerStatus (^MediaCenterEventHandler)(
    MPRemoteCommandEvent* event);

class MediaHardwareKeysEventSourceMacMediaCenter final
    : public mozilla::dom::MediaControlKeySource {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaHardwareKeysEventSourceMacMediaCenter,
                             override)
  MediaHardwareKeysEventSourceMacMediaCenter();

  MediaCenterEventHandler CreatePlayPauseHandler();
  MediaCenterEventHandler CreateNextTrackHandler();
  MediaCenterEventHandler CreatePreviousTrackHandler();
  MediaCenterEventHandler CreatePlayHandler();
  MediaCenterEventHandler CreatePauseHandler();
  MediaCenterEventHandler CreateChangePlaybackPositionHandler();

  bool Open() override;
  void Close() override;
  bool IsOpened() const override;
  void SetPlaybackState(dom::MediaSessionPlaybackState aState) override;
  void SetMediaMetadata(const dom::MediaMetadataBase& aMetadata) override;
  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override;
  void SetPositionState(const Maybe<dom::PositionState>& aState) override;

 private:
  ~MediaHardwareKeysEventSourceMacMediaCenter();
  void BeginListeningForEvents();
  void EndListeningForEvents();
  void HandleEvent(const dom::MediaControlAction& aAction);
  void UpdatePositionInfo();

  bool mOpened = false;
  Maybe<dom::PositionState> mPositionState;
  dom::MediaMetadataBase mMediaMetadata;

  // Should only be used on main thread
  UniquePtr<dom::FetchImageHelper> mImageFetcher;
  MozPromiseRequestHolder<dom::ImagePromise> mImageFetchRequest;

  nsString mFetchingUrl;
  nsString mCurrentImageUrl;
  size_t mNextImageIndex = 0;

  void LoadImageAtIndex(const size_t aIndex);

  MediaCenterEventHandler mPlayPauseHandler;
  MediaCenterEventHandler mNextTrackHandler;
  MediaCenterEventHandler mPreviousTrackHandler;
  MediaCenterEventHandler mPauseHandler;
  MediaCenterEventHandler mPlayHandler;
  MediaCenterEventHandler mChangePlaybackPositionHandler;
};

}  // namespace widget
}  // namespace mozilla

#endif  // WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_
