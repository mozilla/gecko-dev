/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Base64_h__
#define mozilla_Base64_h__

#include "nsString.h"

class nsIInputStream;

namespace mozilla {

nsresult
Base64EncodeInputStream(nsIInputStream* aInputStream,
                        nsACString& aDest,
                        uint32_t aCount,
                        uint32_t aOffset = 0);
nsresult
Base64EncodeInputStream(nsIInputStream* aInputStream,
                        nsAString& aDest,
                        uint32_t aCount,
                        uint32_t aOffset = 0);

nsresult
Base64Encode(const nsACString& aString, nsACString& aBinary);
nsresult
Base64Encode(const nsAString& aString, nsAString& aBinaryData);

nsresult
Base64Decode(const nsACString& aBinaryData, nsACString& aString);
nsresult
Base64Decode(const nsAString& aBinaryData, nsAString& aString);

} // namespace mozilla

#endif
