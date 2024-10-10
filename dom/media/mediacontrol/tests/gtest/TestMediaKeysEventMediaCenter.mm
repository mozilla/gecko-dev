/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#import <MediaPlayer/MediaPlayer.h>

#include "gtest/gtest.h"
#include "MediaHardwareKeysEventSourceMacMediaCenter.h"
#include "MediaKeyListenerTest.h"
#include "mozilla/TimeStamp.h"
#include "nsCocoaUtils.h"
#include "prinrval.h"
#include "prthread.h"

using namespace mozilla::dom;
using namespace mozilla::widget;

NS_ASSUME_NONNULL_BEGIN

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayPauseEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler playPauseHandler = source->CreatePlayPauseHandler();
  playPauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Playpause));

  listener->Clear();  // Reset stored media key

  playPauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Playpause));

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];

  ASSERT_TRUE(!commandCenter.togglePlayPauseCommand.enabled);

  source->SetSupportedMediaKeys({MediaControlKey::Playpause});

  ASSERT_TRUE(commandCenter.togglePlayPauseCommand.enabled);
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler playHandler = source->CreatePlayHandler();

  center.playbackState = MPNowPlayingPlaybackStatePaused;

  playHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Play));

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];

  ASSERT_TRUE(!commandCenter.playCommand.enabled);

  source->SetSupportedMediaKeys({MediaControlKey::Play});

  ASSERT_TRUE(commandCenter.playCommand.enabled);
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPauseEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler pauseHandler = source->CreatePauseHandler();

  pauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Pause));

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];

  ASSERT_TRUE(!commandCenter.pauseCommand.enabled);

  source->SetSupportedMediaKeys({MediaControlKey::Pause});

  ASSERT_TRUE(commandCenter.pauseCommand.enabled);
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPrevNextEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  MediaCenterEventHandler nextHandler = source->CreateNextTrackHandler();

  nextHandler(nil);

  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Nexttrack));

  MediaCenterEventHandler previousHandler =
      source->CreatePreviousTrackHandler();

  previousHandler(nil);

  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Previoustrack));

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];

  ASSERT_TRUE(!commandCenter.previousTrackCommand.enabled);
  ASSERT_TRUE(!commandCenter.nextTrackCommand.enabled);

  source->SetSupportedMediaKeys(
      {MediaControlKey::Previoustrack, MediaControlKey::Nexttrack});

  ASSERT_TRUE(commandCenter.previousTrackCommand.enabled);
  ASSERT_TRUE(commandCenter.nextTrackCommand.enabled);
}

@interface MockChangePlaybackPositionCommandEvent : MPRemoteCommandEvent
@property(nonatomic, assign) NSTimeInterval positionTime;
@end

@implementation MockChangePlaybackPositionCommandEvent

- (instancetype)initWithPositionTime:(NSTimeInterval)time {
  _positionTime = time;
  return self;
}

@end

TEST(MediaHardwareKeysEventSourceMacMediaCenter,
     TestMediaCenterChangePlaybackPositionEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  MediaCenterEventHandler changePositionHandler =
      source->CreateChangePlaybackPositionHandler();

  double seekPosition = 5.0;
  MockChangePlaybackPositionCommandEvent* event =
      [[MockChangePlaybackPositionCommandEvent alloc]
          initWithPositionTime:seekPosition];
  changePositionHandler(event);

  ASSERT_TRUE(listener->IsKeyEqualTo(MediaControlKey::Seekto));
  mozilla::Maybe<SeekDetails> seekDetails = listener->GetSeekDetails();
  ASSERT_TRUE(seekDetails->mAbsolute->mSeekTime == seekPosition);

  MPRemoteCommandCenter* commandCenter =
      [MPRemoteCommandCenter sharedCommandCenter];

  ASSERT_TRUE(!commandCenter.changePlaybackPositionCommand.enabled);

  source->SetSupportedMediaKeys({MediaControlKey::Seekto});

  ASSERT_TRUE(commandCenter.changePlaybackPositionCommand.enabled);
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestSetMetadata)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  MediaMetadataBase metadata;
  metadata.mTitle = u"MediaPlayback";
  metadata.mArtist = u"Firefox";
  metadata.mAlbum = u"Mozilla";
  source->SetMediaMetadata(metadata);

  // The update procedure of nowPlayingInfo is async, so wait for a second
  // before checking the result.
  PR_Sleep(PR_SecondsToInterval(1));
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyTitle]
      isEqualToString:@"MediaPlayback"]);
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyArtist]
      isEqualToString:@"Firefox"]);
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyAlbumTitle]
      isEqualToString:@"Mozilla"]);

  source->Close();
  PR_Sleep(PR_SecondsToInterval(1));
  ASSERT_TRUE(center.nowPlayingInfo == nil);
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterSetPosition)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  PositionState positionState;
  positionState.mDuration = 10.0;
  positionState.mPlaybackRate = 1.0;
  positionState.mLastReportedPlaybackPosition = 5.0;
  positionState.mPositionUpdatedTime = mozilla::TimeStamp::Now();
  source->SetPositionState(mozilla::Some(positionState));

  PR_Sleep(PR_SecondsToInterval(1));
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyPlaybackDuration]
      isEqualToNumber:@10.0]);
  ASSERT_TRUE(
      fabs([center.nowPlayingInfo[MPNowPlayingInfoPropertyElapsedPlaybackTime]
               doubleValue] -
           5.0) < 0.1);
  ASSERT_TRUE([center.nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate]
      isEqualToNumber:@1.0]);

  source->SetPlaybackState(MediaSessionPlaybackState::Paused);

  PR_Sleep(PR_SecondsToInterval(1));
  ASSERT_TRUE([center.nowPlayingInfo[MPNowPlayingInfoPropertyPlaybackRate]
      isEqualToNumber:@0.0]);
}

NS_ASSUME_NONNULL_END
