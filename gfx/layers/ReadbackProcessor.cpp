/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReadbackProcessor.h"
#include <sys/types.h>                  // for int32_t
#include "Layers.h"                     // for Layer, ThebesLayer, etc
#include "ReadbackLayer.h"              // for ReadbackLayer, ReadbackSink
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxContext.h"                 // for gfxContext
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "nsAutoPtr.h"                  // for nsRefPtr, nsAutoPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxContext::Release, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"                   // for nsIntRegion
#include "nsSize.h"                     // for nsIntSize

namespace mozilla {
namespace layers {

void
ReadbackProcessor::BuildUpdates(ContainerLayer* aContainer)
{
  NS_ASSERTION(mAllUpdates.IsEmpty(), "Some updates not processed?");

  if (!aContainer->mMayHaveReadbackChild)
    return;

  aContainer->mMayHaveReadbackChild = false;
  // go backwards so the updates read from earlier layers are later in the
  // array.
  for (Layer* l = aContainer->GetLastChild(); l; l = l->GetPrevSibling()) {
    if (l->GetType() == Layer::TYPE_READBACK) {
      aContainer->mMayHaveReadbackChild = true;
      BuildUpdatesForLayer(static_cast<ReadbackLayer*>(l));
    }
  }
}

static Layer*
FindBackgroundLayer(ReadbackLayer* aLayer, nsIntPoint* aOffset)
{
  gfx::Matrix transform;
  if (!aLayer->GetTransform().Is2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return nullptr;
  nsIntPoint transformOffset(int32_t(transform._31), int32_t(transform._32));

  for (Layer* l = aLayer->GetPrevSibling(); l; l = l->GetPrevSibling()) {
    gfx::Matrix backgroundTransform;
    if (!l->GetTransform().Is2D(&backgroundTransform) ||
        gfx::ThebesMatrix(backgroundTransform).HasNonIntegerTranslation())
      return nullptr;

    nsIntPoint backgroundOffset(int32_t(backgroundTransform._31), int32_t(backgroundTransform._32));
    nsIntRect rectInBackground(transformOffset - backgroundOffset, aLayer->GetSize());
    const nsIntRegion& visibleRegion = l->GetEffectiveVisibleRegion();
    if (!visibleRegion.Intersects(rectInBackground))
      continue;
    // Since l is present in the background, from here on we either choose l
    // or nothing.
    if (!visibleRegion.Contains(rectInBackground))
      return nullptr;

    if (l->GetEffectiveOpacity() != 1.0 ||
        l->GetMaskLayer() ||
        !(l->GetContentFlags() & Layer::CONTENT_OPAQUE))
      return nullptr;

    // cliprects are post-transform
    const nsIntRect* clipRect = l->GetEffectiveClipRect();
    if (clipRect && !clipRect->Contains(nsIntRect(transformOffset, aLayer->GetSize())))
      return nullptr;

    Layer::LayerType type = l->GetType();
    if (type != Layer::TYPE_COLOR && type != Layer::TYPE_THEBES)
      return nullptr;

    *aOffset = backgroundOffset - transformOffset;
    return l;
  }

  return nullptr;
}

void
ReadbackProcessor::BuildUpdatesForLayer(ReadbackLayer* aLayer)
{
  if (!aLayer->mSink)
    return;

  nsIntPoint offset;
  Layer* newBackground = FindBackgroundLayer(aLayer, &offset);
  if (!newBackground) {
    aLayer->SetUnknown();
    return;
  }

  if (newBackground->GetType() == Layer::TYPE_COLOR) {
    ColorLayer* colorLayer = static_cast<ColorLayer*>(newBackground);
    if (aLayer->mBackgroundColor != colorLayer->GetColor()) {
      aLayer->mBackgroundLayer = nullptr;
      aLayer->mBackgroundColor = colorLayer->GetColor();
      NS_ASSERTION(aLayer->mBackgroundColor.a == 1.0,
                   "Color layer said it was opaque!");
      nsRefPtr<gfxContext> ctx =
          aLayer->mSink->BeginUpdate(aLayer->GetRect(),
                                     aLayer->AllocateSequenceNumber());
      if (ctx) {
        ctx->SetColor(aLayer->mBackgroundColor);
        nsIntSize size = aLayer->GetSize();
        ctx->Rectangle(gfxRect(0, 0, size.width, size.height));
        ctx->Fill();
        aLayer->mSink->EndUpdate(ctx, aLayer->GetRect());
      }
    }
  } else {
    NS_ASSERTION(newBackground->AsThebesLayer(), "Must be ThebesLayer");
    ThebesLayer* thebesLayer = static_cast<ThebesLayer*>(newBackground);
    // updateRect is relative to the ThebesLayer
    nsIntRect updateRect = aLayer->GetRect() - offset;
    if (thebesLayer != aLayer->mBackgroundLayer ||
        offset != aLayer->mBackgroundLayerOffset) {
      aLayer->mBackgroundLayer = thebesLayer;
      aLayer->mBackgroundLayerOffset = offset;
      aLayer->mBackgroundColor = gfxRGBA(0,0,0,0);
      thebesLayer->SetUsedForReadback(true);
    } else {
      nsIntRegion invalid;
      invalid.Sub(updateRect, thebesLayer->GetValidRegion());
      updateRect = invalid.GetBounds();
    }

    Update update = { aLayer, updateRect, aLayer->AllocateSequenceNumber() };
    mAllUpdates.AppendElement(update);
  }
}

void
ReadbackProcessor::GetThebesLayerUpdates(ThebesLayer* aLayer,
                                         nsTArray<Update>* aUpdates,
                                         nsIntRegion* aUpdateRegion)
{
  // All ThebesLayers used for readback are in mAllUpdates (some possibly
  // with an empty update rect).
  aLayer->SetUsedForReadback(false);
  if (aUpdateRegion) {
    aUpdateRegion->SetEmpty();
  }
  for (uint32_t i = mAllUpdates.Length(); i > 0; --i) {
    const Update& update = mAllUpdates[i - 1];
    if (update.mLayer->mBackgroundLayer == aLayer) {
      aLayer->SetUsedForReadback(true);
      // Don't bother asking for updates if we have an empty update rect.
      if (!update.mUpdateRect.IsEmpty()) {
        aUpdates->AppendElement(update);
        if (aUpdateRegion) {
          aUpdateRegion->Or(*aUpdateRegion, update.mUpdateRect);
        }
      }
      mAllUpdates.RemoveElementAt(i - 1);
    }
  }
}

ReadbackProcessor::~ReadbackProcessor()
{
  for (uint32_t i = mAllUpdates.Length(); i > 0; --i) {
    const Update& update = mAllUpdates[i - 1];
    // Unprocessed update. Notify the readback sink that this content is
    // unknown.
    update.mLayer->SetUnknown();
  }
}

}
}
