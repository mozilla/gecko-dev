/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ZoomConstraintsClient.h"

#include <inttypes.h>
#include "FrameMetrics.h"
#include "LayersLogging.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "nsDocument.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsPoint.h"
#include "nsPresShell.h"
#include "nsView.h"
#include "nsViewportInfo.h"
#include "Units.h"
#include "UnitTransforms.h"

#define ZCC_LOG(...)
// #define ZCC_LOG(...) printf_stderr("ZCC: " __VA_ARGS__)

NS_IMPL_ISUPPORTS(ZoomConstraintsClient, nsIDOMEventListener, nsIObserver)

static const nsLiteralString DOM_META_ADDED = NS_LITERAL_STRING("DOMMetaAdded");
static const nsLiteralCString BEFORE_FIRST_PAINT = NS_LITERAL_CSTRING("before-first-paint");

using namespace mozilla;
using namespace mozilla::layers;

ZoomConstraintsClient::ZoomConstraintsClient() :
  mDocument(nullptr),
  mPresShell(nullptr)
{
}

ZoomConstraintsClient::~ZoomConstraintsClient()
{
}

static nsIWidget*
GetWidget(nsIPresShell* aShell)
{
  if (nsIFrame* rootFrame = aShell->GetRootFrame()) {
#ifdef MOZ_WIDGET_ANDROID
    return rootFrame->GetNearestWidget();
#else
    if (nsView* view = rootFrame->GetView()) {
      return view->GetWidget();
    }
#endif
  }
  return nullptr;
}

void
ZoomConstraintsClient::Destroy()
{
  if (!(mPresShell && mDocument)) {
    return;
  }

  ZCC_LOG("Destroying %p\n", this);

  if (mEventTarget) {
    mEventTarget->RemoveEventListener(DOM_META_ADDED, this, false);
    mEventTarget = nullptr;
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, BEFORE_FIRST_PAINT.Data());
  }

  if (mGuid) {
    if (nsIWidget* widget = GetWidget(mPresShell)) {
      ZCC_LOG("Sending null constraints in %p for { %u, %" PRIu64 " }\n",
        this, mGuid->mPresShellId, mGuid->mScrollId);
      widget->UpdateZoomConstraints(mGuid->mPresShellId, mGuid->mScrollId, Nothing());
      mGuid = Nothing();
    }
  }

  mDocument = nullptr;
  mPresShell = nullptr;
}

void
ZoomConstraintsClient::Init(nsIPresShell* aPresShell, nsIDocument* aDocument)
{
  if (!(aPresShell && aDocument)) {
    return;
  }

  mPresShell = aPresShell;
  mDocument = aDocument;

  if (nsCOMPtr<nsPIDOMWindow> window = mDocument->GetWindow()) {
    mEventTarget = window->GetChromeEventHandler();
  }
  if (mEventTarget) {
    mEventTarget->AddEventListener(DOM_META_ADDED, this, false);
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, BEFORE_FIRST_PAINT.Data(), false);
  }
}

NS_IMETHODIMP
ZoomConstraintsClient::HandleEvent(nsIDOMEvent* event)
{
  nsAutoString type;
  event->GetType(type);

  if (type.Equals(DOM_META_ADDED)) {
    ZCC_LOG("Got a dom-meta-added event in %p\n", this);
    RefreshZoomConstraints();
  }
  return NS_OK;
}

NS_IMETHODIMP
ZoomConstraintsClient::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (SameCOMIdentity(aSubject, mDocument) && BEFORE_FIRST_PAINT.EqualsASCII(aTopic)) {
    ZCC_LOG("Got a before-first-paint event in %p\n", this);
    RefreshZoomConstraints();
  }
  return NS_OK;
}

mozilla::layers::ZoomConstraints
ComputeZoomConstraintsFromViewportInfo(const nsViewportInfo& aViewportInfo)
{
  mozilla::layers::ZoomConstraints constraints;
  constraints.mAllowZoom = aViewportInfo.IsZoomAllowed();
  constraints.mAllowDoubleTapZoom = aViewportInfo.IsDoubleTapZoomAllowed();
  constraints.mMinZoom.scale = aViewportInfo.GetMinZoom().scale;
  constraints.mMaxZoom.scale = aViewportInfo.GetMaxZoom().scale;
  return constraints;
}

void
ZoomConstraintsClient::RefreshZoomConstraints()
{
  nsIWidget* widget = GetWidget(mPresShell);
  if (!widget) {
    return;
  }

  uint32_t presShellId = 0;
  FrameMetrics::ViewID viewId = FrameMetrics::NULL_SCROLL_ID;
  bool scrollIdentifiersValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
        mDocument->GetDocumentElement(),
        &presShellId, &viewId);
  if (!scrollIdentifiersValid) {
    return;
  }

  nsIFrame* rootFrame = mPresShell->GetRootScrollFrame();
  if (!rootFrame) {
    rootFrame = mPresShell->GetRootFrame();
  }
  nsSize size = nsLayoutUtils::CalculateCompositionSizeForFrame(rootFrame, false);
  int32_t auPerDevPixel = mPresShell->GetPresContext()->AppUnitsPerDevPixel();
  LayoutDeviceIntSize screenSize = LayoutDeviceIntSize::FromAppUnitsRounded(
        size, auPerDevPixel);

  nsViewportInfo viewportInfo = nsContentUtils::GetViewportInfo(
    mDocument,
    ViewAs<ScreenPixel>(screenSize, PixelCastJustification::LayoutDeviceIsScreenForBounds));

  mozilla::layers::ZoomConstraints zoomConstraints =
    ComputeZoomConstraintsFromViewportInfo(viewportInfo);

  if (zoomConstraints.mAllowDoubleTapZoom) {
    // If the CSS viewport is narrower than the screen (i.e. width <= device-width)
    // then we disable double-tap-to-zoom behaviour.
    CSSToLayoutDeviceScale scale(
      (float)nsPresContext::AppUnitsPerCSSPixel() / auPerDevPixel);
    if ((viewportInfo.GetSize() * scale).width <= screenSize.width) {
      zoomConstraints.mAllowDoubleTapZoom = false;
    }
  }

  ScrollableLayerGuid newGuid(0, presShellId, viewId);
  if (mGuid && mGuid.value() != newGuid) {
    ZCC_LOG("Clearing old constraints in %p for { %u, %" PRIu64 " }\n",
      this, mGuid->mPresShellId, mGuid->mScrollId);
    // If the guid changes, send a message to clear the old one
    widget->UpdateZoomConstraints(mGuid->mPresShellId, mGuid->mScrollId, Nothing());
  }
  mGuid = Some(newGuid);
  ZCC_LOG("Sending constraints %s in %p for { %u, %" PRIu64 " }\n",
    Stringify(zoomConstraints).c_str(), this, presShellId, viewId);
  widget->UpdateZoomConstraints(presShellId, viewId, Some(zoomConstraints));
}
