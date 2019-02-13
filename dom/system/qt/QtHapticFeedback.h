/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIHapticFeedback.h"

class QtHapticFeedback : public nsIHapticFeedback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHAPTICFEEDBACK
protected:
  virtual ~QtHapticFeedback() {}
};
