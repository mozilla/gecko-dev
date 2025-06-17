/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_urlclassiferipcutils_h
#define mozilla_urlclassiferipcutils_h

#include "ipc/EnumSerializer.h"
#include "nsIUrlClassifierFeature.h"
#include "nsIClassifiedChannel.h"

namespace IPC {

template <>
struct ParamTraits<nsIUrlClassifierFeature::listType>
    : public ContiguousEnumSerializerInclusive<
          nsIUrlClassifierFeature::listType,
          nsIUrlClassifierFeature::listType::blocklist,
          nsIUrlClassifierFeature::listType::entitylist> {};

template <>
struct ParamTraits<mozilla::net::ClassificationFlags> {
  static void Write(MessageWriter* aWriter,
                    const mozilla::net::ClassificationFlags& aParam) {
    WriteParam(aWriter, aParam.firstPartyFlags);
    WriteParam(aWriter, aParam.thirdPartyFlags);
  }

  static bool Read(MessageReader* aReader,
                   mozilla::net::ClassificationFlags* aResult) {
    uint32_t firstPartyFlags;
    uint32_t thirdPartyFlags;
    if (!ReadParam(aReader, &firstPartyFlags) ||
        !ReadParam(aReader, &thirdPartyFlags)) {
      return false;
    }
    aResult->firstPartyFlags = firstPartyFlags;
    aResult->thirdPartyFlags = thirdPartyFlags;
    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_urlclassiferipcutils_h
