/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MLSTypeUtils_h
#define mozilla_dom_MLSTypeUtils_h

#include "mozilla/dom/MLSBinding.h"
#include "nsTArray.h"

namespace mozilla::dom {

nsTArray<uint8_t> ExtractMLSBytesOrUint8ArrayWithUnknownType(
    const MLSBytesOrUint8Array& aArgument, ErrorResult& aRv);

nsTArray<uint8_t> ExtractMLSBytesOrUint8Array(
    MLSObjectType aExpectedType, const MLSBytesOrUint8Array& aArgument,
    ErrorResult& aRv);

nsTArray<uint8_t> ExtractMLSBytesOrUint8ArrayOrUTF8String(
    MLSObjectType aExpectedType,
    const MLSBytesOrUint8ArrayOrUTF8String& aArgument, ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // mozilla_dom_MLSTypeUtils_h
