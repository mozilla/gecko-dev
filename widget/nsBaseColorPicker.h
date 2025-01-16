/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseColorPicker_h_
#define nsBaseColorPicker_h_

#include "nsCOMPtr.h"
#include "nsIColorPicker.h"
#include "nsString.h"

class mozIDOMWindowProxy;

namespace mozilla::dom {
class BrowsingContext;
}  // namespace mozilla::dom

class nsBaseColorPicker : public nsIColorPicker {
 public:
  // nsIColorPicker
  NS_IMETHOD Init(mozilla::dom::BrowsingContext* aBrowsingContext,
                  const nsAString& aTitle, const nsAString& aInitialColor,
                  const nsTArray<nsString>& aDefaultColors) override final;
  NS_IMETHOD Open(nsIColorPickerShownCallback* aCallback) override final;

 protected:
  virtual ~nsBaseColorPicker() = default;

  virtual nsresult InitNative(const nsTArray<nsString>& aDefaultColors) = 0;
  virtual nsresult OpenNative() = 0;

  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;
  nsString mTitle;
  nsString mInitialColor;
  nsCOMPtr<nsIColorPickerShownCallback> mCallback;
};

#endif  // nsBaseColorPicker_h_
