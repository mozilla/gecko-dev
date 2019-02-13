/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaResourceHandler.h"

namespace android {

MediaResourceHandler::MediaResourceHandler(const wp<ResourceListener> &aListener)
  : mListener(aListener)
  , mType(IMediaResourceManagerService::INVALID_RESOURCE_TYPE)
  , mWaitingResource(false)
{
}

MediaResourceHandler::~MediaResourceHandler()
{
  cancelResource();
}

bool
MediaResourceHandler::IsWaitingResource()
{
  Mutex::Autolock al(mLock);
  return mWaitingResource;
}

bool
MediaResourceHandler::requestResource(IMediaResourceManagerService::ResourceType aType)
{
  Mutex::Autolock al(mLock);

  if (mClient != nullptr && mService != nullptr) {
    return false;
  }

  sp<MediaResourceManagerClient> client = new MediaResourceManagerClient(this);
  sp<IMediaResourceManagerService> service = client->getMediaResourceManagerService();

  if (service == nullptr) {
    return false;
  }

  if (service->requestMediaResource(client, (int)aType, true) != OK) {
    return false;
  }

  mClient = client;
  mService = service;
  mType = aType;
  mWaitingResource = true;

  return true;
}

void
MediaResourceHandler::cancelResource()
{
  Mutex::Autolock al(mLock);

  if (mClient != nullptr && mService != nullptr) {
    mService->cancelClient(mClient, (int)mType);
  }

  mWaitingResource = false;
  mClient = nullptr;
  mService = nullptr;
}

// Called on a Binder thread
void
MediaResourceHandler::statusChanged(int aEvent)
{
  sp<ResourceListener> listener;

  Mutex::Autolock autoLock(mLock);

  listener = mListener.promote();
  if (listener == nullptr) {
    return;
  }

  mWaitingResource = false;

  MediaResourceManagerClient::State state = (MediaResourceManagerClient::State)aEvent;
  if (state == MediaResourceManagerClient::CLIENT_STATE_RESOURCE_ASSIGNED) {
    listener->resourceReserved();
  } else {
    listener->resourceCanceled();
  }
}

} // namespace android
