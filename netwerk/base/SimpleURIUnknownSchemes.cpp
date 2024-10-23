/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleURIUnknownSchemes.h"
#include "mozilla/StaticPrefs_network.h"

static mozilla::LazyLogModule gURLLog("URL");

namespace mozilla::net {

nsTArray<nsCString> ParseUriSchemes(const nsCString& inputStrList) {
  nsTArray<nsCString> result;
  for (const auto& scheme : inputStrList.Split(',')) {
    nsCString* str = result.AppendElement(scheme);
    str->StripWhitespace();
  }
  return result;
}

void SimpleURIUnknownSchemes::ParseAndMergePrefSchemes() {
  AutoWriteLock lock(mSchemeLock);
  ParseAndMergePrefSchemesLocked();
}

void SimpleURIUnknownSchemes::ParseAndMergePrefSchemesLocked() {
  nsAutoCString prefListStr;
  Preferences::GetCString(SIMPLE_URI_SCHEMES_PREF, prefListStr);
  nsTArray<nsCString> prefSchemes = ParseUriSchemes(prefListStr);
  MergeSimpleURISchemes(prefSchemes, mRemoteSettingsURISchemes);
}

void SimpleURIUnknownSchemes::SetAndMergeRemoteSchemes(
    const nsTArray<nsCString>& remoteSettingsList) {
  MOZ_LOG(gURLLog, LogLevel::Debug,
          ("SimpleURIUnknownSchemes::SetAndMergeRemoteSchemes()"));
  AutoWriteLock lock(mSchemeLock);

  // update the local copy of remote settings schemes in case of pref-update
  mRemoteSettingsURISchemes = remoteSettingsList.Clone();

  // update the merged list with the new remote settings schemes
  ParseAndMergePrefSchemesLocked();
}

void SimpleURIUnknownSchemes::MergeSimpleURISchemes(
    const nsTArray<nsCString>& prefList,
    const nsTArray<nsCString>& remoteSettingsList) {
  mSimpleURISchemes.Clear();
  for (const nsCString& scheme : prefList) {
    mSimpleURISchemes.Insert(scheme);
  }
  for (const nsCString& scheme : remoteSettingsList) {
    mSimpleURISchemes.Insert(scheme);
  }
}

bool SimpleURIUnknownSchemes::IsSimpleURIUnknownScheme(
    const nsACString& aScheme) {
  AutoReadLock lock(mSchemeLock);
  return mSimpleURISchemes.Contains(aScheme);
}

void SimpleURIUnknownSchemes::GetRemoteSchemes(nsTArray<nsCString>& aArray) {
  aArray.Clear();
  AutoReadLock lock(mSchemeLock);
  for (const auto& uri : mRemoteSettingsURISchemes) {
    aArray.AppendElement(uri);
  }
}

}  // namespace mozilla::net
