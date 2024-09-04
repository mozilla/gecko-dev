/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ViewTransition_h
#define mozilla_dom_ViewTransition_h

#include "nsWrapperCache.h"

namespace mozilla::dom {

class Promise;
class Document;
class ViewTransitionUpdateCallback;

class ViewTransition final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(ViewTransition)

  ViewTransition(Document&, ViewTransitionUpdateCallback*);

  Promise* UpdateCallbackDone();
  Promise* Ready();
  Promise* Finished();
  void SkipTransition();

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~ViewTransition();
  RefPtr<Document> mDocument;
};

}  // namespace mozilla::dom

#endif
