/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionArray.h"
#include "mozilla/dom/MozMobileConnectionArrayBinding.h"
#include "mozilla/Preferences.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(MobileConnectionArray)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MobileConnectionArray)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  // Notify our mobile connections that we're going away.
  tmp->DropConnections();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(MobileConnectionArray)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileConnections)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(MobileConnectionArray)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileConnectionArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileConnectionArray)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileConnectionArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileConnectionArray::MobileConnectionArray(nsPIDOMWindow* aWindow)
  : mInitialized(false)
  , mWindow(aWindow)
{
  uint32_t numRil = mozilla::Preferences::GetUint("ril.numRadioInterfaces", 1);
  MOZ_ASSERT(numRil > 0);

  mMobileConnections.SetLength(numRil);

  SetIsDOMBinding();
}

MobileConnectionArray::~MobileConnectionArray()
{
  DropConnections();
}

void
MobileConnectionArray::Init()
{
  mInitialized = true;

  for (uint32_t id = 0; id < mMobileConnections.Length(); id++) {
    nsRefPtr<MobileConnection> mobileConnection = new MobileConnection(mWindow, id);
    mMobileConnections[id] = mobileConnection;
  }
}

void
MobileConnectionArray::DropConnections()
{
  if (mInitialized) {
    for (uint32_t i = 0; i < mMobileConnections.Length(); i++) {
      mMobileConnections[i]->Shutdown();
    }
  }

  mMobileConnections.Clear();
}

nsPIDOMWindow*
MobileConnectionArray::GetParentObject() const
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

JSObject*
MobileConnectionArray::WrapObject(JSContext* aCx)
{
  return MozMobileConnectionArrayBinding::Wrap(aCx, this);
}

MobileConnection*
MobileConnectionArray::Item(uint32_t aIndex)
{
  bool unused;
  return IndexedGetter(aIndex, unused);
}

uint32_t
MobileConnectionArray::Length() const
{
  return mMobileConnections.Length();
}

MobileConnection*
MobileConnectionArray::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  if (!mInitialized) {
    Init();
  }

  aFound = false;
  aFound = aIndex < mMobileConnections.Length();

  return aFound ? mMobileConnections[aIndex] : nullptr;
}
