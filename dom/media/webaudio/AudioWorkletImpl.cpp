/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletImpl.h"

#include "AudioContext.h"
#include "AudioNodeTrack.h"
#include "AudioWorklet.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/AudioWorkletBinding.h"
#include "mozilla/dom/AudioWorkletGlobalScope.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/WorkletThread.h"
#include "nsGlobalWindowInner.h"

namespace mozilla {

/* static */ already_AddRefed<dom::AudioWorklet>
AudioWorkletImpl::CreateWorklet(dom::AudioContext* aContext, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsGlobalWindowInner* window = aContext->GetOwnerWindow();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsIPrincipal* principal = window->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<dom::MessageChannel> messageChannel =
      dom::MessageChannel::Constructor(window, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  dom::UniqueMessagePortId globalScopePortId;
  messageChannel->Port2()->CloneAndDisentangle(globalScopePortId);

  RefPtr<AudioWorkletImpl> impl =
      new AudioWorkletImpl(window, principal, aContext->DestinationTrack(),
                           std::move(globalScopePortId));

  // The Worklet owns a reference to the AudioContext so as to keep the graph
  // thread running as long as the Worklet is alive by keeping the
  // AudioDestinationNode alive.
  return MakeAndAddRef<dom::AudioWorklet>(
      window, std::move(impl), ToSupports(aContext), messageChannel->Port1());
}

AudioWorkletImpl::AudioWorkletImpl(nsPIDOMWindowInner* aWindow,
                                   nsIPrincipal* aPrincipal,
                                   AudioNodeTrack* aDestinationTrack,
                                   dom::UniqueMessagePortId&& aPortIdentifier)
    : WorkletImpl(aWindow, aPrincipal),
      mDestinationTrack(aDestinationTrack),
      mGlobalScopePortIdentifier(std::move(aPortIdentifier)) {}

AudioWorkletImpl::~AudioWorkletImpl() = default;

JSObject* AudioWorkletImpl::WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                                        JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(NS_IsMainThread());
  return dom::AudioWorklet_Binding::Wrap(
      aCx, static_cast<dom::AudioWorklet*>(aWorklet), aGivenProto);
}

nsresult AudioWorkletImpl::SendControlMessage(
    already_AddRefed<nsIRunnable> aRunnable) {
  mDestinationTrack->SendRunnable(std::move(aRunnable));
  return NS_OK;
}

void AudioWorkletImpl::OnAddModuleStarted() const {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker(ProfilerStringView("AudioWorklet.addModule"),
                      geckoprofiler::category::MEDIA_RT,
                      {MarkerTiming::IntervalStart()});
#endif
}

void AudioWorkletImpl::OnAddModulePromiseSettled() const {
#ifdef MOZ_GECKO_PROFILER
  profiler_add_marker(ProfilerStringView("AudioWorklet.addModule"),
                      geckoprofiler::category::MEDIA_RT,
                      {MarkerTiming::IntervalEnd()});
#endif
}

already_AddRefed<dom::WorkletGlobalScope>
AudioWorkletImpl::ConstructGlobalScope(JSContext* aCx) {
  dom::WorkletThread::AssertIsOnWorkletThread();

  RefPtr<dom::AudioWorkletGlobalScope> globalScope =
      new dom::AudioWorkletGlobalScope(this);

  ErrorResult rv;
  RefPtr<dom::MessagePort> deserializedPort =
      dom::MessagePort::Create(globalScope, mGlobalScopePortIdentifier, rv);
  if (NS_WARN_IF(rv.MaybeSetPendingException(aCx))) {
    // The exception will be propagated into the global
    return globalScope.forget();
  }

  globalScope->SetPort(deserializedPort);

  return globalScope.forget();
}

}  // namespace mozilla
