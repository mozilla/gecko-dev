/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/Event.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsComponentManagerUtils.h"
#include "nsVideoFrame.h"
#include "nsIFrame.h"
#include "nsTArrayHelpers.h"
#include "nsIWebVTTParserWrapper.h"

namespace mozilla {
namespace dom {

CompareTextTracks::CompareTextTracks(HTMLMediaElement* aMediaElement)
{
  mMediaElement = aMediaElement;
}

int32_t
CompareTextTracks::TrackChildPosition(TextTrack* aTextTrack) const {
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return -1;
  }
  return mMediaElement->IndexOf(trackElement);
}

bool
CompareTextTracks::Equals(TextTrack* aOne, TextTrack* aTwo) const {
  // Two tracks can never be equal. If they have corresponding TrackElements
  // they would need to occupy the same tree position (impossible) and in the
  // case of tracks coming from AddTextTrack source we put the newest at the
  // last position, so they won't be equal as well.
  return false;
}

bool
CompareTextTracks::LessThan(TextTrack* aOne, TextTrack* aTwo) const
{
  TextTrackSource sourceOne = aOne->GetTextTrackSource();
  TextTrackSource sourceTwo = aTwo->GetTextTrackSource();
  if (sourceOne != sourceTwo) {
    return sourceOne == Track ||
           (sourceOne == AddTextTrack && sourceTwo == MediaResourceSpecific);
  }
  switch (sourceOne) {
    case Track: {
      int32_t positionOne = TrackChildPosition(aOne);
      int32_t positionTwo = TrackChildPosition(aTwo);
      // If either position one or positiontwo are -1 then something has gone
      // wrong. In this case we should just put them at the back of the list.
      return positionOne != -1 && positionTwo != -1 &&
             positionOne < positionTwo;
    }
    case AddTextTrack:
      // For AddTextTrack sources the tracks will already be in the correct relative
      // order in the source array. Assume we're called in iteration order and can
      // therefore always report aOne < aTwo to maintain the original temporal ordering.
      return true;
    case MediaResourceSpecific:
      // No rules for Media Resource Specific tracks yet.
      break;
  }
  return true;
}

NS_IMPL_CYCLE_COLLECTION(TextTrackManager, mMediaElement, mTextTracks,
                         mPendingTextTracks, mNewCues)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TextTrackManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextTrackManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextTrackManager)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackManager::sParserWrapper;

TextTrackManager::TextTrackManager(HTMLMediaElement *aMediaElement)
  : mMediaElement(aMediaElement)
  , performedTrackSelection(false)
{
  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* scriptObject =
    mMediaElement->OwnerDoc()->GetScriptHandlingObject(hasHadScriptObject);

  NS_ENSURE_TRUE_VOID(scriptObject || !hasHadScriptObject);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(scriptObject);
  mNewCues = new TextTrackCueList(window);
  mTextTracks = new TextTrackList(window, this);
  mPendingTextTracks = new TextTrackList(window, this);

  if (!sParserWrapper) {
    nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
      do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID);
    sParserWrapper = parserWrapper;
    ClearOnShutdown(&sParserWrapper);
  }
}

TextTrackManager::~TextTrackManager()
{
}

TextTrackList*
TextTrackManager::TextTracks() const
{
  return mTextTracks;
}

already_AddRefed<TextTrack>
TextTrackManager::AddTextTrack(TextTrackKind aKind, const nsAString& aLabel,
                               const nsAString& aLanguage,
                               TextTrackMode aMode,
                               TextTrackReadyState aReadyState,
                               TextTrackSource aTextTrackSource)
{
  if (!mMediaElement || !mTextTracks) {
    return nullptr;
  }
  nsRefPtr<TextTrack> ttrack =
    mTextTracks->AddTextTrack(aKind, aLabel, aLanguage, aMode, aReadyState,
                              aTextTrackSource, CompareTextTracks(mMediaElement));
  AddCues(ttrack);

  if (aTextTrackSource == Track) {
    HonorUserPreferencesForTrackSelection();
  }

  return ttrack.forget();
}

void
TextTrackManager::AddTextTrack(TextTrack* aTextTrack)
{
  if (!mMediaElement || !mTextTracks) {
    return;
  }
  mTextTracks->AddTextTrack(aTextTrack, CompareTextTracks(mMediaElement));
  AddCues(aTextTrack);
  if (aTextTrack->GetTextTrackSource() == Track) {
    HonorUserPreferencesForTrackSelection();
  }
}

void
TextTrackManager::AddCues(TextTrack* aTextTrack)
{
  if (!mNewCues) {
    return;
  }

  TextTrackCueList* cueList = aTextTrack->GetCues();
  if (cueList) {
    bool dummy;
    for (uint32_t i = 0; i < cueList->Length(); ++i) {
      mNewCues->AddCue(*cueList->IndexedGetter(i, dummy));
    }
  }
}

void
TextTrackManager::RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly)
{
  if (!mPendingTextTracks || !mTextTracks) {
    return;
  }

  mPendingTextTracks->RemoveTextTrack(aTextTrack);
  if (aPendingListOnly) {
    return;
  }

  mTextTracks->RemoveTextTrack(aTextTrack);
}

void
TextTrackManager::DidSeek()
{
  if (mTextTracks) {
    mTextTracks->DidSeek();
  }
}

