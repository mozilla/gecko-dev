/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaSession_h
#define mozilla_dom_MediaSession_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class MediaSession final : public nsISupports, public nsWrapperCache {
 public:
  // Ref counting and cycle collection
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaSession)

  explicit MediaSession(nsPIDOMWindowInner* aParent);

  // WebIDL methods
  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  MediaMetadata* GetMetadata() const;

  void SetMetadata(MediaMetadata* aMetadata);

  void SetActionHandler(MediaSessionAction aAction,
                        MediaSessionActionHandler* aHandler);

  MOZ_CAN_RUN_SCRIPT
  void NotifyHandler(const MediaSessionActionDetails& aDetails);

 private:
  ~MediaSession() = default;

  nsCOMPtr<nsPIDOMWindowInner> mParent;

  RefPtr<MediaMetadata> mMediaMetadata;
  static const size_t ACTIONS = MediaSessionActionValues::Count;
  RefPtr<MediaSessionActionHandler> mActionHandlers[ACTIONS] = {nullptr};
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaSession_h
