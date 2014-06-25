/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_String_inl_h
#define vm_String_inl_h

#include "vm/String.h"

#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"

#include "jscntxt.h"

#include "gc/Marking.h"

#include "jsgcinlines.h"

namespace js {

template <AllowGC allowGC, typename CharT>
static MOZ_ALWAYS_INLINE JSInlineString *
AllocateFatInlineString(ThreadSafeContext *cx, size_t len, CharT **chars)
{
    MOZ_ASSERT(JSFatInlineString::lengthFits<CharT>(len));

    if (JSInlineString::lengthFits<CharT>(len)) {
        JSInlineString *str = JSInlineString::new_<allowGC>(cx);
        if (!str)
            return nullptr;
        *chars = str->init<CharT>(len);
        return str;
    }

    JSFatInlineString *str = JSFatInlineString::new_<allowGC>(cx);
    if (!str)
        return nullptr;
    *chars = str->init<CharT>(len);
    return str;
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE JSInlineString *
NewFatInlineString(ThreadSafeContext *cx, mozilla::Range<const Latin1Char> chars)
{
    size_t len = chars.length();

    if (EnableLatin1Strings) {
        Latin1Char *p;
        JSInlineString *str = AllocateFatInlineString<allowGC>(cx, len, &p);
        if (!str)
            return nullptr;

        mozilla::PodCopy(p, chars.start().get(), len);
        p[len] = '\0';
        return str;
    }

    jschar *p;
    JSInlineString *str = AllocateFatInlineString<allowGC>(cx, len, &p);
    if (!str)
        return nullptr;

    for (size_t i = 0; i < len; ++i)
        p[i] = static_cast<jschar>(chars[i]);
    p[len] = '\0';
    return str;
}

template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE JSInlineString *
NewFatInlineString(ThreadSafeContext *cx, mozilla::Range<const jschar> chars)
{
    /*
     * Don't bother trying to find a static atom; measurement shows that not
     * many get here (for one, Atomize is catching them).
     */

    size_t len = chars.length();
    jschar *storage;
    JSInlineString *str = AllocateFatInlineString<allowGC>(cx, len, &storage);
    if (!str)
        return nullptr;

    mozilla::PodCopy(storage, chars.start().get(), len);
    storage[len] = 0;
    return str;
}

template <typename CharT>
static MOZ_ALWAYS_INLINE JSInlineString *
NewFatInlineString(ExclusiveContext *cx, HandleLinearString base, size_t start, size_t length)
{
    MOZ_ASSERT(JSFatInlineString::lengthFits<CharT>(length));

    CharT *chars;
    JSInlineString *s = AllocateFatInlineString<CanGC>(cx, length, &chars);
    if (!s)
        return nullptr;

    JS::AutoCheckCannotGC nogc;
    mozilla::PodCopy(chars, base->chars<CharT>(nogc) + start, length);
    chars[length] = 0;
    return s;
}

static inline void
StringWriteBarrierPost(js::ThreadSafeContext *maybecx, JSString **strp)
{
}

static inline void
StringWriteBarrierPostRemove(js::ThreadSafeContext *maybecx, JSString **strp)
{
}

} /* namespace js */

MOZ_ALWAYS_INLINE bool
JSString::validateLength(js::ThreadSafeContext *maybecx, size_t length)
{
    if (MOZ_UNLIKELY(length > JSString::MAX_LENGTH)) {
        js_ReportAllocationOverflow(maybecx);
        return false;
    }

    return true;
}

MOZ_ALWAYS_INLINE void
JSRope::init(js::ThreadSafeContext *cx, JSString *left, JSString *right, size_t length)
{
    d.u1.length = length;
    d.u1.flags = ROPE_FLAGS;
    if (left->hasLatin1Chars() && right->hasLatin1Chars())
        d.u1.flags |= LATIN1_CHARS_BIT;
    d.s.u2.left = left;
    d.s.u3.right = right;
    js::StringWriteBarrierPost(cx, &d.s.u2.left);
    js::StringWriteBarrierPost(cx, &d.s.u3.right);
}

template <js::AllowGC allowGC>
MOZ_ALWAYS_INLINE JSRope *
JSRope::new_(js::ThreadSafeContext *cx,
             typename js::MaybeRooted<JSString*, allowGC>::HandleType left,
             typename js::MaybeRooted<JSString*, allowGC>::HandleType right,
             size_t length)
{
    if (!validateLength(cx, length))
        return nullptr;
    JSRope *str = (JSRope *)js::NewGCString<allowGC>(cx);
    if (!str)
        return nullptr;
    str->init(cx, left, right, length);
    return str;
}

inline void
JSRope::markChildren(JSTracer *trc)
{
    js::gc::MarkStringUnbarriered(trc, &d.s.u2.left, "left child");
    js::gc::MarkStringUnbarriered(trc, &d.s.u3.right, "right child");
}

MOZ_ALWAYS_INLINE void
JSDependentString::init(js::ThreadSafeContext *cx, JSLinearString *base, size_t start,
                        size_t length)
{
    MOZ_ASSERT(!js::IsPoisonedPtr(base));
    MOZ_ASSERT(start + length <= base->length());
    d.u1.length = length;
    JS::AutoCheckCannotGC nogc;
    if (base->hasLatin1Chars()) {
        d.u1.flags = DEPENDENT_FLAGS | LATIN1_CHARS_BIT;
        d.s.u2.nonInlineCharsLatin1 = base->latin1Chars(nogc) + start;
    } else {
        d.u1.flags = DEPENDENT_FLAGS;
        d.s.u2.nonInlineCharsTwoByte = base->twoByteChars(nogc) + start;
    }
    d.s.u3.base = base;
    js::StringWriteBarrierPost(cx, reinterpret_cast<JSString **>(&d.s.u3.base));
}

MOZ_ALWAYS_INLINE JSLinearString *
JSDependentString::new_(js::ExclusiveContext *cx, JSLinearString *baseArg, size_t start,
                        size_t length)
{
    /* Try to avoid long chains of dependent strings. */
    while (baseArg->isDependent()) {
        start += baseArg->asDependent().baseOffset();
        baseArg = baseArg->asDependent().base();
    }

    MOZ_ASSERT(start + length <= baseArg->length());
    MOZ_ASSERT(baseArg->isFlat());

    /*
     * Do not create a string dependent on inline chars from another string,
     * both to avoid the awkward moving-GC hazard this introduces and because it
     * is more efficient to immediately undepend here.
     */
    bool useFatInline = baseArg->hasTwoByteChars()
                        ? JSFatInlineString::twoByteLengthFits(length)
                        : JSFatInlineString::latin1LengthFits(length);
    if (useFatInline) {
        js::RootedLinearString base(cx, baseArg);
        if (baseArg->hasLatin1Chars())
            return js::NewFatInlineString<JS::Latin1Char>(cx, base, start, length);
        return js::NewFatInlineString<jschar>(cx, base, start, length);
    }

    JSDependentString *str = (JSDependentString *)js::NewGCString<js::NoGC>(cx);
    if (str) {
        str->init(cx, baseArg, start, length);
        return str;
    }

    js::RootedLinearString base(cx, baseArg);

    str = (JSDependentString *)js::NewGCString<js::CanGC>(cx);
    if (!str)
        return nullptr;
    str->init(cx, base, start, length);
    return str;
}

inline void
JSString::markBase(JSTracer *trc)
{
    JS_ASSERT(hasBase());
    js::gc::MarkStringUnbarriered(trc, &d.s.u3.base, "base");
}

MOZ_ALWAYS_INLINE void
JSFlatString::init(const jschar *chars, size_t length)
{
    d.u1.length = length;
    d.u1.flags = FLAT_BIT;
    d.s.u2.nonInlineCharsTwoByte = chars;
}

MOZ_ALWAYS_INLINE void
JSFlatString::init(const JS::Latin1Char *chars, size_t length)
{
    d.u1.length = length;
    d.u1.flags = FLAT_BIT | LATIN1_CHARS_BIT;
    d.s.u2.nonInlineCharsLatin1 = chars;
}

template <js::AllowGC allowGC, typename CharT>
MOZ_ALWAYS_INLINE JSFlatString *
JSFlatString::new_(js::ThreadSafeContext *cx, const CharT *chars, size_t length)
{
    JS_ASSERT(chars[length] == CharT(0));

    if (!validateLength(cx, length))
        return nullptr;

    JSFlatString *str = (JSFlatString *)js::NewGCString<allowGC>(cx);
    if (!str)
        return nullptr;

    str->init(chars, length);
    return str;
}

inline js::PropertyName *
JSFlatString::toPropertyName(JSContext *cx)
{
#ifdef DEBUG
    uint32_t dummy;
    JS_ASSERT(!isIndex(&dummy));
#endif
    if (isAtom())
        return asAtom().asPropertyName();
    JSAtom *atom = js::AtomizeString(cx, this);
    if (!atom)
        return nullptr;
    return atom->asPropertyName();
}

template <js::AllowGC allowGC>
MOZ_ALWAYS_INLINE JSInlineString *
JSInlineString::new_(js::ThreadSafeContext *cx)
{
    return (JSInlineString *)js::NewGCString<allowGC>(cx);
}

MOZ_ALWAYS_INLINE jschar *
JSInlineString::initTwoByte(size_t length)
{
    JS_ASSERT(twoByteLengthFits(length));
    d.u1.length = length;
    d.u1.flags = INIT_INLINE_FLAGS;
    return d.inlineStorageTwoByte;
}

MOZ_ALWAYS_INLINE JS::Latin1Char *
JSInlineString::initLatin1(size_t length)
{
    JS_ASSERT(latin1LengthFits(length));
    d.u1.length = length;
    d.u1.flags = INIT_INLINE_FLAGS | LATIN1_CHARS_BIT;
    return d.inlineStorageLatin1;
}

MOZ_ALWAYS_INLINE jschar *
JSFatInlineString::initTwoByte(size_t length)
{
    JS_ASSERT(twoByteLengthFits(length));
    d.u1.length = length;
    d.u1.flags = INIT_FAT_INLINE_FLAGS;
    return d.inlineStorageTwoByte;
}

MOZ_ALWAYS_INLINE JS::Latin1Char *
JSFatInlineString::initLatin1(size_t length)
{
    JS_ASSERT(latin1LengthFits(length));
    d.u1.length = length;
    d.u1.flags = INIT_FAT_INLINE_FLAGS | LATIN1_CHARS_BIT;
    return d.inlineStorageLatin1;
}

template<>
MOZ_ALWAYS_INLINE JS::Latin1Char *
JSInlineString::init<JS::Latin1Char>(size_t length)
{
    return initLatin1(length);
}

template<>
MOZ_ALWAYS_INLINE jschar *
JSInlineString::init<jschar>(size_t length)
{
    return initTwoByte(length);
}

template<>
MOZ_ALWAYS_INLINE JS::Latin1Char *
JSFatInlineString::init<JS::Latin1Char>(size_t length)
{
    return initLatin1(length);
}

template<>
MOZ_ALWAYS_INLINE jschar *
JSFatInlineString::init<jschar>(size_t length)
{
    return initTwoByte(length);
}

template <js::AllowGC allowGC>
MOZ_ALWAYS_INLINE JSFatInlineString *
JSFatInlineString::new_(js::ThreadSafeContext *cx)
{
    return js::NewGCFatInlineString<allowGC>(cx);
}

MOZ_ALWAYS_INLINE void
JSExternalString::init(const jschar *chars, size_t length, const JSStringFinalizer *fin)
{
    JS_ASSERT(fin);
    JS_ASSERT(fin->finalize);
    d.u1.length = length;
    d.u1.flags = EXTERNAL_FLAGS;
    d.s.u2.nonInlineCharsTwoByte = chars;
    d.s.u3.externalFinalizer = fin;
}

MOZ_ALWAYS_INLINE JSExternalString *
JSExternalString::new_(JSContext *cx, const jschar *chars, size_t length,
                       const JSStringFinalizer *fin)
{
    JS_ASSERT(chars[length] == 0);

    if (!validateLength(cx, length))
        return nullptr;
    JSExternalString *str = js::NewGCExternalString(cx);
    if (!str)
        return nullptr;
    str->init(chars, length, fin);
    cx->runtime()->updateMallocCounter(cx->zone(), (length + 1) * sizeof(jschar));
    return str;
}

inline JSLinearString *
js::StaticStrings::getUnitStringForElement(JSContext *cx, JSString *str, size_t index)
{
    JS_ASSERT(index < str->length());

    jschar c;
    if (!str->getChar(cx, index, &c))
        return nullptr;
    if (c < UNIT_STATIC_LIMIT)
        return getUnit(c);
    return NewDependentString(cx, str, index, 1);
}

inline JSAtom *
js::StaticStrings::getLength2(jschar c1, jschar c2)
{
    JS_ASSERT(fitsInSmallChar(c1));
    JS_ASSERT(fitsInSmallChar(c2));
    size_t index = (((size_t)toSmallChar[c1]) << 6) + toSmallChar[c2];
    return length2StaticTable[index];
}

MOZ_ALWAYS_INLINE void
JSString::finalize(js::FreeOp *fop)
{
    /* FatInline strings are in a different arena. */
    JS_ASSERT(getAllocKind() != js::gc::FINALIZE_FAT_INLINE_STRING);

    if (isFlat())
        asFlat().finalize(fop);
    else
        JS_ASSERT(isDependent() || isRope());
}

inline void
JSFlatString::finalize(js::FreeOp *fop)
{
    JS_ASSERT(getAllocKind() != js::gc::FINALIZE_FAT_INLINE_STRING);

    if (!isInline())
        fop->free_(nonInlineCharsRaw());
}

inline void
JSFatInlineString::finalize(js::FreeOp *fop)
{
    JS_ASSERT(getAllocKind() == js::gc::FINALIZE_FAT_INLINE_STRING);

    if (!isInline())
        fop->free_(nonInlineCharsRaw());
}

inline void
JSAtom::finalize(js::FreeOp *fop)
{
    JS_ASSERT(JSString::isAtom());
    JS_ASSERT(JSString::isFlat());

    if (!isInline())
        fop->free_(nonInlineCharsRaw());
}

inline void
JSExternalString::finalize(js::FreeOp *fop)
{
    const JSStringFinalizer *fin = externalFinalizer();
    fin->finalize(fin, const_cast<jschar *>(nonInlineChars()));
}

#endif /* vm_String_inl_h */
