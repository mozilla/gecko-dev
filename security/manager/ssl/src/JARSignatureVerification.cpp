/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateDB.h"

// This functionality was disabled on mozilla-aurora because the implementation
// used an NSS function that will not be part of the NSS release that Aurora
// will ship with.
NS_IMETHODIMP
nsNSSCertificateDB::OpenSignedJARFileAsync(
  nsIFile * aJarFile, nsIOpenSignedJARFileCallback * aCallback)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
