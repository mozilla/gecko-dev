/*
 * Copyright 2008 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontConfigInterface.h"
#include "SkFontConfigTypeface.h"
#include "SkFontDescriptor.h"
#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontStream.h"
#include "SkStream.h"
#include "SkTypeface.h"
#include "SkTypefaceCache.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

SK_DECLARE_STATIC_MUTEX(gFontConfigInterfaceMutex);
static SkFontConfigInterface* gFontConfigInterface;

SkFontConfigInterface* SkFontConfigInterface::RefGlobal() {
    SkAutoMutexAcquire ac(gFontConfigInterfaceMutex);

    return SkSafeRef(gFontConfigInterface);
}

SkFontConfigInterface* SkFontConfigInterface::SetGlobal(SkFontConfigInterface* fc) {
    SkAutoMutexAcquire ac(gFontConfigInterfaceMutex);

    SkRefCnt_SafeAssign(gFontConfigInterface, fc);
    return fc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// convenience function to create the direct interface if none is installed.
extern SkFontConfigInterface* SkCreateDirectFontConfigInterface();

static SkFontConfigInterface* RefFCI() {
    for (;;) {
        SkFontConfigInterface* fci = SkFontConfigInterface::RefGlobal();
        if (fci) {
            return fci;
        }
        fci = SkFontConfigInterface::GetSingletonDirectInterface(&gFontConfigInterfaceMutex);
        SkFontConfigInterface::SetGlobal(fci);
    }
}

// export this to SkFontMgr_fontconfig.cpp until this file just goes away.
SkFontConfigInterface* SkFontHost_fontconfig_ref_global();
SkFontConfigInterface* SkFontHost_fontconfig_ref_global() {
    return RefFCI();
}

///////////////////////////////////////////////////////////////////////////////

struct FindRec {
    FindRec(const char* name, SkTypeface::Style style)
        : fFamilyName(name)  // don't need to make a deep copy
        , fStyle(style) {}

    const char* fFamilyName;
    SkTypeface::Style fStyle;
};

static bool find_proc(SkTypeface* face, SkTypeface::Style style, void* ctx) {
    FontConfigTypeface* fci = (FontConfigTypeface*)face;
    const FindRec* rec = (const FindRec*)ctx;

    return rec->fStyle == style && fci->isFamilyName(rec->fFamilyName);
}

SkTypeface* FontConfigTypeface::LegacyCreateTypeface(
                const SkTypeface* familyFace,
                const char familyName[],
                SkTypeface::Style style) {
    SkAutoTUnref<SkFontConfigInterface> fci(RefFCI());
    if (NULL == fci.get()) {
        return NULL;
    }

    if (familyFace) {
        FontConfigTypeface* fct = (FontConfigTypeface*)familyFace;
        familyName = fct->getFamilyName();
    }

    FindRec rec(familyName, style);
    SkTypeface* face = SkTypefaceCache::FindByProcAndRef(find_proc, &rec);
    if (face) {
//        SkDebugf("found cached face <%s> <%s> %p [%d]\n", familyName, ((FontConfigTypeface*)face)->getFamilyName(), face, face->getRefCnt());
        return face;
    }

    SkFontConfigInterface::FontIdentity indentity;
    SkString                            outFamilyName;
    SkTypeface::Style                   outStyle;

    if (!fci->matchFamilyName(familyName, style,
                              &indentity, &outFamilyName, &outStyle)) {
        return NULL;
    }

    // check if we, in fact, already have this. perhaps fontconfig aliased the
    // requested name to some other name we actually have...
    rec.fFamilyName = outFamilyName.c_str();
    rec.fStyle = outStyle;
    face = SkTypefaceCache::FindByProcAndRef(find_proc, &rec);
    if (face) {
        return face;
    }

    face = FontConfigTypeface::Create(outStyle, indentity, outFamilyName);
    SkTypefaceCache::Add(face, style);
//    SkDebugf("add face <%s> <%s> %p [%d]\n", familyName, outFamilyName.c_str(), face, face->getRefCnt());
    return face;
}

#ifdef SK_FONTHOST_DOES_NOT_USE_FONTMGR

SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style) {
    return FontConfigTypeface::LegacyCreateTypeface(familyFace, familyName,
                                                    style);
}

SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream) {
    if (!stream) {
        return NULL;
    }
    const size_t length = stream->getLength();
    if (!length) {
        return NULL;
    }
    if (length >= 1024 * 1024 * 1024) {
        return NULL;  // don't accept too large fonts (>= 1GB) for safety.
    }

    // ask freetype for reported style and if it is a fixed width font
    SkTypeface::Style style = SkTypeface::kNormal;
    bool isFixedWidth = false;
    if (!SkTypeface_FreeType::ScanFont(stream, 0, NULL, &style, &isFixedWidth)) {
        return NULL;
    }

    SkTypeface* face = SkNEW_ARGS(FontConfigTypeface, (style, isFixedWidth, stream));
    return face;
}

SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[]) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    return stream.get() ? CreateTypefaceFromStream(stream) : NULL;
}

#endif

///////////////////////////////////////////////////////////////////////////////

SkStream* FontConfigTypeface::onOpenStream(int* ttcIndex) const {
    SkStream* stream = this->getLocalStream();
    if (stream) {
        // should have been provided by CreateFromStream()
        *ttcIndex = 0;

        SkAutoTUnref<SkStream> dupStream(stream->duplicate());
        if (dupStream) {
            return dupStream.detach();
        }

        // TODO: update interface use, remove the following code in this block.
        size_t length = stream->getLength();

        const void* memory = stream->getMemoryBase();
        if (NULL != memory) {
            return new SkMemoryStream(memory, length, true);
        }

        SkAutoTMalloc<uint8_t> allocMemory(length);
        stream->rewind();
        if (length == stream->read(allocMemory.get(), length)) {
            SkAutoTUnref<SkMemoryStream> copyStream(new SkMemoryStream());
            copyStream->setMemoryOwned(allocMemory.detach(), length);
            return copyStream.detach();
        }

        stream->rewind();
        stream->ref();
    } else {
        SkAutoTUnref<SkFontConfigInterface> fci(RefFCI());
        if (NULL == fci.get()) {
            return NULL;
        }
        stream = fci->openStream(this->getIdentity());
        *ttcIndex = this->getIdentity().fTTCIndex;
    }
    return stream;
}

void FontConfigTypeface::onGetFontDescriptor(SkFontDescriptor* desc,
                                             bool* isLocalStream) const {
    desc->setFamilyName(this->getFamilyName());
    *isLocalStream = SkToBool(this->getLocalStream());
}
