/*
*******************************************************************************
*
*   Copyright (C) 2009-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"
#include "cpputils.h"
#include "cstring.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "ucln_cmn.h"
#include "uhash.h"

U_NAMESPACE_BEGIN

// Public API dispatch via Normalizer2 subclasses -------------------------- ***

Normalizer2::~Normalizer2() {}

UBool
Normalizer2::getRawDecomposition(UChar32, UnicodeString &) const {
    return FALSE;
}

UChar32
Normalizer2::composePair(UChar32, UChar32) const {
    return U_SENTINEL;
}

uint8_t
Normalizer2::getCombiningClass(UChar32 /*c*/) const {
    return 0;
}

// Normalizer2 implementation for the old UNORM_NONE.
class NoopNormalizer2 : public Normalizer2 {
    virtual ~NoopNormalizer2();

    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            if(&dest!=&src) {
                dest=src;
            } else {
                errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            }
        }
        return dest;
    }
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            if(&first!=&second) {
                first.append(second);
            } else {
                errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            }
        }
        return first;
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        if(U_SUCCESS(errorCode)) {
            if(&first!=&second) {
                first.append(second);
            } else {
                errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            }
        }
        return first;
    }
    virtual UBool
    getDecomposition(UChar32, UnicodeString &) const {
        return FALSE;
    }
    // No need to override the default getRawDecomposition().
    virtual UBool
    isNormalized(const UnicodeString &, UErrorCode &) const {
        return TRUE;
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &, UErrorCode &) const {
        return UNORM_YES;
    }
    virtual int32_t
    spanQuickCheckYes(const UnicodeString &s, UErrorCode &) const {
        return s.length();
    }
    virtual UBool hasBoundaryBefore(UChar32) const { return TRUE; }
    virtual UBool hasBoundaryAfter(UChar32) const { return TRUE; }
    virtual UBool isInert(UChar32) const { return TRUE; }
};

NoopNormalizer2::~NoopNormalizer2() {}

// Intermediate class:
// Has Normalizer2Impl and does boilerplate argument checking and setup.
class Normalizer2WithImpl : public Normalizer2 {
public:
    Normalizer2WithImpl(const Normalizer2Impl &ni) : impl(ni) {}
    virtual ~Normalizer2WithImpl();

    // normalize
    virtual UnicodeString &
    normalize(const UnicodeString &src,
              UnicodeString &dest,
              UErrorCode &errorCode) const {
        if(U_FAILURE(errorCode)) {
            dest.setToBogus();
            return dest;
        }
        const UChar *sArray=src.getBuffer();
        if(&dest==&src || sArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            dest.setToBogus();
            return dest;
        }
        dest.remove();
        ReorderingBuffer buffer(impl, dest);
        if(buffer.init(src.length(), errorCode)) {
            normalize(sArray, sArray+src.length(), buffer, errorCode);
        }
        return dest;
    }
    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const = 0;

