/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDragServiceProxy.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"

using mozilla::CSSIntRegion;
using mozilla::LayoutDeviceIntRect;
using mozilla::Maybe;
using mozilla::dom::OptionalShmem;
using mozilla::dom::TabChild;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SurfaceFormat;
using mozilla::ipc::Shmem;

nsDragServiceProxy::nsDragServiceProxy() {}

nsDragServiceProxy::~nsDragServiceProxy() {}

static void GetPrincipalURIFromNode(nsCOMPtr<nsINode>& sourceNode,
                                    nsCString& aPrincipalURISpec) {
  if (!sourceNode) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = sourceNode->NodePrincipal();
  nsCOMPtr<nsIURI> principalURI;
  nsresult rv = principal->GetURI(getter_AddRefs(principalURI));
  if (NS_FAILED(rv) || !principalURI) {
    return;
  }

  principalURI->GetSpec(aPrincipalURISpec);
}

nsresult nsDragServiceProxy::InvokeDragSessionImpl(
    nsIArray* aArrayTransferables, const Maybe<CSSIntRegion>& aRegion,
    uint32_t aActionType) {
  NS_ENSURE_STATE(mSourceDocument->GetDocShell());
  TabChild* child = TabChild::GetFrom(mSourceDocument->GetDocShell());
  NS_ENSURE_STATE(child);
  nsTArray<mozilla::dom::IPCDataTransfer> dataTransfers;
  nsContentUtils::TransferablesToIPCTransferables(
      aArrayTransferables, dataTransfers, false, child->Manager(), nullptr);

  nsCString principalURISpec;
  GetPrincipalURIFromNode(mSourceNode, principalURISpec);

  LayoutDeviceIntRect dragRect;
  if (mHasImage || mSelection) {
    nsPresContext* pc;
    RefPtr<SourceSurface> surface;
    DrawDrag(mSourceNode, aRegion, mScreenPosition, &dragRect, &surface, &pc);

    if (surface) {
      RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
      if (dataSurface) {
        size_t length;
        int32_t stride;
        Maybe<Shmem> maybeShm = nsContentUtils::GetSurfaceData(
            dataSurface, &length, &stride, child);
        if (maybeShm.isNothing()) {
          return NS_ERROR_FAILURE;
        }

        auto surfaceData = maybeShm.value();

        // Save the surface data to shared memory.
        if (!surfaceData.IsReadable() || !surfaceData.get<char>()) {
          NS_WARNING("Failed to create shared memory for drag session.");
          return NS_ERROR_FAILURE;
        }

        mozilla::Unused << child->SendInvokeDragSession(
            dataTransfers, aActionType, surfaceData, stride,
            dataSurface->GetFormat(), dragRect, principalURISpec);
        StartDragSession();
        return NS_OK;
      }
    }
  }

  mozilla::Unused << child->SendInvokeDragSession(
      dataTransfers, aActionType, mozilla::void_t(), 0,
      static_cast<SurfaceFormat>(0), dragRect, principalURISpec);
  StartDragSession();
  return NS_OK;
}
