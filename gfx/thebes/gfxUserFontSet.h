/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_USER_FONT_SET_H
#define GFX_USER_FONT_SET_H

#include "gfxFont.h"
#include "nsRefPtrHashtable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsURIHashKey.h"

class nsFontFaceLoader;

//#define DEBUG_USERFONT_CACHE

// parsed CSS @font-face rule information
// lifetime: from when @font-face rule processed until font is loaded
struct gfxFontFaceSrc {
    bool                   mIsLocal;       // url or local

    // if url, whether to use the origin principal or not
    bool                   mUseOriginPrincipal;

    // format hint flags, union of all possible formats
    // (e.g. TrueType, EOT, SVG, etc.)
    // see FLAG_FORMAT_* enum values below
    uint32_t               mFormatFlags;

    nsString               mLocalName;     // full font name if local
    nsCOMPtr<nsIURI>       mURI;           // uri if url
    nsCOMPtr<nsIURI>       mReferrer;      // referrer url if url
    nsCOMPtr<nsIPrincipal> mOriginPrincipal; // principal if url
};

inline bool
operator==(const gfxFontFaceSrc& a, const gfxFontFaceSrc& b)
{
    bool equals;
    return (a.mIsLocal && b.mIsLocal &&
            a.mLocalName == b.mLocalName) ||
           (!a.mIsLocal && !b.mIsLocal &&
            a.mUseOriginPrincipal == b.mUseOriginPrincipal &&
            a.mFormatFlags == b.mFormatFlags &&
            NS_SUCCEEDED(a.mURI->Equals(b.mURI, &equals)) && equals &&
            NS_SUCCEEDED(a.mReferrer->Equals(b.mReferrer, &equals)) && equals &&
            a.mOriginPrincipal->Equals(b.mOriginPrincipal));
}

// Subclassed to store platform-specific code cleaned out when font entry is
// deleted.
// Lifetime: from when platform font is created until it is deactivated.
// If the platform does not need to add any platform-specific code/data here,
// then the gfxUserFontSet will allocate a base gfxUserFontData and attach
// to the entry to track the basic user font info fields here.
class gfxUserFontData {
public:
    gfxUserFontData()
        : mSrcIndex(0), mFormat(0), mMetaOrigLen(0)
    { }
    virtual ~gfxUserFontData() { }

    nsTArray<uint8_t> mMetadata;  // woff metadata block (compressed), if any
    nsCOMPtr<nsIURI>  mURI;       // URI of the source, if it was url()
    nsCOMPtr<nsIPrincipal> mPrincipal; // principal for the download, if url()
    nsString          mLocalName; // font name used for the source, if local()
    nsString          mRealName;  // original fullname from the font resource
    uint32_t          mSrcIndex;  // index in the rule's source list
    uint32_t          mFormat;    // format hint for the source used, if any
    uint32_t          mMetaOrigLen; // length needed to decompress metadata
    bool              mPrivate;   // whether font belongs to a private window
};

// initially contains a set of proxy font entry objects, replaced with
// platform/user fonts as downloaded

class gfxMixedFontFamily : public gfxFontFamily {
public:
    friend class gfxUserFontSet;

    gfxMixedFontFamily(const nsAString& aName)
        : gfxFontFamily(aName) { }

    virtual ~gfxMixedFontFamily() { }

    // Add the given font entry to the end of the family's list.
    // Any earlier occurrence is removed, so this has the effect of "advancing"
    // the entry to the end of the list.
    void AddFontEntry(gfxFontEntry *aFontEntry) {
        // We append to mAvailableFonts -before- searching for and removing
        // any existing reference to avoid the risk that we'll remove the last
        // reference to the font entry, and thus delete it.
        mAvailableFonts.AppendElement(aFontEntry);
        uint32_t i = mAvailableFonts.Length() - 1;
        while (i > 0) {
            if (mAvailableFonts[--i] == aFontEntry) {
                mAvailableFonts.RemoveElementAt(i);
                break;
            }
        }
        aFontEntry->mFamilyName = Name();
        ResetCharacterMap();
    }

    // Replace aProxyFontEntry in the family's list with aRealFontEntry.
    void ReplaceFontEntry(gfxFontEntry *aProxyFontEntry,
                          gfxFontEntry *aRealFontEntry) {
        uint32_t numFonts = mAvailableFonts.Length();
        uint32_t i;
        for (i = 0; i < numFonts; i++) {
            gfxFontEntry *fe = mAvailableFonts[i];
            if (fe == aProxyFontEntry) {
                // Note that this may delete aProxyFontEntry, if there's no
                // other reference to it except from its family.
                mAvailableFonts[i] = aRealFontEntry;
                aRealFontEntry->mFamilyName = Name();
                break;
            }
        }
        NS_ASSERTION(i < numFonts, "font entry not found in family!");
        ResetCharacterMap();
    }