    // normalize and append
    virtual UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UErrorCode &errorCode) const {
        return normalizeSecondAndAppend(first, second, TRUE, errorCode);
    }
    virtual UnicodeString &
    append(UnicodeString &first,
           const UnicodeString &second,
           UErrorCode &errorCode) const {
        return normalizeSecondAndAppend(first, second, FALSE, errorCode);
    }
    UnicodeString &
    normalizeSecondAndAppend(UnicodeString &first,
                             const UnicodeString &second,
                             UBool doNormalize,
                             UErrorCode &errorCode) const {
        uprv_checkCanGetBuffer(first, errorCode);
        if(U_FAILURE(errorCode)) {
            return first;
        }
        const UChar *secondArray=second.getBuffer();
        if(&first==&second || secondArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return first;
        }
        int32_t firstLength=first.length();
        UnicodeString safeMiddle;
        {
            ReorderingBuffer buffer(impl, first);
            if(buffer.init(firstLength+second.length(), errorCode)) {
                normalizeAndAppend(secondArray, secondArray+second.length(), doNormalize,
                                   safeMiddle, buffer, errorCode);
            }
        }  // The ReorderingBuffer destructor finalizes the first string.
        if(U_FAILURE(errorCode)) {
            // Restore the modified suffix of the first string.
            first.replace(firstLength-safeMiddle.length(), 0x7fffffff, safeMiddle);
        }
        return first;
    }
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       UnicodeString &safeMiddle,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const = 0;
    virtual UBool
    getDecomposition(UChar32 c, UnicodeString &decomposition) const {
        UChar buffer[4];
        int32_t length;
        const UChar *d=impl.getDecomposition(c, buffer, length);
        if(d==NULL) {
            return FALSE;
        }
        if(d==buffer) {
            decomposition.setTo(buffer, length);  // copy the string (Jamos from Hangul syllable c)
        } else {
            decomposition.setTo(FALSE, d, length);  // read-only alias
        }
        return TRUE;
    }
    virtual UBool
    getRawDecomposition(UChar32 c, UnicodeString &decomposition) const {
        UChar buffer[30];
        int32_t length;
        const UChar *d=impl.getRawDecomposition(c, buffer, length);
        if(d==NULL) {
            return FALSE;
        }
        if(d==buffer) {
            decomposition.setTo(buffer, length);  // copy the string (algorithmic decomposition)
        } else {
            decomposition.setTo(FALSE, d, length);  // read-only alias
        }
        return TRUE;
    }
    virtual UChar32
    composePair(UChar32 a, UChar32 b) const {
        return impl.composePair(a, b);
    }

    virtual uint8_t
    getCombiningClass(UChar32 c) const {
        return impl.getCC(impl.getNorm16(c));
    }

    // quick checks
    virtual UBool
    isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }
        const UChar *sArray=s.getBuffer();
        if(sArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return FALSE;
        }
        const UChar *sLimit=sArray+s.length();
        return sLimit==spanQuickCheckYes(sArray, sLimit, errorCode);
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
        return Normalizer2WithImpl::isNormalized(s, errorCode) ? UNORM_YES : UNORM_NO;
    }
    virtual int32_t
    spanQuickCheckYes(const UnicodeString &s, UErrorCode &errorCode) const {
        if(U_FAILURE(errorCode)) {
            return 0;
        }
        const UChar *sArray=s.getBuffer();
        if(sArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        return (int32_t)(spanQuickCheckYes(sArray, sArray+s.length(), errorCode)-sArray);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const = 0;

    virtual UNormalizationCheckResult getQuickCheck(UChar32) const {
        return UNORM_YES;
    }

    const Normalizer2Impl &impl;
};

Normalizer2WithImpl::~Normalizer2WithImpl() {}

class DecomposeNormalizer2 : public Normalizer2WithImpl {
public:
    DecomposeNormalizer2(const Normalizer2Impl &ni) : Normalizer2WithImpl(ni) {}
    virtual ~DecomposeNormalizer2();

private:
    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.decompose(src, limit, &buffer, errorCode);
    }
    using Normalizer2WithImpl::normalize;  // Avoid warning about hiding base class function.
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       UnicodeString &safeMiddle,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.decomposeAndAppend(src, limit, doNormalize, safeMiddle, buffer, errorCode);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const {
        return impl.decompose(src, limit, NULL, errorCode);
    }
    using Normalizer2WithImpl::spanQuickCheckYes;  // Avoid warning about hiding base class function.
    virtual UNormalizationCheckResult getQuickCheck(UChar32 c) const {
        return impl.isDecompYes(impl.getNorm16(c)) ? UNORM_YES : UNORM_NO;
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const { return impl.hasDecompBoundary(c, TRUE); }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return impl.hasDecompBoundary(c, FALSE); }
    virtual UBool isInert(UChar32 c) const { return impl.isDecompInert(c); }
};

DecomposeNormalizer2::~DecomposeNormalizer2() {}

