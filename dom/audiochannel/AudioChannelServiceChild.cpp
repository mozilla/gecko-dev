/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelServiceChild.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_GONK
#include "SpeakerManagerService.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

StaticRefPtr<AudioChannelServiceChild> gAudioChannelServiceChild;

// static
AudioChannelService*
AudioChannelServiceChild::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  return gAudioChannelServiceChild;

}

// static
AudioChannelService*
AudioChannelServiceChild::GetOrCreateAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gAudioChannelServiceChild) {
    return gAudioChannelServiceChild;
  }

  // Create new instance, register, return
  nsRefPtr<AudioChannelServiceChild> service = new AudioChannelServiceChild();
  MOZ_ASSERT(service);

  gAudioChannelServiceChild = service;
  return gAudioChannelServiceChild;
}

void
AudioChannelServiceChild::Shutdown()
{
  if (gAudioChannelServiceChild) {
    gAudioChannelServiceChild = nullptr;
  }
}

AudioChannelServiceChild::AudioChannelServiceChild()
{
}

AudioChannelServiceChild::~AudioChannelServiceChild()
{
}

AudioChannelState
AudioChannelServiceChild::GetState(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  AudioChannelState state = AUDIO_CHANNEL_STATE_MUTED;
  bool oldElementHidden = data->mElementHidden;

  UpdateChannelType(data->mChannel, CONTENT_PROCESS_ID_MAIN, aElementHidden,
                    oldElementHidden);

  // Update visibility.
  data->mElementHidden = aElementHidden;

  ContentChild* cc = ContentChild::GetSingleton();
  cc->SendAudioChannelGetState(data->mChannel, aElementHidden, oldElementHidden,
                               &state);
  data->mState = state;
  cc->SendAudioChannelChangedNotification();

  #ifdef MOZ_WIDGET_GONK
  /** Only modify the speaker status when
   *  (1) apps in the foreground.
   *  (2) apps in the backgrund and inactive.
   *  Notice : modify only when the visible status is stable, because there
   *  has lantency in passing the visibility events.
   **/
  bool active = AnyAudioChannelIsActive();
  if (aElementHidden == oldElementHidden &&
      (!aElementHidden || (aElementHidden && !active))) {
    for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
      mSpeakerManager[i]->SetAudioChannelActive(active);
    }
  }
  #endif

  return state;
}

void
AudioChannelServiceChild::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                                    AudioChannel aChannel,
                                                    bool aWithVideo)
{
  AudioChannelService::RegisterAudioChannelAgent(aAgent, aChannel, aWithVideo);

  ContentChild::GetSingleton()->SendAudioChannelRegisterType(aChannel, aWithVideo);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "audio-channel-agent-changed", nullptr);
  }
}

void
AudioChannelServiceChild::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  AudioChannelAgentData *pData;
  if (!mAgents.Get(aAgent, &pData)) {
    return;
  }

  // We need to keep a copy because unregister will remove the
  // AudioChannelAgentData object from the hashtable.
  AudioChannelAgentData data(*pData);

  AudioChannelService::UnregisterAudioChannelAgent(aAgent);

  ContentChild::GetSingleton()->SendAudioChannelUnregisterType(
      data.mChannel, data.mElementHidden, data.mWithVideo);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "audio-channel-agent-changed", nullptr);
  }
#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive();
  for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
    mSpeakerManager[i]->SetAudioChannelActive(active);
  }
#endif
}

void
AudioChannelServiceChild::SetDefaultVolumeControlChannel(int32_t aChannel,
                                                         bool aHidden)
{
  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelChangeDefVolChannel(aChannel, aHidden);
  }
}
