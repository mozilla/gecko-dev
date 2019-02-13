/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScriptableRegion_h
#define nsScriptableRegion_h

#include "nsIScriptableRegion.h"
#include "gfxCore.h"
#include "nsISupports.h"
#include "nsRegion.h"
#include "mozilla/Attributes.h"

class NS_GFX nsScriptableRegion final : public nsIScriptableRegion {
public:
	nsScriptableRegion();

	NS_DECL_ISUPPORTS

	NS_DECL_NSISCRIPTABLEREGION

private:
        ~nsScriptableRegion() {}
	nsIntRegion mRegion;
};

#endif