void
TextTrackManager::UpdateCueDisplay()
{
  if (!mMediaElement || !mTextTracks) {
    return;
  }

  nsIFrame* frame = mMediaElement->GetPrimaryFrame();
  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    return;
  }

  nsCOMPtr<nsIContent> overlay = videoFrame->GetCaptionOverlay();
  if (!overlay) {
    return;
  }

  nsTArray<nsRefPtr<TextTrackCue> > activeCues;
  mTextTracks->UpdateAndGetShowingCues(activeCues);

  if (activeCues.Length() > 0) {
    nsCOMPtr<nsIWritableVariant> jsCues =
      do_CreateInstance("@mozilla.org/variant;1");

    jsCues->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                       &NS_GET_IID(nsIDOMEventTarget),
                       activeCues.Length(),
                       static_cast<void*>(activeCues.Elements()));

    nsPIDOMWindow* window = mMediaElement->OwnerDoc()->GetWindow();
    if (window) {
      sParserWrapper->ProcessCues(window, jsCues, overlay);
    }
  } else if (overlay->Length() > 0) {
    nsContentUtils::SetNodeTextContent(overlay, EmptyString(), true);
  }
}

void
TextTrackManager::AddCue(TextTrackCue& aCue)
{
  if (mNewCues) {
    mNewCues->AddCue(aCue);
  }
}

void
TextTrackManager::PopulatePendingList()
{
  if (!mTextTracks || !mPendingTextTracks || !mMediaElement) {
    return;
  }
  uint32_t len = mTextTracks->Length();
  bool dummy;
  for (uint32_t index = 0; index < len; ++index) {
    TextTrack* ttrack = mTextTracks->IndexedGetter(index, dummy);
    if (ttrack && ttrack->Mode() != TextTrackMode::Disabled &&
        ttrack->ReadyState() == TextTrackReadyState::Loading) {
      mPendingTextTracks->AddTextTrack(ttrack,
                                       CompareTextTracks(mMediaElement));
    }
  }
}

void
TextTrackManager::AddListeners()
{
  if (mMediaElement) {
    mMediaElement->AddEventListener(NS_LITERAL_STRING("resizevideocontrols"),
                                    this, false, false);
  }
}

void
TextTrackManager::HonorUserPreferencesForTrackSelection()
{
  if (performedTrackSelection || !mTextTracks) {
    return;
  }

  TextTrackKind ttKinds[] = { TextTrackKind::Captions,
                              TextTrackKind::Subtitles };

  // Steps 1 - 3: Perform automatic track selection for different TextTrack
  // Kinds.
  PerformTrackSelection(ttKinds, ArrayLength(ttKinds));
  PerformTrackSelection(TextTrackKind::Descriptions);
  PerformTrackSelection(TextTrackKind::Chapters);

  // Step 4: Set all TextTracks with a kind of metadata that are disabled
  // to hidden.
  for (uint32_t i = 0; i < mTextTracks->Length(); i++) {
    TextTrack* track = (*mTextTracks)[i];
    if (track->Kind() == TextTrackKind::Metadata && TrackIsDefault(track) &&
        track->Mode() == TextTrackMode::Disabled) {
      track->SetMode(TextTrackMode::Hidden);
    }
  }

  performedTrackSelection = true;
}

bool
TextTrackManager::TrackIsDefault(TextTrack* aTextTrack)
{
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return false;
  }
  return trackElement->Default();
}

void
TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKind)
{
  TextTrackKind ttKinds[] = { aTextTrackKind };
  PerformTrackSelection(ttKinds, ArrayLength(ttKinds));
}

void
TextTrackManager::PerformTrackSelection(TextTrackKind aTextTrackKinds[],
                                        uint32_t size)
{
  nsTArray<TextTrack*> candidates;
  GetTextTracksOfKinds(aTextTrackKinds, size, candidates);

  // Step 3: If any TextTracks in candidates are showing then abort these steps.
  for (uint32_t i = 0; i < candidates.Length(); i++) {
    if (candidates[i]->Mode() == TextTrackMode::Showing) {
      return;
    }
  }

  // Step 4: Honor user preferences for track selection, otherwise, set the
  // first TextTrack in candidates with a default attribute to showing.
  // TODO: Bug 981691 - Honor user preferences for text track selection.
  for (uint32_t i = 0; i < candidates.Length(); i++) {
    if (TrackIsDefault(candidates[i]) &&
        candidates[i]->Mode() == TextTrackMode::Disabled) {
      candidates[i]->SetMode(TextTrackMode::Showing);
      return;
    }
  }
}

void
TextTrackManager::GetTextTracksOfKinds(TextTrackKind aTextTrackKinds[],
                                       uint32_t size,
                                       nsTArray<TextTrack*>& aTextTracks)
{
  for (uint32_t i = 0; i < size; i++) {
    GetTextTracksOfKind(aTextTrackKinds[i], aTextTracks);
  }
}

void
TextTrackManager::GetTextTracksOfKind(TextTrackKind aTextTrackKind,
                                      nsTArray<TextTrack*>& aTextTracks)
{
  if (!mTextTracks) {
    return;
  }
  for (uint32_t i = 0; i < mTextTracks->Length(); i++) {
    TextTrack* textTrack = (*mTextTracks)[i];
    if (textTrack->Kind() == aTextTrackKind) {
      aTextTracks.AppendElement(textTrack);
    }
  }
}

NS_IMETHODIMP
TextTrackManager::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mTextTracks) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("resizevideocontrols")) {
    for (uint32_t i = 0; i< mTextTracks->Length(); i++) {
      ((*mTextTracks)[i])->SetCuesDirty();
    }
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
