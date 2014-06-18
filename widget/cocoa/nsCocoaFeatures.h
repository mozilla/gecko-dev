/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaFeatures_h_
#define nsCocoaFeatures_h_

#include <stdint.h>

class nsCocoaFeatures {
public:
  static int32_t OSXVersion();
  static int32_t OSXVersionMajor();
  static int32_t OSXVersionMinor();
  static int32_t OSXVersionBugFix();
  static bool OnLionOrLater();
  static bool OnMountainLionOrLater();
  static bool OnMavericksOrLater();
  static bool OnYosemiteOrLater();
  static bool SupportCoreAnimationPlugins();

private:
  static void InitializeVersionNumbers();

  static int32_t mOSXVersion;
  static int32_t mOSXVersionMajor;
  static int32_t mOSXVersionMinor;
  static int32_t mOSXVersionBugFix;
};
#endif // nsCocoaFeatures_h_