    // Remove all font entries from the family
    void DetachFontEntries() {
        mAvailableFonts.Clear();
    }
};

class gfxProxyFontEntry;

class gfxUserFontSet {

public:

    NS_INLINE_DECL_REFCOUNTING(gfxUserFontSet)

    gfxUserFontSet();

    enum {
        // no flags ==> no hint set
        // unknown ==> unknown format hint set
        FLAG_FORMAT_UNKNOWN        = 1,
        FLAG_FORMAT_OPENTYPE       = 1 << 1,
        FLAG_FORMAT_TRUETYPE       = 1 << 2,
        FLAG_FORMAT_TRUETYPE_AAT   = 1 << 3,
        FLAG_FORMAT_EOT            = 1 << 4,
        FLAG_FORMAT_SVG            = 1 << 5,
        FLAG_FORMAT_WOFF           = 1 << 6,

        // mask of all unused bits, update when adding new formats
        FLAG_FORMAT_NOT_USED       = ~((1 << 7)-1)
    };

    enum LoadStatus {
        STATUS_LOADING = 0,
        STATUS_LOADED,
        STATUS_FORMAT_NOT_SUPPORTED,
        STATUS_ERROR,
        STATUS_END_OF_LIST
    };


    // add in a font face
    // weight - 0 == unknown, [100, 900] otherwise (multiples of 100)
    // stretch = [NS_FONT_STRETCH_ULTRA_CONDENSED, NS_FONT_STRETCH_ULTRA_EXPANDED]
    // italic style = constants in gfxFontConstants.h, e.g. NS_FONT_STYLE_NORMAL
    // TODO: support for unicode ranges not yet implemented
    gfxFontEntry *AddFontFace(const nsAString& aFamilyName,
                              const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                              uint32_t aWeight,
                              int32_t aStretch,
                              uint32_t aItalicStyle,
                              const nsTArray<gfxFontFeature>& aFeatureSettings,
                              const nsString& aLanguageOverride,
                              gfxSparseBitSet *aUnicodeRanges = nullptr);

    // add in a font face for which we have the gfxFontEntry already
    void AddFontFace(const nsAString& aFamilyName, gfxFontEntry* aFontEntry);

    // Whether there is a face with this family name
    bool HasFamily(const nsAString& aFamilyName) const
    {
        return GetFamily(aFamilyName) != nullptr;
    }

    gfxFontFamily *GetFamily(const nsAString& aName) const;

    // Lookup a font entry for a given style, returns null if not loaded.
    // aFamily must be a family returned by our GetFamily method.
    gfxFontEntry *FindFontEntry(gfxFontFamily *aFamily,
                                const gfxFontStyle& aFontStyle,
                                bool& aNeedsBold,
                                bool& aWaitForUserFont);

    // Find a family (possibly one of several!) that owns the given entry.
    // This may be somewhat expensive, as it enumerates all the fonts in
    // the set. Currently used only by the Linux (gfxPangoFontGroup) backend,
    // which does not directly track families in the font group's list.
    gfxFontFamily *FindFamilyFor(gfxFontEntry *aFontEntry) const;

    // check whether the given source is allowed to be loaded;
    // returns the Principal (for use in the key when caching the loaded font),
    // and whether the load should bypass the cache (force-reload).
    virtual nsresult CheckFontLoad(const gfxFontFaceSrc *aFontFaceSrc,
                                   nsIPrincipal **aPrincipal,
                                   bool *aBypassCache) = 0;

    // initialize the process that loads external font data, which upon 
    // completion will call OnLoadComplete method
    virtual nsresult StartLoad(gfxMixedFontFamily *aFamily,
                               gfxProxyFontEntry *aProxy,
                               const gfxFontFaceSrc *aFontFaceSrc) = 0;

    // when download has been completed, pass back data here
    // aDownloadStatus == NS_OK ==> download succeeded, error otherwise
    // returns true if platform font creation sucessful (or local()
    // reference was next in line)
    // Ownership of aFontData is passed in here; the font set must
    // ensure that it is eventually deleted with NS_Free().
    bool OnLoadComplete(gfxMixedFontFamily *aFamily,
                        gfxProxyFontEntry *aProxy,
                        const uint8_t *aFontData, uint32_t aLength,
                        nsresult aDownloadStatus);

    // Replace a proxy with a real fontEntry; this is implemented in
    // nsUserFontSet in order to keep track of the entry corresponding
    // to each @font-face rule.
    virtual void ReplaceFontEntry(gfxMixedFontFamily *aFamily,
                                  gfxProxyFontEntry *aProxy,
                                  gfxFontEntry *aFontEntry) = 0;

