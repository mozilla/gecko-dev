/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCConstructors.h"
#include "nsMacRomanToUnicode.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

nsresult
nsMacRomanToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult)
{
  static const uint16_t g_MacRomanMappingTable[] = {
#include "macroman.ut"
  };

  return CreateOneByteDecoder((uMappingTable*) &g_MacRomanMappingTable,
                            aOuter, aIID, aResult);
}

