/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePortList.h"
#include "mozilla/dom/MessagePortListBinding.h"
#include "mozilla/dom/MessagePort.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MessagePortList, mOwner, mPorts)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MessagePortList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MessagePortList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessagePortList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
MessagePortList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MessagePortListBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
