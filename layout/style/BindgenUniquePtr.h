/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BindgenUniquePtr_h
#define mozilla_BindgenUniquePtr_h

#include <memory>
#include "mozilla/UniquePtr.h"

namespace mozilla {

template <typename T>
class DefaultDelete;

/// <div rustbindgen="true" replaces="mozilla::BindgenUniquePtr">
template <typename T>
struct BindgenUniquePtr_Simple {
  T* mPtr;
};

template <typename T>
class BindgenUniquePtr final : public std::unique_ptr<T, DefaultDelete<T>> {
  static_assert(alignof(std::unique_ptr<T, DefaultDelete<T>>) ==
                alignof(BindgenUniquePtr_Simple<T>));
  static_assert(sizeof(std::unique_ptr<T, DefaultDelete<T>>) ==
                sizeof(BindgenUniquePtr_Simple<T>));
};

}  // namespace mozilla

#endif
