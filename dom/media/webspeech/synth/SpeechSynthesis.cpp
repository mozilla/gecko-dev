/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSpeechTask.h"
#include "mozilla/Logging.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Element.h"

#include "mozilla/dom/SpeechSynthesisBinding.h"
#include "SpeechSynthesis.h"
#include "nsSynthVoiceRegistry.h"
#include "nsIDocument.h"

#undef LOG
PRLogModuleInfo*
GetSpeechSynthLog()
{
  static PRLogModuleInfo* sLog = nullptr;

  if (!sLog) {
    sLog = PR_NewLogModule("SpeechSynthesis");
  }

  return sLog;
}
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

namespace mozilla {
namespace dom {

static PLDHashOperator
TraverseCachedVoices(const nsAString& aKey, SpeechSynthesisVoice* aEntry, void* aData)
{
  nsCycleCollectionTraversalCallback* cb = static_cast<nsCycleCollectionTraversalCallback*>(aData);
  cb->NoteXPCOMChild(aEntry);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SpeechSynthesis)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSpeechQueue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mVoiceCache.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentTask)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSpeechQueue)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  tmp->mVoiceCache.EnumerateRead(TraverseCachedVoices, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(SpeechSynthesis)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechSynthesis)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechSynthesis)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechSynthesis)

SpeechSynthesis::SpeechSynthesis(nsPIDOMWindow* aParent)
  : mParent(aParent)
{
}

SpeechSynthesis::~SpeechSynthesis()
{
}

JSObject*
SpeechSynthesis::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechSynthesisBinding::Wrap(aCx, this, aGivenProto);
}

nsIDOMWindow*
SpeechSynthesis::GetParentObject() const
{
  return mParent;
}

bool
SpeechSynthesis::Pending() const
{
  switch (mSpeechQueue.Length()) {
  case 0:
    return false;

  case 1:
    return mSpeechQueue.ElementAt(0)->GetState() == SpeechSynthesisUtterance::STATE_PENDING;

  default:
    return true;
  }
}

bool
SpeechSynthesis::Speaking() const
{
  if (mSpeechQueue.IsEmpty()) {
    return false;
  }

  return mSpeechQueue.ElementAt(0)->GetState() == SpeechSynthesisUtterance::STATE_SPEAKING;
}

bool
SpeechSynthesis::Paused() const
{
  if (mSpeechQueue.IsEmpty()) {
    return false;
  }

  return mSpeechQueue.ElementAt(0)->IsPaused();
}

void
SpeechSynthesis::Speak(SpeechSynthesisUtterance& aUtterance)
{
  if (aUtterance.mState != SpeechSynthesisUtterance::STATE_NONE) {
    // XXX: Should probably raise an error
    return;
  }

  mSpeechQueue.AppendElement(&aUtterance);
  aUtterance.mState = SpeechSynthesisUtterance::STATE_PENDING;

  if (mSpeechQueue.Length() == 1 && !mCurrentTask) {
    AdvanceQueue();
  }
}

void
SpeechSynthesis::AdvanceQueue()
{
  LOG(LogLevel::Debug,
      ("SpeechSynthesis::AdvanceQueue length=%d", mSpeechQueue.Length()));

  if (mSpeechQueue.IsEmpty()) {
    return;
  }

  nsRefPtr<SpeechSynthesisUtterance> utterance = mSpeechQueue.ElementAt(0);

  nsAutoString docLang;
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mParent);
  nsIDocument* doc = win->GetExtantDoc();

  if (doc) {
    Element* elm = doc->GetHtmlElement();

    if (elm) {
      elm->GetLang(docLang);
    }
  }

  mCurrentTask =
    nsSynthVoiceRegistry::GetInstance()->SpeakUtterance(*utterance, docLang);

  if (mCurrentTask) {
    mCurrentTask->SetSpeechSynthesis(this);
  }

  return;
}

void
SpeechSynthesis::Cancel()
{
  if (mCurrentTask) {
   if (mSpeechQueue.Length() > 1) {
      // Remove all queued utterances except for current one.
      mSpeechQueue.RemoveElementsAt(1, mSpeechQueue.Length() - 1);
    }

   mCurrentTask->Cancel();
  }
}

void
SpeechSynthesis::Pause()
{
  if (mCurrentTask && !Paused() && (Speaking() || Pending())) {
    mCurrentTask->Pause();
  }
}

void
SpeechSynthesis::Resume()
{
  if (mCurrentTask && Paused()) {
    mCurrentTask->Resume();
  }
}

void
SpeechSynthesis::OnEnd(const nsSpeechTask* aTask)
{
  MOZ_ASSERT(mCurrentTask == aTask);

  if (!mSpeechQueue.IsEmpty()) {
    mSpeechQueue.RemoveElementAt(0);
  }

  mCurrentTask = nullptr;
  AdvanceQueue();
}

void
SpeechSynthesis::GetVoices(nsTArray< nsRefPtr<SpeechSynthesisVoice> >& aResult)
{
  aResult.Clear();
  uint32_t voiceCount = 0;

  nsresult rv = nsSynthVoiceRegistry::GetInstance()->GetVoiceCount(&voiceCount);
  NS_ENSURE_SUCCESS_VOID(rv);

  for (uint32_t i = 0; i < voiceCount; i++) {
    nsAutoString uri;
    rv = nsSynthVoiceRegistry::GetInstance()->GetVoice(i, uri);

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to retrieve voice from registry");
      continue;
    }

    SpeechSynthesisVoice* voice = mVoiceCache.GetWeak(uri);

    if (!voice) {
      voice = new SpeechSynthesisVoice(this, uri);
    }

    aResult.AppendElement(voice);
  }

  mVoiceCache.Clear();

  for (uint32_t i = 0; i < aResult.Length(); i++) {
    SpeechSynthesisVoice* voice = aResult[i];
    mVoiceCache.Put(voice->mUri, voice);
  }
}

} // namespace dom
} // namespace mozilla
