/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExternalTexture_H_
#define ExternalTexture_H_

#include "nsIGlobalObject.h"
#include "ObjectModel.h"

namespace mozilla::webgpu {

// NOTE: Incomplete. Follow-up to complete implementation is at
// <https://bugzilla.mozilla.org/show_bug.cgi?id=1827116>.
class ExternalTexture : public ObjectBase {
 public:
  GPU_DECL_CYCLE_COLLECTION(ExternalTexture)
  GPU_DECL_JS_WRAP(ExternalTexture)

  explicit ExternalTexture(nsIGlobalObject* const aGlobal) : mGlobal(aGlobal) {}

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  ~ExternalTexture() = default;
  void Cleanup() {}
};

}  // namespace mozilla::webgpu

#endif  // GPU_ExternalTexture_H_
