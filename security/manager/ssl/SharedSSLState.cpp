/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSSLState.h"
#include "nsClientAuthRemember.h"
#include "nsComponentManagerUtils.h"
#include "nsICertOverrideService.h"
#include "mozilla/OriginAttributes.h"
#include "nsNSSComponent.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"
#include "nsServiceManagerUtils.h"
#include "PSMRunnable.h"
#include "PublicSSL.h"
#include "ssl.h"
#include "nsNetCID.h"

namespace mozilla {

namespace psm {

namespace {
SharedSSLState* gPublicState;
SharedSSLState* gPrivateState;
}  // namespace

SharedSSLState::SharedSSLState(PublicOrPrivate aPublicOrPrivate,
                               uint32_t aTlsFlags)
    : mIOLayerHelpers(new nsSSLIOLayerHelpers(aPublicOrPrivate, aTlsFlags)) {
  mIOLayerHelpers->Init();
}

SharedSSLState::~SharedSSLState() = default;

/*static*/
void SharedSSLState::GlobalInit() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");
  gPublicState = new SharedSSLState(PublicOrPrivate::Public);
  gPrivateState = new SharedSSLState(PublicOrPrivate::Private);
}

/*static*/
void SharedSSLState::GlobalCleanup() {
  MOZ_ASSERT(NS_IsMainThread(), "Not on main thread");

  if (gPrivateState) {
    gPrivateState->Cleanup();
    delete gPrivateState;
    gPrivateState = nullptr;
  }

  if (gPublicState) {
    gPublicState->Cleanup();
    delete gPublicState;
    gPublicState = nullptr;
  }
}

void SharedSSLState::Cleanup() { mIOLayerHelpers->Cleanup(); }

SharedSSLState* PublicSSLState() { return gPublicState; }

SharedSSLState* PrivateSSLState() { return gPrivateState; }

}  // namespace psm
}  // namespace mozilla
