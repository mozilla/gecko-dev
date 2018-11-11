/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ZoomConstraintsClient_h_
#define ZoomConstraintsClient_h_

#include "FrameMetrics.h"
#include "mozilla/Maybe.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsWeakPtr.h"

class nsIDOMEventTarget;
class nsIDocument;
class nsIPresShell;

class ZoomConstraintsClient final : public nsIDOMEventListener,
                                    public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER

  ZoomConstraintsClient();

private:
  ~ZoomConstraintsClient();

public:
  void Init(nsIPresShell* aPresShell, nsIDocument *aDocument);
  void Destroy();
  void ScreenSizeChanged();

private:
  void RefreshZoomConstraints();

  nsCOMPtr<nsIDocument> mDocument;
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell; // raw ref since the presShell owns this
  nsCOMPtr<nsIDOMEventTarget> mEventTarget;
  mozilla::Maybe<mozilla::layers::ScrollableLayerGuid> mGuid;
};

#endif

