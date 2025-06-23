/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CLIENTUSAGEARAY_H_
#define DOM_QUOTA_CLIENTUSAGEARAY_H_

#include <cstdint>
#include "mozilla/Maybe.h"
#include "mozilla/dom/quota/Client.h"

namespace mozilla::dom::quota {

class ClientUsageArray final : public Array<Maybe<uint64_t>, Client::TYPE_MAX> {
 public:
  Maybe<uint64_t>& operator[](size_t aIndex) {
    if (MOZ_UNLIKELY(aIndex >= Client::TypeMax())) {
      MOZ_CRASH("indexing into invalid element");
    }
    return Array::operator[](aIndex);
  }

  const Maybe<uint64_t>& operator[](size_t aIndex) const {
    if (MOZ_UNLIKELY(aIndex >= Client::TypeMax())) {
      MOZ_CRASH("indexing into invalid element");
    }
    return Array::operator[](aIndex);
  }

  constexpr size_t Length() const { return size(); }

  void Serialize(nsACString& aText) const;

  nsresult Deserialize(const nsACString& aText);
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CLIENTUSAGEARAY_H_
