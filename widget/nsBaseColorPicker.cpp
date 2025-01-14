/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBaseColorPicker.h"

NS_IMETHODIMP
nsBaseColorPicker::Init(mozilla::dom::BrowsingContext* aBrowsingContext,
                        const nsAString& aTitle, const nsAString& aInitialColor,
                        const nsTArray<nsString>& aDefaultColors) {
  MOZ_ASSERT(
      aBrowsingContext,
      "Null browsingContext passed to colorpicker, no color picker for you!");

  mBrowsingContext = aBrowsingContext;
  mTitle = aTitle;
  mInitialColor = aInitialColor;
  return InitNative(aDefaultColors);
}

NS_IMETHODIMP
nsBaseColorPicker::Open(nsIColorPickerShownCallback* aCallback) {
  MOZ_ASSERT(aCallback);

  mCallback = aCallback;
  return OpenNative();
}
