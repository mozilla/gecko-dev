/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackRegion.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TextTrackCue,
                                   DOMEventTargetHelper,
                                   mDocument,
                                   mTrack,
                                   mTrackElement,
                                   mDisplayState,
                                   mRegion)

NS_IMPL_ADDREF_INHERITED(TextTrackCue, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackCue, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackCue)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackCue::sParserWrapper;

// Set cue setting defaults based on step 19 & seq.
// in http://dev.w3.org/html5/webvtt/#parsing
void
TextTrackCue::SetDefaultCueSettings()
{
  mPosition = 50;
  mPositionAlign = AlignSetting::Middle;
  mSize = 100;
  mPauseOnExit = false;
  mSnapToLines = true;
  mLineIsAutoKeyword = true;
  mAlign = AlignSetting::Middle;
  mLineAlign = AlignSetting::Start;
  mVertical = DirectionSetting::_empty;
}

TextTrackCue::TextTrackCue(nsPIDOMWindow* aOwnerWindow,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText,
                           ErrorResult& aRv)
  : DOMEventTargetHelper(aOwnerWindow)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mReset(false)
{
  SetDefaultCueSettings();
  MOZ_ASSERT(aOwnerWindow);
  if (NS_FAILED(StashDocument())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

TextTrackCue::TextTrackCue(nsPIDOMWindow* aOwnerWindow,
                           double aStartTime,
                           double aEndTime,
                           const nsAString& aText,
                           HTMLTrackElement* aTrackElement,
                           ErrorResult& aRv)
  : DOMEventTargetHelper(aOwnerWindow)
  , mText(aText)
  , mStartTime(aStartTime)
  , mEndTime(aEndTime)
  , mTrackElement(aTrackElement)
  , mReset(false)
{
  SetDefaultCueSettings();
  MOZ_ASSERT(aOwnerWindow);
  if (NS_FAILED(StashDocument())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

TextTrackCue::~TextTrackCue()
{
}

/** Save a reference to our creating document so we don't have to
 *  keep getting it from our window.
 */
nsresult
TextTrackCue::StashDocument()
{
  nsPIDOMWindow* window = GetOwner();
  if (!window) {
    return NS_ERROR_NO_INTERFACE;
  }
  mDocument = window->GetDoc();
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

already_AddRefed<DocumentFragment>
TextTrackCue::GetCueAsHTML()
{
  // mDocument may be null during cycle collector shutdown.
  // See bug 941701.
  if (!mDocument) {
    return nullptr;
  }

  if (!sParserWrapper) {
    nsresult rv;
    nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
      do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return mDocument->CreateDocumentFragment();
    }
    sParserWrapper = parserWrapper;
    ClearOnShutdown(&sParserWrapper);
  }

  nsPIDOMWindow* window = mDocument->GetWindow();
  if (!window) {
    return mDocument->CreateDocumentFragment();
  }

  nsCOMPtr<nsIDOMHTMLElement> div;
  sParserWrapper->ConvertCueToDOMTree(window, this,
                                      getter_AddRefs(div));
  if (!div) {
    return mDocument->CreateDocumentFragment();
  }
  nsRefPtr<DocumentFragment> docFrag = mDocument->CreateDocumentFragment();
  nsCOMPtr<nsIDOMNode> throwAway;
  docFrag->AppendChild(div, getter_AddRefs(throwAway));

  return docFrag.forget();
}

void
TextTrackCue::SetTrackElement(HTMLTrackElement* aTrackElement)
{
  mTrackElement = aTrackElement;
}

JSObject*
TextTrackCue::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return VTTCueBinding::Wrap(aCx, this, aGivenProto);
}

TextTrackRegion*
TextTrackCue::GetRegion()
{
  return mRegion;
}

void
TextTrackCue::SetRegion(TextTrackRegion* aRegion)
{
  if (mRegion == aRegion) {
    return;
  }
  mRegion = aRegion;
  mReset = true;
}

} // namespace dom
} // namespace mozilla
