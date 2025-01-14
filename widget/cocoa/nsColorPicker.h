/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColorPicker_h_
#define nsColorPicker_h_

#include "nsBaseColorPicker.h"
#include "nsString.h"
#include "nsCOMPtr.h"

class nsIColorPickerShownCallback;
class mozIDOMWindowProxy;
@class NSColorPanelWrapper;
@class NSColor;

class nsColorPicker final : public nsBaseColorPicker {
 public:
  NS_DECL_ISUPPORTS

  // For NSColorPanelWrapper.
  void Update(NSColor* aColor);
  void Done();

 private:
  ~nsColorPicker();

  // nsBaseColorPicker
  nsresult InitNative(const nsTArray<nsString>& aDefaultColors) override;
  nsresult OpenNative() override;

  static NSColor* GetNSColorFromHexString(const nsAString& aColor);
  static void GetHexStringFromNSColor(NSColor* aColor, nsAString& aResult);

  NSColorPanelWrapper* mColorPanelWrapper;
};

#endif  // nsColorPicker_h_
