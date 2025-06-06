/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebIdentityChild_h
#define mozilla_dom_WebIdentityChild_h

#include "mozilla/dom/PWebIdentity.h"
#include "mozilla/dom/PWebIdentityChild.h"

namespace mozilla::dom {

class WebIdentityHandler;

class WebIdentityChild final : public PWebIdentityChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebIdentityChild, override);

  WebIdentityChild() = default;

  void ActorDestroy(ActorDestroyReason why) override;

  void SetHandler(WebIdentityHandler* aHandler);

 private:
  ~WebIdentityChild() = default;

  WebIdentityHandler* mHandler{};
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebIdentityChild_h
