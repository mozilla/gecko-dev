/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_urlclassiferipcutils_h
#define mozilla_urlclassiferipcutils_h

#include "ipc/EnumSerializer.h"
#include "nsIUrlClassifierFeature.h"

namespace IPC {

template <>
struct ParamTraits<nsIUrlClassifierFeature::listType>
    : public ContiguousEnumSerializerInclusive<
          nsIUrlClassifierFeature::listType,
          nsIUrlClassifierFeature::listType::blocklist,
          nsIUrlClassifierFeature::listType::entitylist> {};

}  // namespace IPC

#endif  // mozilla_urlclassiferipcutils_h