    // generation - each time a face is loaded, generation is
    // incremented so that the change can be recognized 
    uint64_t GetGeneration() { return mGeneration; }

    // increment the generation on font load
    void IncrementGeneration();

    // rebuild if local rules have been used
    void RebuildLocalRules();

    class UserFontCache {
    public:
        // Record a loaded user-font in the cache. This requires that the
        // font-entry's userFontData has been set up already, as it relies
        // on the URI and Principal recorded there.
        static void CacheFont(gfxFontEntry *aFontEntry);

        // The given gfxFontEntry is being destroyed, so remove any record that
        // refers to it.
        static void ForgetFont(gfxFontEntry *aFontEntry);

        // Return the gfxFontEntry corresponding to a given URI and principal,
        // and the features of the given proxy, or nullptr if none is available.
        // The aPrivate flag is set for requests coming from private windows,
        // so we can avoid leaking fonts cached in private windows mode out to
        // normal windows.
        static gfxFontEntry* GetFont(nsIURI            *aSrcURI,
                                     nsIPrincipal      *aPrincipal,
                                     gfxProxyFontEntry *aProxy,
                                     bool               aPrivate);

        // Clear everything so that we don't leak URIs and Principals.
        static void Shutdown();

#ifdef DEBUG_USERFONT_CACHE
        // dump contents
        static void Dump();
#endif

    private:
        // Helper that we use to observe the empty-cache notification
        // from nsICacheService.
        class Flusher : public nsIObserver
        {
            virtual ~Flusher() {}
        public:
            NS_DECL_ISUPPORTS
            NS_DECL_NSIOBSERVER
            Flusher() {}
        };

        // Key used to look up entries in the user-font cache.
        // Note that key comparison does *not* use the mFontEntry field
        // as a whole; it only compares specific fields within the entry
        // (weight/width/style/features) that could affect font selection
        // or rendering, and that must match between a font-set's proxy
        // entry and the corresponding "real" font entry.
        struct Key {
            nsCOMPtr<nsIURI>        mURI;
            nsCOMPtr<nsIPrincipal>  mPrincipal;
            gfxFontEntry           *mFontEntry;
            bool                    mPrivate;

            Key(nsIURI* aURI, nsIPrincipal* aPrincipal,
                gfxFontEntry* aFontEntry, bool aPrivate)
                : mURI(aURI),
                  mPrincipal(aPrincipal),
                  mFontEntry(aFontEntry),
                  mPrivate(aPrivate)
            { }
        };

        class Entry : public PLDHashEntryHdr {
        public:
            typedef const Key& KeyType;
            typedef const Key* KeyTypePointer;

            Entry(KeyTypePointer aKey)
                : mURI(aKey->mURI),
                  mPrincipal(aKey->mPrincipal),
                  mFontEntry(aKey->mFontEntry),
                  mPrivate(aKey->mPrivate)
            { }

            Entry(const Entry& aOther)
                : mURI(aOther.mURI),
                  mPrincipal(aOther.mPrincipal),
                  mFontEntry(aOther.mFontEntry),
                  mPrivate(aOther.mPrivate)
            { }

            ~Entry() { }

            bool KeyEquals(const KeyTypePointer aKey) const;

            static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

            static PLDHashNumber HashKey(const KeyTypePointer aKey) {
                uint32_t principalHash;
                aKey->mPrincipal->GetHashValue(&principalHash);
                return mozilla::HashGeneric(principalHash + int(aKey->mPrivate),
                                            nsURIHashKey::HashKey(aKey->mURI),
                                            HashFeatures(aKey->mFontEntry->mFeatureSettings),
                                            mozilla::HashString(aKey->mFontEntry->mFamilyName),
                                            ((uint32_t)aKey->mFontEntry->mItalic |
                                             (aKey->mFontEntry->mWeight << 1) |
                                             (aKey->mFontEntry->mStretch << 10) ) ^
                                             aKey->mFontEntry->mLanguageOverride);
            }

            enum { ALLOW_MEMMOVE = false };

            gfxFontEntry* GetFontEntry() const { return mFontEntry; }

            static PLDHashOperator RemoveIfPrivate(Entry* aEntry, void* aUserData);
            static PLDHashOperator RemoveIfMatches(Entry* aEntry, void* aUserData);
            static PLDHashOperator DisconnectSVG(Entry* aEntry, void* aUserData);

#ifdef DEBUG_USERFONT_CACHE
            static PLDHashOperator DumpEntry(Entry* aEntry, void* aUserData);
#endif

        private:
            static uint32_t
            HashFeatures(const nsTArray<gfxFontFeature>& aFeatures) {
                return mozilla::HashBytes(aFeatures.Elements(),
                                          aFeatures.Length() * sizeof(gfxFontFeature));
            }

