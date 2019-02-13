/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginTags_h_
#define nsPluginTags_h_

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIPluginTag.h"
#include "nsITimer.h"
#include "nsString.h"

struct PRLibrary;
struct nsPluginInfo;
class nsNPAPIPlugin;

// A linked-list of plugin information that is used for instantiating plugins
// and reflecting plugin information into JavaScript.
class nsPluginTag final : public nsIPluginTag
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINTAG

  // These must match the STATE_* values in nsIPluginTag.idl
  enum PluginState {
    ePluginState_Disabled = 0,
    ePluginState_Clicktoplay = 1,
    ePluginState_Enabled = 2,
    ePluginState_MaxValue = 3,
  };

  nsPluginTag(nsPluginInfo* aPluginInfo,
              int64_t aLastModifiedTime,
              bool fromExtension);
  nsPluginTag(const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* aFullPath,
              const char* aVersion,
              const char* const* aMimeTypes,
              const char* const* aMimeDescriptions,
              const char* const* aExtensions,
              int32_t aVariants,
              int64_t aLastModifiedTime,
              bool fromExtension,
              bool aArgsAreUTF8 = false);
  nsPluginTag(uint32_t aId,
              const char* aName,
              const char* aDescription,
              const char* aFileName,
              const char* aFullPath,
              const char* aVersion,
              nsTArray<nsCString> aMimeTypes,
              nsTArray<nsCString> aMimeDescriptions,
              nsTArray<nsCString> aExtensions,
              bool aIsJavaPlugin,
              bool aIsFlashPlugin,
              int64_t aLastModifiedTime,
              bool aFromExtension);

  void TryUnloadPlugin(bool inShutdown);

  // plugin is enabled and not blocklisted
  bool IsActive();

  bool IsEnabled();
  void SetEnabled(bool enabled);
  bool IsClicktoplay();
  bool IsBlocklisted();

  PluginState GetPluginState();
  void SetPluginState(PluginState state);

  // import legacy flags from plugin registry into the preferences
  void ImportFlagsToPrefs(uint32_t flag);

  bool HasSameNameAndMimes(const nsPluginTag *aPluginTag) const;
  nsCString GetNiceFileName();

  bool IsFromExtension() const;

  nsRefPtr<nsPluginTag> mNext;
  uint32_t      mId;

  // Number of PluginModuleParents living in all content processes.
  size_t        mContentProcessRunningCount;

  // True if we've ever created an instance of this plugin in the current process.
  bool          mHadLocalInstance;

  nsCString     mName; // UTF-8
  nsCString     mDescription; // UTF-8
  nsTArray<nsCString> mMimeTypes; // UTF-8
  nsTArray<nsCString> mMimeDescriptions; // UTF-8
  nsTArray<nsCString> mExtensions; // UTF-8
  PRLibrary     *mLibrary;
  nsRefPtr<nsNPAPIPlugin> mPlugin;
  bool          mIsJavaPlugin;
  bool          mIsFlashPlugin;
  bool          mSupportsAsyncInit;
  nsCString     mFileName; // UTF-8
  nsCString     mFullPath; // UTF-8
  nsCString     mVersion;  // UTF-8
  int64_t       mLastModifiedTime;
  nsCOMPtr<nsITimer> mUnloadTimer;

  void          InvalidateBlocklistState();

  // Returns true if this plugin claims it supports this MIME type.  The
  // comparison is done ASCII-case-insensitively.
  bool          HasMimeType(const nsACString & aMimeType) const;
  // Returns true if this plugin claims it supports the given extension.  In hat
  // case, aMatchingType is set to the MIME type the plugin claims corresponds
  // to this extension.  Again, the extension is done ASCII-case-insensitively.
  bool          HasExtension(const nsACString & aExtension,
                             /* out */ nsACString & aMatchingType) const;

private:
  virtual ~nsPluginTag();

  nsCString     mNiceFileName; // UTF-8
  uint16_t      mCachedBlocklistState;
  bool          mCachedBlocklistStateValid;
  bool          mIsFromExtension;

  void InitMime(const char* const* aMimeTypes,
                const char* const* aMimeDescriptions,
                const char* const* aExtensions,
                uint32_t aVariantCount);
  nsresult EnsureMembersAreUTF8();
  void FixupVersion();

  static uint32_t sNextId;
};

#endif // nsPluginTags_h_
