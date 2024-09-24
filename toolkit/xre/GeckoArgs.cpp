/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoArgs.h"

namespace mozilla::geckoargs {

template <>
Maybe<UniqueFileHandle> CommandLineArg<UniqueFileHandle>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  MOZ_CRASH("not implemented yet");
}

template <>
void CommandLineArg<UniqueFileHandle>::PutCommon(const char* aName,
                                                 UniqueFileHandle aValue,
                                                 ChildProcessArgs& aArgs) {
  if (aValue) {
    aArgs.mFiles.push_back(std::move(aValue));
    MOZ_CRASH("not implemented yet");
  }
}

}  // namespace mozilla::geckoargs
