/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemProviderUnmountEvent.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsIVirtualFileSystemRequestManager.h"

namespace mozilla {
namespace dom {

FileSystemProviderUnmountEvent::FileSystemProviderUnmountEvent(
  EventTarget* aOwner,
  nsIVirtualFileSystemRequestManager* aManager)
  : FileSystemProviderEvent(aOwner, aManager)
{

}

JSObject*
FileSystemProviderUnmountEvent::WrapObjectInternal(JSContext* aCx,
                                                   JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemProviderUnmountEventBinding::Wrap(aCx, this, aGivenProto);
}

FileSystemProviderRequestedOptions*
FileSystemProviderUnmountEvent::Options() const
{
  MOZ_ASSERT(mOptions);

  return mOptions;
}

nsresult
FileSystemProviderUnmountEvent::InitFileSystemProviderEvent(
  uint32_t aRequestId,
  nsIVirtualFileSystemRequestOption* aOption)
{
  InitFileSystemProviderEventInternal(NS_LITERAL_STRING("unmountrequested"),
                                      aRequestId,
                                      aOption);
  return NS_OK;
}

void
FileSystemProviderUnmountEvent::SuccessCallback()
{
  FileSystemProviderEvent::OnSuccess(nullptr, false);
}

} // namespace dom
} // namespace mozilla