            nsCOMPtr<nsIURI>       mURI;
            nsCOMPtr<nsIPrincipal> mPrincipal;

            // The "real" font entry corresponding to this downloaded font.
            // The font entry MUST notify the cache when it is destroyed
            // (by calling Forget()).
            gfxFontEntry          *mFontEntry;

            // Whether this font was loaded from a private window.
            bool                   mPrivate;
        };

        static nsTHashtable<Entry> *sUserFonts;
    };

protected:
    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxUserFontSet();

    // Return whether the font set is associated with a private-browsing tab.
    virtual bool GetPrivateBrowsing() = 0;

    // for a given proxy font entry, attempt to load the next resource
    // in the src list
    LoadStatus LoadNext(gfxMixedFontFamily *aFamily,
                        gfxProxyFontEntry *aProxyEntry);

    // helper method for creating a platform font
    // returns font entry if platform font creation successful
    // Ownership of aFontData is passed in here; the font set must
    // ensure that it is eventually deleted with NS_Free().
    gfxFontEntry* LoadFont(gfxMixedFontFamily *aFamily,
                           gfxProxyFontEntry *aProxy,
                           const uint8_t *aFontData, uint32_t &aLength);

    // parse data for a data URL
    virtual nsresult SyncLoadFontData(gfxProxyFontEntry *aFontToLoad,
                                      const gfxFontFaceSrc *aFontFaceSrc,
                                      uint8_t* &aBuffer,
                                      uint32_t &aBufferLength) = 0;

    // report a problem of some kind (implemented in nsUserFontSet)
    virtual nsresult LogMessage(gfxMixedFontFamily *aFamily,
                                gfxProxyFontEntry *aProxy,
                                const char *aMessage,
                                uint32_t aFlags = nsIScriptError::errorFlag,
                                nsresult aStatus = NS_OK) = 0;

    const uint8_t* SanitizeOpenTypeData(gfxMixedFontFamily *aFamily,
                                        gfxProxyFontEntry *aProxy,
                                        const uint8_t* aData,
                                        uint32_t aLength,
                                        uint32_t& aSaneLength,
                                        bool aIsCompressed);

    static bool OTSMessage(void *aUserData, const char *format, ...);

    // helper method for performing the actual userfont set rebuild
    virtual void DoRebuildUserFontSet() = 0;

    // font families defined by @font-face rules
    nsRefPtrHashtable<nsStringHashKey, gfxMixedFontFamily> mFontFamilies;

    uint64_t        mGeneration;

    // true when local names have been looked up, false otherwise
    bool mLocalRulesUsed;

    static PRLogModuleInfo* GetUserFontsLog();

private:
    static void CopyWOFFMetadata(const uint8_t* aFontData,
                                 uint32_t aLength,
                                 FallibleTArray<uint8_t>* aMetadata,
                                 uint32_t* aMetaOrigLen);
};

// acts a placeholder until the real font is downloaded

class gfxProxyFontEntry : public gfxFontEntry {
    friend class gfxUserFontSet;

public:
    gfxProxyFontEntry(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                      uint32_t aWeight,
                      int32_t aStretch,
                      uint32_t aItalicStyle,
                      const nsTArray<gfxFontFeature>& aFeatureSettings,
                      uint32_t aLanguageOverride,
                      gfxSparseBitSet *aUnicodeRanges);

    virtual ~gfxProxyFontEntry();

    // Return whether the entry matches the given list of attributes
    bool Matches(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
                 uint32_t aWeight,
                 int32_t aStretch,
                 uint32_t aItalicStyle,
                 const nsTArray<gfxFontFeature>& aFeatureSettings,
                 uint32_t aLanguageOverride,
                 gfxSparseBitSet *aUnicodeRanges);

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold);

    // note that code depends on the ordering of these values!
    enum LoadingState {
        NOT_LOADING = 0,     // not started to load any font resources yet
        LOADING_STARTED,     // loading has started; hide fallback font
        LOADING_ALMOST_DONE, // timeout happened but we're nearly done,
                             // so keep hiding fallback font
        LOADING_SLOWLY,      // timeout happened and we're not nearly done,
                             // so use the fallback font
        LOADING_FAILED       // failed to load any source: use fallback
    };
    LoadingState             mLoadingState;
    bool                     mUnsupportedFormat;

    nsTArray<gfxFontFaceSrc> mSrcList;
    uint32_t                 mSrcIndex; // index of loading src item
    nsFontFaceLoader        *mLoader; // current loader for this entry, if any
    nsCOMPtr<nsIPrincipal>   mPrincipal;
};


#endif /* GFX_USER_FONT_SET_H */