class ComposeNormalizer2 : public Normalizer2WithImpl {
public:
    ComposeNormalizer2(const Normalizer2Impl &ni, UBool fcc) :
        Normalizer2WithImpl(ni), onlyContiguous(fcc) {}
    virtual ~ComposeNormalizer2();

private:
    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.compose(src, limit, onlyContiguous, TRUE, buffer, errorCode);
    }
    using Normalizer2WithImpl::normalize;  // Avoid warning about hiding base class function.
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       UnicodeString &safeMiddle,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.composeAndAppend(src, limit, doNormalize, onlyContiguous, safeMiddle, buffer, errorCode);
    }

    virtual UBool
    isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }
        const UChar *sArray=s.getBuffer();
        if(sArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return FALSE;
        }
        UnicodeString temp;
        ReorderingBuffer buffer(impl, temp);
        if(!buffer.init(5, errorCode)) {  // small destCapacity for substring normalization
            return FALSE;
        }
        return impl.compose(sArray, sArray+s.length(), onlyContiguous, FALSE, buffer, errorCode);
    }
    virtual UNormalizationCheckResult
    quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
        if(U_FAILURE(errorCode)) {
            return UNORM_MAYBE;
        }
        const UChar *sArray=s.getBuffer();
        if(sArray==NULL) {
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            return UNORM_MAYBE;
        }
        UNormalizationCheckResult qcResult=UNORM_YES;
        impl.composeQuickCheck(sArray, sArray+s.length(), onlyContiguous, &qcResult);
        return qcResult;
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &) const {
        return impl.composeQuickCheck(src, limit, onlyContiguous, NULL);
    }
    using Normalizer2WithImpl::spanQuickCheckYes;  // Avoid warning about hiding base class function.
    virtual UNormalizationCheckResult getQuickCheck(UChar32 c) const {
        return impl.getCompQuickCheck(impl.getNorm16(c));
    }
    virtual UBool hasBoundaryBefore(UChar32 c) const {
        return impl.hasCompBoundaryBefore(c);
    }
    virtual UBool hasBoundaryAfter(UChar32 c) const {
        return impl.hasCompBoundaryAfter(c, onlyContiguous, FALSE);
    }
    virtual UBool isInert(UChar32 c) const {
        return impl.hasCompBoundaryAfter(c, onlyContiguous, TRUE);
    }

    const UBool onlyContiguous;
};

ComposeNormalizer2::~ComposeNormalizer2() {}

class FCDNormalizer2 : public Normalizer2WithImpl {
public:
    FCDNormalizer2(const Normalizer2Impl &ni) : Normalizer2WithImpl(ni) {}
    virtual ~FCDNormalizer2();

private:
    virtual void
    normalize(const UChar *src, const UChar *limit,
              ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.makeFCD(src, limit, &buffer, errorCode);
    }
    using Normalizer2WithImpl::normalize;  // Avoid warning about hiding base class function.
    virtual void
    normalizeAndAppend(const UChar *src, const UChar *limit, UBool doNormalize,
                       UnicodeString &safeMiddle,
                       ReorderingBuffer &buffer, UErrorCode &errorCode) const {
        impl.makeFCDAndAppend(src, limit, doNormalize, safeMiddle, buffer, errorCode);
    }
    virtual const UChar *
    spanQuickCheckYes(const UChar *src, const UChar *limit, UErrorCode &errorCode) const {
        return impl.makeFCD(src, limit, NULL, errorCode);
    }
    using Normalizer2WithImpl::spanQuickCheckYes;  // Avoid warning about hiding base class function.
    virtual UBool hasBoundaryBefore(UChar32 c) const { return impl.hasFCDBoundaryBefore(c); }
    virtual UBool hasBoundaryAfter(UChar32 c) const { return impl.hasFCDBoundaryAfter(c); }
    virtual UBool isInert(UChar32 c) const { return impl.isFCDInert(c); }
};

FCDNormalizer2::~FCDNormalizer2() {}

// instance cache ---------------------------------------------------------- ***

struct Norm2AllModes : public UMemory {
    static Norm2AllModes *createInstance(const char *packageName,
                                         const char *name,
                                         UErrorCode &errorCode);
    Norm2AllModes() : comp(impl, FALSE), decomp(impl), fcd(impl), fcc(impl, TRUE) {}

    Normalizer2Impl impl;
    ComposeNormalizer2 comp;
    DecomposeNormalizer2 decomp;
    FCDNormalizer2 fcd;
    ComposeNormalizer2 fcc;
};

Norm2AllModes *
Norm2AllModes::createInstance(const char *packageName,
                              const char *name,
                              UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    LocalPointer<Norm2AllModes> allModes(new Norm2AllModes);
    if(allModes.isNull()) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    allModes->impl.load(packageName, name, errorCode);
    return U_SUCCESS(errorCode) ? allModes.orphan() : NULL;
}

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_normalizer2_cleanup();
U_CDECL_END


static Norm2AllModes *nfcSingleton;
static Norm2AllModes *nfkcSingleton;
static Norm2AllModes *nfkc_cfSingleton;
static Normalizer2   *noopSingleton;
static UHashtable    *cache=NULL;

