/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISPARENT_H_
#define MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISPARENT_H_

#include "mozilla/dom/ContentParent.h"
#include "mozilla/PClipboardContentAnalysisParent.h"
#include "nsIClipboard.h"

namespace mozilla {

class ClipboardContentAnalysisParent final
    : public PClipboardContentAnalysisParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(ClipboardContentAnalysisParent, override)
  explicit ClipboardContentAnalysisParent(
      RefPtr<dom::ThreadsafeContentParentHandle>&&
          aThreadsafeContentParentHandle)
      : mThreadsafeContentParentHandle(
            std::move(aThreadsafeContentParentHandle)) {}

 private:
  ~ClipboardContentAnalysisParent() = default;
  RefPtr<dom::ThreadsafeContentParentHandle> mThreadsafeContentParentHandle;
  ipc::IPCResult GetSomeClipboardData(
      nsTArray<nsCString>&& aTypes,
      const nsIClipboard::ClipboardType& aWhichClipboard,
      const uint64_t& aRequestingWindowContextId, bool aCheckAllContent,
      IPCTransferableDataOrError* aTransferableDataOrError);

 public:
  ipc::IPCResult RecvGetClipboard(
      nsTArray<nsCString>&& aTypes,
      const nsIClipboard::ClipboardType& aWhichClipboard,
      const uint64_t& aRequestingWindowContextId,
      IPCTransferableDataOrError* aTransferableDataOrError);
  ipc::IPCResult RecvGetAllClipboardDataSync(
      nsTArray<nsCString>&& aTypes,
      const nsIClipboard::ClipboardType& aWhichClipboard,
      const uint64_t& aRequestingWindowContextId,
      IPCTransferableDataOrError* aTransferableDataOrError);
};
}  // namespace mozilla

#endif  // MOZILLA_WIDGET_CLIPBOARDCONTENTANALYSISPARENT_H_
