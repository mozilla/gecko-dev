/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoWindowStateHelper_h
#define __nsAutoWindowStateHelper_h

#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"

/**
 * Helper class for dealing with notifications around opening modal
 * windows.
 */

class nsPIDOMWindow;

class nsAutoWindowStateHelper
{
public:
  nsAutoWindowStateHelper(nsPIDOMWindow *aWindow);
  ~nsAutoWindowStateHelper();

  bool DefaultEnabled()
  {
    return mDefaultEnabled;
  }

protected:
  bool DispatchEventToChrome(const char *aEventName);

  nsCOMPtr<nsPIDOMWindow> mWindow;
  bool mDefaultEnabled;
};


#endif