static icu::UInitOnce nfcInitOnce = U_INITONCE_INITIALIZER;
static icu::UInitOnce nfkcInitOnce = U_INITONCE_INITIALIZER;
static icu::UInitOnce nfkc_cfInitOnce = U_INITONCE_INITIALIZER;
static icu::UInitOnce noopInitOnce = U_INITONCE_INITIALIZER;

// UInitOnce singleton initialization function
static void U_CALLCONV initSingletons(const char *what, UErrorCode &errorCode) {
    if (uprv_strcmp(what, "nfc") == 0) {
        nfcSingleton     = Norm2AllModes::createInstance(NULL, "nfc", errorCode);
    } else if (uprv_strcmp(what, "nfkc") == 0) {
        nfkcSingleton    = Norm2AllModes::createInstance(NULL, "nfkc", errorCode);
    } else if (uprv_strcmp(what, "nfkc_cf") == 0) {
        nfkc_cfSingleton = Norm2AllModes::createInstance(NULL, "nfkc_cf", errorCode);
    } else if (uprv_strcmp(what, "noop") == 0) {
        noopSingleton    = new NoopNormalizer2;
    } else {
        U_ASSERT(FALSE);   // Unknown singleton
    }
    ucln_common_registerCleanup(UCLN_COMMON_NORMALIZER2, uprv_normalizer2_cleanup);
}

U_CDECL_BEGIN

static void U_CALLCONV deleteNorm2AllModes(void *allModes) {
    delete (Norm2AllModes *)allModes;
}

static UBool U_CALLCONV uprv_normalizer2_cleanup() {
    delete nfcSingleton;
    nfcSingleton = NULL;
    delete nfkcSingleton;
    nfkcSingleton = NULL;
    delete nfkc_cfSingleton;
    nfkc_cfSingleton = NULL;
    delete noopSingleton;
    noopSingleton = NULL;
    uhash_close(cache);
    cache=NULL;
    nfcInitOnce.reset(); 
    nfkcInitOnce.reset(); 
    nfkc_cfInitOnce.reset(); 
    noopInitOnce.reset(); 
    return TRUE;
}

U_CDECL_END

const Normalizer2 *Normalizer2Factory::getNFCInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton!=NULL ? &nfcSingleton->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFDInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton!=NULL ? &nfcSingleton->decomp : NULL;
}

const Normalizer2 *Normalizer2Factory::getFCDInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton!=NULL ? &nfcSingleton->fcd : NULL;
}

const Normalizer2 *Normalizer2Factory::getFCCInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton!=NULL ? &nfcSingleton->fcc : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKCInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfkcInitOnce, &initSingletons, "nfkc", errorCode);
    return nfkcSingleton!=NULL ? &nfkcSingleton->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKDInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfkcInitOnce, &initSingletons, "nfkc", errorCode);
    return nfkcSingleton!=NULL ? &nfkcSingleton->decomp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNFKC_CFInstance(UErrorCode &errorCode) {
    umtx_initOnce(nfkc_cfInitOnce, &initSingletons, "nfkc_cf", errorCode);
    return nfkc_cfSingleton!=NULL ? &nfkc_cfSingleton->comp : NULL;
}

const Normalizer2 *Normalizer2Factory::getNoopInstance(UErrorCode &errorCode) {
    umtx_initOnce(noopInitOnce, &initSingletons, "noop", errorCode);
    return noopSingleton;
}

const Normalizer2 *
Normalizer2Factory::getInstance(UNormalizationMode mode, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    switch(mode) {
    case UNORM_NFD:
        return getNFDInstance(errorCode);
    case UNORM_NFKD:
        return getNFKDInstance(errorCode);
    case UNORM_NFC:
        return getNFCInstance(errorCode);
    case UNORM_NFKC:
        return getNFKCInstance(errorCode);
    case UNORM_FCD:
        return getFCDInstance(errorCode);
    default:  // UNORM_NONE
        return getNoopInstance(errorCode);
    }
}

