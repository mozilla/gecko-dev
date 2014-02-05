/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfxMacPlatformFontList_H_
#define gfxMacPlatformFontList_H_

#include <CoreFoundation/CoreFoundation.h>

#include "mozilla/MemoryReporting.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"

#include "gfxPlatformFontList.h"
#include "gfxPlatform.h"
#include "gfxPlatformMac.h"

#include "nsUnicharUtils.h"
#include "nsTArray.h"

class gfxMacPlatformFontList;

// a single member of a font family (i.e. a single face, such as Times Italic)
class MacOSFontEntry : public gfxFontEntry
{
public:
    friend class gfxMacPlatformFontList;

    MacOSFontEntry(const nsAString& aPostscriptName, int32_t aWeight,
                   bool aIsStandardFace = false);

    // for use with data fonts
    MacOSFontEntry(const nsAString& aPostscriptName, CGFontRef aFontRef,
                   uint16_t aWeight, uint16_t aStretch, uint32_t aItalicStyle,
                   bool aIsUserFont, bool aIsLocal);

    virtual ~MacOSFontEntry() {
        ::CGFontRelease(mFontRef);
    }

    virtual CGFontRef GetFontRef();

    // override gfxFontEntry table access function to bypass table cache,
    // use CGFontRef API to get direct access to system font data
    virtual hb_blob_t *GetFontTable(uint32_t aTag) MOZ_OVERRIDE;

    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

    nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr);

    bool RequiresAATLayout() const { return mRequiresAAT; }

    bool IsCFF();

protected:
    virtual gfxFont* CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold);

    virtual bool HasFontTable(uint32_t aTableTag);

    static void DestroyBlobFunc(void* aUserData);

    CGFontRef mFontRef; // owning reference to the CGFont, released on destruction

    bool mFontRefInitialized;
    bool mRequiresAAT;
    bool mIsCFF;
    bool mIsCFFInitialized;
};

class gfxMacPlatformFontList : public gfxPlatformFontList {
public:
    static gfxMacPlatformFontList* PlatformFontList() {
        return static_cast<gfxMacPlatformFontList*>(sPlatformFontList);
    }

    static int32_t AppleWeightToCSSWeight(int32_t aAppleWeight);

    virtual gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle);

    virtual bool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);
    
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const uint8_t *aFontData, uint32_t aLength);

    void ClearPrefFonts() { mPrefFonts.Clear(); }

private:
    friend class gfxPlatformMac;

    gfxMacPlatformFontList();
    virtual ~gfxMacPlatformFontList();

    // initialize font lists
    virtual nsresult InitFontList();

    // special case font faces treated as font families (set via prefs)
    void InitSingleFaceList();

    static void RegisteredFontsChangedNotificationCallback(CFNotificationCenterRef center,
                                                           void *observer,
                                                           CFStringRef name,
                                                           const void *object,
                                                           CFDictionaryRef userInfo);

    // search fonts system-wide for a given character, null otherwise
    virtual gfxFontEntry* GlobalFontFallback(const uint32_t aCh,
                                             int32_t aRunScript,
                                             const gfxFontStyle* aMatchStyle,
                                             uint32_t& aCmapCount,
                                             gfxFontFamily** aMatchedFamily);

    virtual bool UsesSystemFallback() { return true; }

    virtual already_AddRefed<FontInfoData> CreateFontInfoData();

    enum {
        kATSGenerationInitial = -1
    };

    // default font for use with system-wide font fallback
    CTFontRef mDefaultFont;
};

#endif /* gfxMacPlatformFontList_H_ */
