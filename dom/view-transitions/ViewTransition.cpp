/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewTransition.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ViewTransitionBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ViewTransition, mDocument)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ViewTransition)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ViewTransition)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ViewTransition)

ViewTransition::ViewTransition(Document& aDoc,
                               ViewTransitionUpdateCallback* aCb)
    : mDocument(&aDoc) {}

ViewTransition::~ViewTransition() = default;

nsISupports* ViewTransition::GetParentObject() const {
  return ToSupports(mDocument.get());
}

Promise* ViewTransition::UpdateCallbackDone() {
  // TODO(emilio): Not yet implemented.
  return nullptr;
}

Promise* ViewTransition::Ready() {
  // TODO(emilio): Not yet implemented.
  return nullptr;
}

Promise* ViewTransition::Finished() {
  // TODO(emilio): Not yet implemented.
  return nullptr;
}

void ViewTransition::SkipTransition() {
  // TODO(emilio): Not yet implemented.
}

JSObject* ViewTransition::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return ViewTransition_Binding::Wrap(aCx, this, aGivenProto);
}

};  // namespace mozilla::dom
