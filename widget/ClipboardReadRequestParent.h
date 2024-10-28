/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ClipboardReadRequestParent_h
#define mozilla_ClipboardReadRequestParent_h

#include "mozilla/dom/ContentParent.h"
#include "mozilla/PClipboardReadRequestParent.h"
#include "nsIClipboard.h"

namespace mozilla {

class ClipboardReadRequestParent final : public PClipboardReadRequestParent {
  using IPCResult = mozilla::ipc::IPCResult;
  using ContentParent = mozilla::dom::ContentParent;

 public:
  ClipboardReadRequestParent(ContentParent* aManager,
                             nsIClipboardDataSnapshot* aClipboardDataSnapshot)
      : mManager(aManager), mClipboardDataSnapshot(aClipboardDataSnapshot) {}

  NS_INLINE_DECL_REFCOUNTING(ClipboardReadRequestParent, override)

  // PClipboardReadRequestParent
  IPCResult RecvGetData(const nsTArray<nsCString>& aFlavors,
                        GetDataResolver&& aResolver);
  IPCResult RecvGetDataSync(
      const nsTArray<nsCString>& aFlavors,
      dom::IPCTransferableDataOrError* aTransferableDataOrError);

 private:
  ~ClipboardReadRequestParent() = default;

  RefPtr<ContentParent> mManager;
  nsCOMPtr<nsIClipboardDataSnapshot> mClipboardDataSnapshot;
};

}  // namespace mozilla

#endif  // mozilla_ClipboardReadRequestParent_h
