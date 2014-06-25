/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsIDOMScriptObjectFactory.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"

class nsDOMScriptObjectFactory MOZ_FINAL : public nsIDOMScriptObjectFactory,
                                           public nsIObserver
{
  ~nsDOMScriptObjectFactory() {}

public:
  nsDOMScriptObjectFactory();

  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER

  // nsIDOMScriptObjectFactory
  NS_IMETHOD_(nsISupports *) GetClassInfoInstance(nsDOMClassInfoID aID) MOZ_OVERRIDE;
  NS_IMETHOD_(nsISupports *) GetExternalClassInfoInstance(const nsAString& aName) MOZ_OVERRIDE;

  NS_IMETHOD RegisterDOMClassInfo(const char *aName,
                                  nsDOMClassInfoExternalConstructorFnc aConstructorFptr,
                                  const nsIID *aProtoChainInterface,
                                  const nsIID **aInterfaces,
                                  uint32_t aScriptableFlags,
                                  bool aHasClassInterface,
                                  const nsCID *aConstructorCID) MOZ_OVERRIDE;
};

