/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <MediaPlayer/MediaPlayer.h>

#include "MediaHardwareKeysEventSourceMacMediaCenter.h"

#include "mozilla/dom/MediaControlUtils.h"
#include "nsCocoaUtils.h"

using namespace mozilla::dom;

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                                                   \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug,                            \
          ("MediaHardwareKeysEventSourceMacMediaCenter=%p, " msg, this, \
           ##__VA_ARGS__))

namespace mozilla {
namespace widget {

MediaCenterEventHandler
MediaHardwareKeysEventSourceMacMediaCenter::CreatePlayPauseHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    center.playbackState =
        center.playbackState == MPNowPlayingPlaybackStatePlaying
            ? MPNowPlayingPlaybackStatePaused
            : MPNowPlayingPlaybackStatePlaying;
    HandleEvent(MediaControlAction(MediaControlKey::Playpause));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler
MediaHardwareKeysEventSourceMacMediaCenter::CreateNextTrackHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    HandleEvent(MediaControlAction(MediaControlKey::Nexttrack));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler
MediaHardwareKeysEventSourceMacMediaCenter::CreatePreviousTrackHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    HandleEvent(MediaControlAction(MediaControlKey::Previoustrack));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler
MediaHardwareKeysEventSourceMacMediaCenter::CreatePlayHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    if (center.playbackState != MPNowPlayingPlaybackStatePlaying) {
      center.playbackState = MPNowPlayingPlaybackStatePlaying;
    }
    HandleEvent(MediaControlAction(MediaControlKey::Play));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler
MediaHardwareKeysEventSourceMacMediaCenter::CreatePauseHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    if (center.playbackState != MPNowPlayingPlaybackStatePaused) {
      center.playbackState = MPNowPlayingPlaybackStatePaused;
    }
    HandleEvent(MediaControlAction(MediaControlKey::Pause));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::
    CreateChangePlaybackPositionHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPChangePlaybackPositionCommandEvent* changePosEvent =
        (MPChangePlaybackPositionCommandEvent*)event;
    HandleEvent(
        MediaControlAction(MediaControlKey::Seekto,
                           SeekDetails(changePosEvent.positionTime, false)));
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaHardwareKeysEventSourceMacMediaCenter::
    MediaHardwareKeysEventSourceMacMediaCenter() {
  mPlayPauseHandler = CreatePlayPauseHandler();
  mNextTrackHandler = CreateNextTrackHandler();
  mPreviousTrackHandler = CreatePreviousTrackHandler();
  mPlayHandler = CreatePlayHandler();
  mPauseHandler = CreatePauseHandler();
  mChangePlaybackPositionHandler = CreateChangePlaybackPositionHandler();
  LOG("Create MediaHardwareKeysEventSourceMacMediaCenter");
}

MediaHardwareKeysEventSourceMacMediaCenter::
    ~MediaHardwareKeysEventSourceMacMediaCenter() {
  LOG("Destroy MediaHardwareKeysEventSourceMacMediaCenter");
  EndListeningForEvents();
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStateStopped;
}

void MediaHardwareKeysEventSourceMacMediaCenter::BeginListeningForEvents() {
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStatePlaying;
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  commandCenter.togglePlayPauseCommand.enabled = false;
  [commandCenter.togglePlayPauseCommand addTargetWithHandler:mPlayPauseHandler];
  commandCenter.nextTrackCommand.enabled = false;
  [commandCenter.nextTrackCommand addTargetWithHandler:mNextTrackHandler];
  commandCenter.previousTrackCommand.enabled = false;
  [commandCenter.previousTrackCommand
      addTargetWithHandler:mPreviousTrackHandler];
  commandCenter.playCommand.enabled = false;
  [commandCenter.playCommand addTargetWithHandler:mPlayHandler];
  commandCenter.pauseCommand.enabled = false;
  [commandCenter.pauseCommand addTargetWithHandler:mPauseHandler];
  commandCenter.changePlaybackPositionCommand.enabled = false;
  [commandCenter.changePlaybackPositionCommand
      addTargetWithHandler:mChangePlaybackPositionHandler];
}

void MediaHardwareKeysEventSourceMacMediaCenter::EndListeningForEvents() {
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStatePaused;
  center.nowPlayingInfo = nil;
  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  commandCenter.togglePlayPauseCommand.enabled = false;
  [commandCenter.togglePlayPauseCommand removeTarget:nil];
  commandCenter.nextTrackCommand.enabled = false;
  [commandCenter.nextTrackCommand removeTarget:nil];
  commandCenter.previousTrackCommand.enabled = false;
  [commandCenter.previousTrackCommand removeTarget:nil];
  commandCenter.playCommand.enabled = false;
  [commandCenter.playCommand removeTarget:nil];
  commandCenter.pauseCommand.enabled = false;
  [commandCenter.pauseCommand removeTarget:nil];
  commandCenter.changePlaybackPositionCommand.enabled = false;
  [commandCenter.changePlaybackPositionCommand removeTarget:nil];
}

bool MediaHardwareKeysEventSourceMacMediaCenter::Open() {
  LOG("Open MediaHardwareKeysEventSourceMacMediaCenter");
  mOpened = true;
  BeginListeningForEvents();
  return true;
}

void MediaHardwareKeysEventSourceMacMediaCenter::Close() {
  LOG("Close MediaHardwareKeysEventSourceMacMediaCenter");
  SetPlaybackState(MediaSessionPlaybackState::None);
  EndListeningForEvents();
  mOpened = false;
  MediaControlKeySource::Close();
}

bool MediaHardwareKeysEventSourceMacMediaCenter::IsOpened() const {
  return mOpened;
}

void MediaHardwareKeysEventSourceMacMediaCenter::HandleEvent(
    const MediaControlAction& aAction) {
  for (auto iter = mListeners.begin(); iter != mListeners.end(); ++iter) {
    (*iter)->OnActionPerformed(aAction);
  }
}

void MediaHardwareKeysEventSourceMacMediaCenter::SetPlaybackState(
    MediaSessionPlaybackState aState) {
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  if (aState == MediaSessionPlaybackState::Playing) {
    center.playbackState = MPNowPlayingPlaybackStatePlaying;
  } else if (aState == MediaSessionPlaybackState::Paused) {
    center.playbackState = MPNowPlayingPlaybackStatePaused;
    UpdatePositionInfo();
  } else if (aState == MediaSessionPlaybackState::None) {
    center.playbackState = MPNowPlayingPlaybackStateStopped;
  }
  MediaControlKeySource::SetPlaybackState(aState);
}

void MediaHardwareKeysEventSourceMacMediaCenter::SetMediaMetadata(
    const MediaMetadataBase& aMetadata) {
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  NSMutableDictionary* nowPlayingInfo =
      [center.nowPlayingInfo mutableCopy] ?: [NSMutableDictionary dictionary];

  [nowPlayingInfo setObject:nsCocoaUtils::ToNSString(aMetadata.mTitle)
                     forKey:MPMediaItemPropertyTitle];
  [nowPlayingInfo setObject:nsCocoaUtils::ToNSString(aMetadata.mArtist)
                     forKey:MPMediaItemPropertyArtist];
  [nowPlayingInfo setObject:nsCocoaUtils::ToNSString(aMetadata.mAlbum)
                     forKey:MPMediaItemPropertyAlbumTitle];
  // The procedure of updating `nowPlayingInfo` is actually an async operation
  // from our testing, Apple's documentation doesn't mention that though. So be
  // aware that checking `nowPlayingInfo` immedately after setting it might not
  // yield the expected result.
  center.nowPlayingInfo = nowPlayingInfo;
}

void MediaHardwareKeysEventSourceMacMediaCenter::SetSupportedMediaKeys(
    const MediaKeysArray& aSupportedKeys) {
  uint32_t supportedKeys = 0;
  for (const MediaControlKey& key : aSupportedKeys) {
    supportedKeys |= GetMediaKeyMask(key);
  }

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];
  commandCenter.togglePlayPauseCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Playpause);
  commandCenter.nextTrackCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Nexttrack);
  commandCenter.previousTrackCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Previoustrack);
  commandCenter.playCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Play);
  commandCenter.pauseCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Pause);
  commandCenter.changePlaybackPositionCommand.enabled =
      supportedKeys & GetMediaKeyMask(MediaControlKey::Seekto);
}

void MediaHardwareKeysEventSourceMacMediaCenter::SetPositionState(
    const Maybe<PositionState>& aState) {
  mPositionState = aState;
  UpdatePositionInfo();
}

void MediaHardwareKeysEventSourceMacMediaCenter::UpdatePositionInfo() {
  if (mPositionState.isSome()) {
    MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
    NSMutableDictionary* nowPlayingInfo =
        [center.nowPlayingInfo mutableCopy] ?: [NSMutableDictionary dictionary];

    [nowPlayingInfo setObject:@(mPositionState->mDuration)
                       forKey:MPMediaItemPropertyPlaybackDuration];
    [nowPlayingInfo setObject:@(mPositionState->CurrentPlaybackPosition())
                       forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [nowPlayingInfo
        setObject:@(center.playbackState == MPNowPlayingPlaybackStatePlaying
                        ? mPositionState->mPlaybackRate
                        : 0.0)
           forKey:MPNowPlayingInfoPropertyPlaybackRate];
    center.nowPlayingInfo = nowPlayingInfo;
  }
}

}  // namespace widget
}  // namespace mozilla
