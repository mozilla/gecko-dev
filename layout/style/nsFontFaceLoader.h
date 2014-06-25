/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for loading in @font-face defined font data */

#ifndef nsFontFaceLoader_h_
#define nsFontFaceLoader_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIStreamLoader.h"
#include "nsIChannel.h"
#include "gfxUserFontSet.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsCSSRules.h"

class nsPresContext;
class nsIPrincipal;

class nsFontFaceLoader;

// nsUserFontSet - defines the loading mechanism for downloadable fonts
class nsUserFontSet : public gfxUserFontSet
{
public:
  nsUserFontSet(nsPresContext* aContext);

  // Called when this font set is no longer associated with a presentation.
  void Destroy();

  // starts loading process, creating and initializing a nsFontFaceLoader obj
  // returns whether load process successfully started or not
  nsresult StartLoad(gfxMixedFontFamily* aFamily,
                     gfxProxyFontEntry* aFontToLoad,
                     const gfxFontFaceSrc* aFontFaceSrc) MOZ_OVERRIDE;

  // Called by nsFontFaceLoader when the loader has completed normally.
  // It's removed from the mLoaders set.
  void RemoveLoader(nsFontFaceLoader* aLoader);

  bool UpdateRules(const nsTArray<nsFontFaceRuleContainer>& aRules);

  nsPresContext* GetPresContext() { return mPresContext; }

  virtual void ReplaceFontEntry(gfxMixedFontFamily* aFamily,
                                gfxProxyFontEntry* aProxy,
                                gfxFontEntry* aFontEntry) MOZ_OVERRIDE;

  nsCSSFontFaceRule* FindRuleForEntry(gfxFontEntry* aFontEntry);

protected:
  // Protected destructor, to discourage deletion outside of Release()
  // (since we inherit from refcounted class gfxUserFontSet):
  ~nsUserFontSet();

  // The font-set keeps track of the collection of rules, and their
  // corresponding font entries (whether proxies or real entries),
  // so that we can update the set without having to throw away
  // all the existing fonts.
  struct FontFaceRuleRecord {
    nsRefPtr<gfxFontEntry>       mFontEntry;
    nsFontFaceRuleContainer      mContainer;
  };

  void InsertRule(nsCSSFontFaceRule* aRule, uint8_t aSheetType,
                  nsTArray<FontFaceRuleRecord>& oldRules,
                  bool& aFontSetModified);

  virtual nsresult LogMessage(gfxMixedFontFamily* aFamily,
                              gfxProxyFontEntry* aProxy,
                              const char* aMessage,
                              uint32_t aFlags = nsIScriptError::errorFlag,
                              nsresult aStatus = NS_OK) MOZ_OVERRIDE;

  virtual nsresult CheckFontLoad(const gfxFontFaceSrc* aFontFaceSrc,
                                 nsIPrincipal** aPrincipal,
                                 bool* aBypassCache) MOZ_OVERRIDE;

  virtual nsresult SyncLoadFontData(gfxProxyFontEntry* aFontToLoad,
                                    const gfxFontFaceSrc* aFontFaceSrc,
                                    uint8_t*& aBuffer,
                                    uint32_t& aBufferLength) MOZ_OVERRIDE;

  virtual bool GetPrivateBrowsing() MOZ_OVERRIDE;

  virtual void DoRebuildUserFontSet() MOZ_OVERRIDE;

  nsPresContext* mPresContext;  // weak reference

  // Set of all loaders pointing to us. These are not strong pointers,
  // but that's OK because nsFontFaceLoader always calls RemoveLoader on
  // us before it dies (unless we die first).
  nsTHashtable< nsPtrHashKey<nsFontFaceLoader> > mLoaders;

  nsTArray<FontFaceRuleRecord>   mRules;
};

class nsFontFaceLoader : public nsIStreamLoaderObserver
{
public:
  nsFontFaceLoader(gfxMixedFontFamily* aFontFamily,
                   gfxProxyFontEntry* aFontToLoad, nsIURI* aFontURI, 
                   nsUserFontSet* aFontSet, nsIChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER 

  // initiate the load
  nsresult Init();
  // cancel the load and remove its reference to mFontSet
  void Cancel();

  void DropChannel() { mChannel = nullptr; }

  void StartedLoading(nsIStreamLoader* aStreamLoader);

  static void LoadTimerCallback(nsITimer* aTimer, void* aClosure);

  static nsresult CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext);

protected:
  virtual ~nsFontFaceLoader();

private:
  nsRefPtr<gfxMixedFontFamily> mFontFamily;
  nsRefPtr<gfxProxyFontEntry>  mFontEntry;
  nsCOMPtr<nsIURI>        mFontURI;
  nsRefPtr<nsUserFontSet> mFontSet;
  nsCOMPtr<nsIChannel>    mChannel;
  nsCOMPtr<nsITimer>      mLoadTimer;

  nsIStreamLoader*        mStreamLoader;
};

#endif /* !defined(nsFontFaceLoader_h_) */
