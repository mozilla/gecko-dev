/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SimpleURIUnknownSchemes_h__
#define SimpleURIUnknownSchemes_h__

#include "nsString.h"
#include "mozilla/RWLock.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

#define SIMPLE_URI_SCHEMES_PREF "network.url.simple_uri_unknown_schemes"

namespace mozilla::net {

class SimpleURIUnknownSchemes {
 public:
  SimpleURIUnknownSchemes() = default;

  // parse the list in the pref specified by SIMPLE_URI_SCHEMES_PREF
  // then merge them with the list obtained from remote settings
  // into a list to tell URL constructors of unknown schemes
  // to use our simpleURI parser
  void ParseAndMergePrefSchemes();

  // pass a remote-settings specified list of unknown schemes that should use
  // the simple uri parser
  // store a local copy of the list
  // and merge the list with the pref-specified list of unknown schemes
  void SetAndMergeRemoteSchemes(const nsTArray<nsCString>& remoteSettingsList);

  bool IsSimpleURIUnknownScheme(const nsACString& aScheme);
  void GetRemoteSchemes(nsTArray<nsCString>& aArray);

 private:
  void ParseAndMergePrefSchemesLocked() MOZ_REQUIRES(mSchemeLock);
  void MergeSimpleURISchemes(const nsTArray<nsCString>& prefList,
                             const nsTArray<nsCString>& remoteSettingsList)
      MOZ_REQUIRES(mSchemeLock);

  mutable RWLock mSchemeLock{"SimpleURIUnknownSchemes"};
  nsTHashSet<nsCString> mSimpleURISchemes MOZ_GUARDED_BY(mSchemeLock);

  // process-local copy of the remote settings schemes
  // keep them separate from pref-entered schemes so user cannot overwrite
  nsTArray<nsCString> mRemoteSettingsURISchemes MOZ_GUARDED_BY(mSchemeLock);
};

}  // namespace mozilla::net
#endif  // SimpleURIUnknownSchemes_h__