const Normalizer2Impl *
Normalizer2Factory::getNFCImpl(UErrorCode &errorCode) {
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton!=NULL ? &nfcSingleton->impl : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getNFKCImpl(UErrorCode &errorCode) {
    umtx_initOnce(nfkcInitOnce, &initSingletons, "nfkc", errorCode);
    return nfkcSingleton!=NULL ? &nfkcSingleton->impl : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getNFKC_CFImpl(UErrorCode &errorCode) {
    umtx_initOnce(nfkc_cfInitOnce, &initSingletons, "nfkc_cf", errorCode);
    return nfkc_cfSingleton!=NULL ? &nfkc_cfSingleton->impl : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getImpl(const Normalizer2 *norm2) {
    return &((Normalizer2WithImpl *)norm2)->impl;
}

const Normalizer2 *
Normalizer2::getNFCInstance(UErrorCode &errorCode) {
    return Normalizer2Factory::getNFCInstance(errorCode);
}

const Normalizer2 *
Normalizer2::getNFDInstance(UErrorCode &errorCode) {
    return Normalizer2Factory::getNFDInstance(errorCode);
}

const Normalizer2 *
Normalizer2::getNFKCInstance(UErrorCode &errorCode) {
    return Normalizer2Factory::getNFKCInstance(errorCode);
}

const Normalizer2 *
Normalizer2::getNFKDInstance(UErrorCode &errorCode) {
    return Normalizer2Factory::getNFKDInstance(errorCode);
}

const Normalizer2 *
Normalizer2::getNFKCCasefoldInstance(UErrorCode &errorCode) {
    return Normalizer2Factory::getNFKC_CFInstance(errorCode);
}

const Normalizer2 *
Normalizer2::getInstance(const char *packageName,
                         const char *name,
                         UNormalization2Mode mode,
                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    if(name==NULL || *name==0) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    Norm2AllModes *allModes=NULL;
    if(packageName==NULL) {
        if(0==uprv_strcmp(name, "nfc")) {
            umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
            allModes=nfcSingleton;
        } else if(0==uprv_strcmp(name, "nfkc")) {
            umtx_initOnce(nfkcInitOnce, &initSingletons, "nfkc", errorCode);
            allModes=nfkcSingleton;
        } else if(0==uprv_strcmp(name, "nfkc_cf")) {
            umtx_initOnce(nfkc_cfInitOnce, &initSingletons, "nfkc_cf", errorCode);
            allModes=nfkc_cfSingleton;
        }
    }
    if(allModes==NULL && U_SUCCESS(errorCode)) {
        {
            Mutex lock;
            if(cache!=NULL) {
                allModes=(Norm2AllModes *)uhash_get(cache, name);
            }
        }
        if(allModes==NULL) {
            LocalPointer<Norm2AllModes> localAllModes(
                Norm2AllModes::createInstance(packageName, name, errorCode));
            if(U_SUCCESS(errorCode)) {
                Mutex lock;
                if(cache==NULL) {
                    cache=uhash_open(uhash_hashChars, uhash_compareChars, NULL, &errorCode);
                    if(U_FAILURE(errorCode)) {
                        return NULL;
                    }
                    uhash_setKeyDeleter(cache, uprv_free);
                    uhash_setValueDeleter(cache, deleteNorm2AllModes);
                }
                void *temp=uhash_get(cache, name);
                if(temp==NULL) {
                    int32_t keyLength=uprv_strlen(name)+1;
                    char *nameCopy=(char *)uprv_malloc(keyLength);
                    if(nameCopy==NULL) {
                        errorCode=U_MEMORY_ALLOCATION_ERROR;
                        return NULL;
                    }
                    uprv_memcpy(nameCopy, name, keyLength);
                    uhash_put(cache, nameCopy, allModes=localAllModes.orphan(), &errorCode);
                } else {
                    // race condition
                    allModes=(Norm2AllModes *)temp;
                }
            }
        }
    }
    if(allModes!=NULL && U_SUCCESS(errorCode)) {
        switch(mode) {
        case UNORM2_COMPOSE:
            return &allModes->comp;
        case UNORM2_DECOMPOSE:
            return &allModes->decomp;
        case UNORM2_FCD:
            return &allModes->fcd;
        case UNORM2_COMPOSE_CONTIGUOUS:
            return &allModes->fcc;
        default:
            break;  // do nothing
        }
    }
    return NULL;
}

U_NAMESPACE_END

// C API ------------------------------------------------------------------- ***

U_NAMESPACE_USE

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFCInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFCInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFDInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFDInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKCInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKCInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKDInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKDInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKCCasefoldInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKCCasefoldInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getInstance(const char *packageName,
                   const char *name,
                   UNormalization2Mode mode,
                   UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getInstance(packageName, name, mode, *pErrorCode);
}

U_CAPI void U_EXPORT2
unorm2_close(UNormalizer2 *norm2) {
    delete (Normalizer2 *)norm2;
}

U_CAPI int32_t U_EXPORT2
unorm2_normalize(const UNormalizer2 *norm2,
                 const UChar *src, int32_t length,
                 UChar *dest, int32_t capacity,
                 UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if( (src==NULL ? length!=0 : length<-1) ||
        (dest==NULL ? capacity!=0 : capacity<0) ||
        (src==dest && src!=NULL)
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString destString(dest, 0, capacity);
    // length==0: Nothing to do, and n2wi->normalize(NULL, NULL, buffer, ...) would crash.
    if(length!=0) {
        const Normalizer2 *n2=(const Normalizer2 *)norm2;
        const Normalizer2WithImpl *n2wi=dynamic_cast<const Normalizer2WithImpl *>(n2);
        if(n2wi!=NULL) {
            // Avoid duplicate argument checking and support NUL-terminated src.
            ReorderingBuffer buffer(n2wi->impl, destString);
            if(buffer.init(length, *pErrorCode)) {
                n2wi->normalize(src, length>=0 ? src+length : NULL, buffer, *pErrorCode);
            }
        } else {
            UnicodeString srcString(length<0, src, length);
            n2->normalize(srcString, destString, *pErrorCode);
        }
    }
    return destString.extract(dest, capacity, *pErrorCode);
}

static int32_t
normalizeSecondAndAppend(const UNormalizer2 *norm2,
                         UChar *first, int32_t firstLength, int32_t firstCapacity,
                         const UChar *second, int32_t secondLength,
                         UBool doNormalize,
                         UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if( (second==NULL ? secondLength!=0 : secondLength<-1) ||
        (first==NULL ? (firstCapacity!=0 || firstLength!=0) :
                       (firstCapacity<0 || firstLength<-1)) ||
        (first==second && first!=NULL)
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString firstString(first, firstLength, firstCapacity);
    firstLength=firstString.length();  // In case it was -1.
    // secondLength==0: Nothing to do, and n2wi->normalizeAndAppend(NULL, NULL, buffer, ...) would crash.
    if(secondLength!=0) {
        const Normalizer2 *n2=(const Normalizer2 *)norm2;
        const Normalizer2WithImpl *n2wi=dynamic_cast<const Normalizer2WithImpl *>(n2);
        if(n2wi!=NULL) {
            // Avoid duplicate argument checking and support NUL-terminated src.
            UnicodeString safeMiddle;
            {
                ReorderingBuffer buffer(n2wi->impl, firstString);
                if(buffer.init(firstLength+secondLength+1, *pErrorCode)) {  // destCapacity>=-1
                    n2wi->normalizeAndAppend(second, secondLength>=0 ? second+secondLength : NULL,
                                             doNormalize, safeMiddle, buffer, *pErrorCode);
                }
            }  // The ReorderingBuffer destructor finalizes firstString.
            if(U_FAILURE(*pErrorCode) || firstString.length()>firstCapacity) {
                // Restore the modified suffix of the first string.
                // This does not restore first[] array contents between firstLength and firstCapacity.
                // (That might be uninitialized memory, as far as we know.)
                if(first!=NULL) { /* don't dereference NULL */
                  safeMiddle.extract(0, 0x7fffffff, first+firstLength-safeMiddle.length());
                  if(firstLength<firstCapacity) {
                    first[firstLength]=0;  // NUL-terminate in case it was originally.
                  }
                }
            }
        } else {
            UnicodeString secondString(secondLength<0, second, secondLength);
            if(doNormalize) {
                n2->normalizeSecondAndAppend(firstString, secondString, *pErrorCode);
            } else {
                n2->append(firstString, secondString, *pErrorCode);
            }
        }
    }
    return firstString.extract(first, firstCapacity, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
unorm2_normalizeSecondAndAppend(const UNormalizer2 *norm2,
                                UChar *first, int32_t firstLength, int32_t firstCapacity,
                                const UChar *second, int32_t secondLength,
                                UErrorCode *pErrorCode) {
    return normalizeSecondAndAppend(norm2,
                                    first, firstLength, firstCapacity,
                                    second, secondLength,
                                    TRUE, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
unorm2_append(const UNormalizer2 *norm2,
              UChar *first, int32_t firstLength, int32_t firstCapacity,
              const UChar *second, int32_t secondLength,
              UErrorCode *pErrorCode) {
    return normalizeSecondAndAppend(norm2,
                                    first, firstLength, firstCapacity,
                                    second, secondLength,
                                    FALSE, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
unorm2_getDecomposition(const UNormalizer2 *norm2,
                        UChar32 c, UChar *decomposition, int32_t capacity,
                        UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(decomposition==NULL ? capacity!=0 : capacity<0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString destString(decomposition, 0, capacity);
    if(reinterpret_cast<const Normalizer2 *>(norm2)->getDecomposition(c, destString)) {
        return destString.extract(decomposition, capacity, *pErrorCode);
    } else {
        return -1;
    }
}

U_CAPI int32_t U_EXPORT2
unorm2_getRawDecomposition(const UNormalizer2 *norm2,
                           UChar32 c, UChar *decomposition, int32_t capacity,
                           UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(decomposition==NULL ? capacity!=0 : capacity<0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString destString(decomposition, 0, capacity);
    if(reinterpret_cast<const Normalizer2 *>(norm2)->getRawDecomposition(c, destString)) {
        return destString.extract(decomposition, capacity, *pErrorCode);
    } else {
        return -1;
    }
}

U_CAPI UChar32 U_EXPORT2
unorm2_composePair(const UNormalizer2 *norm2, UChar32 a, UChar32 b) {
    return reinterpret_cast<const Normalizer2 *>(norm2)->composePair(a, b);
}

U_CAPI uint8_t U_EXPORT2
unorm2_getCombiningClass(const UNormalizer2 *norm2, UChar32 c) {
    return reinterpret_cast<const Normalizer2 *>(norm2)->getCombiningClass(c);
}

U_CAPI UBool U_EXPORT2
unorm2_isNormalized(const UNormalizer2 *norm2,
                    const UChar *s, int32_t length,
                    UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if((s==NULL && length!=0) || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString sString(length<0, s, length);
    return ((const Normalizer2 *)norm2)->isNormalized(sString, *pErrorCode);
}

U_CAPI UNormalizationCheckResult U_EXPORT2
unorm2_quickCheck(const UNormalizer2 *norm2,
                  const UChar *s, int32_t length,
                  UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return UNORM_NO;
    }
    if((s==NULL && length!=0) || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return UNORM_NO;
    }
    UnicodeString sString(length<0, s, length);
    return ((const Normalizer2 *)norm2)->quickCheck(sString, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
unorm2_spanQuickCheckYes(const UNormalizer2 *norm2,
                         const UChar *s, int32_t length,
                         UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if((s==NULL && length!=0) || length<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString sString(length<0, s, length);
    return ((const Normalizer2 *)norm2)->spanQuickCheckYes(sString, *pErrorCode);
}

U_CAPI UBool U_EXPORT2
unorm2_hasBoundaryBefore(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->hasBoundaryBefore(c);
}

U_CAPI UBool U_EXPORT2
unorm2_hasBoundaryAfter(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->hasBoundaryAfter(c);
}

U_CAPI UBool U_EXPORT2
unorm2_isInert(const UNormalizer2 *norm2, UChar32 c) {
    return ((const Normalizer2 *)norm2)->isInert(c);
}

// Some properties APIs ---------------------------------------------------- ***

U_CAPI uint8_t U_EXPORT2
u_getCombiningClass(UChar32 c) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2 *nfd=Normalizer2Factory::getNFDInstance(errorCode);
    if(U_SUCCESS(errorCode)) {
        return nfd->getCombiningClass(c);
    } else {
        return 0;
    }
}

U_CFUNC UNormalizationCheckResult
unorm_getQuickCheck(UChar32 c, UNormalizationMode mode) {
    if(mode<=UNORM_NONE || UNORM_FCD<=mode) {
        return UNORM_YES;
    }
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2 *norm2=Normalizer2Factory::getInstance(mode, errorCode);
    if(U_SUCCESS(errorCode)) {
        return ((const Normalizer2WithImpl *)norm2)->getQuickCheck(c);
    } else {
        return UNORM_MAYBE;
    }
}

U_CFUNC uint16_t
unorm_getFCD16(UChar32 c) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2Impl *impl=Normalizer2Factory::getNFCImpl(errorCode);
    if(U_SUCCESS(errorCode)) {
        return impl->getFCD16(c);
    } else {
        return 0;
    }
}

#endif  // !UCONFIG_NO_NORMALIZATION
