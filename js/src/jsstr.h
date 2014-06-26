/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsstr_h
#define jsstr_h

#include "mozilla/HashFunctions.h"
#include "mozilla/PodOperations.h"

#include "jsutil.h"
#include "NamespaceImports.h"

#include "gc/Rooting.h"
#include "js/RootingAPI.h"
#include "vm/Unicode.h"

class JSAutoByteString;
class JSFlatString;
class JSLinearString;

namespace js {

class StringBuffer;

class MutatingRopeSegmentRange;

template <AllowGC allowGC>
extern JSString *
ConcatStrings(ThreadSafeContext *cx,
              typename MaybeRooted<JSString*, allowGC>::HandleType left,
              typename MaybeRooted<JSString*, allowGC>::HandleType right);

// Return s advanced past any Unicode white space characters.
template <typename CharT>
static inline const CharT *
SkipSpace(const CharT *s, const CharT *end)
{
    JS_ASSERT(s <= end);

    while (s < end && unicode::IsSpace(*s))
        s++;

    return s;
}

// Return less than, equal to, or greater than zero depending on whether
// s1 is less than, equal to, or greater than s2.
template <typename Char1, typename Char2>
inline int32_t
CompareChars(const Char1 *s1, size_t len1, const Char2 *s2, size_t len2)
{
    size_t n = Min(len1, len2);
    for (size_t i = 0; i < n; i++) {
        if (int32_t cmp = s1[i] - s2[i])
            return cmp;
    }

    return int32_t(len1 - len2);
}

extern int32_t
CompareChars(const jschar *s1, size_t len1, JSLinearString *s2);

}  /* namespace js */

struct JSSubString {
    JSLinearString  *base;
    size_t          offset;
    size_t          length;

    JSSubString() { mozilla::PodZero(this); }

    void initEmpty(JSLinearString *base) {
        this->base = base;
        offset = length = 0;
    }
    void init(JSLinearString *base, size_t offset, size_t length) {
        this->base = base;
        this->offset = offset;
        this->length = length;
    }
};

/*
 * Shorthands for ASCII (7-bit) decimal and hex conversion.
 * Manually inline isdigit for performance; MSVC doesn't do this for us.
 */
#define JS7_ISDEC(c)    ((((unsigned)(c)) - '0') <= 9)
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (unsigned)(JS7_ISDEC(c) ? (c) - '0' : 10 + tolower(c) - 'a')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

/* Initialize the String class, returning its prototype object. */
extern JSObject *
js_InitStringClass(JSContext *cx, js::HandleObject obj);

/*
 * Convert a value to a printable C string.
 */
extern const char *
js_ValueToPrintable(JSContext *cx, const js::Value &,
                    JSAutoByteString *bytes, bool asSource = false);

extern size_t
js_strlen(const jschar *s);

extern int32_t
js_strcmp(const jschar *lhs, const jschar *rhs);

template <typename CharT>
extern const CharT *
js_strchr_limit(const CharT *s, jschar c, const CharT *limit);

static MOZ_ALWAYS_INLINE void
js_strncpy(jschar *dst, const jschar *src, size_t nelem)
{
    return mozilla::PodCopy(dst, src, nelem);
}

extern jschar *
js_strdup(js::ThreadSafeContext *cx, const jschar *s);

namespace js {

/* GC-allocate a string descriptor for the given malloc-allocated chars. */
template <js::AllowGC allowGC, typename CharT>
extern JSFlatString *
NewString(js::ThreadSafeContext *cx, CharT *chars, size_t length);

extern JSLinearString *
NewDependentString(JSContext *cx, JSString *base, size_t start, size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC, typename CharT>
extern JSFlatString *
NewStringCopyN(js::ThreadSafeContext *cx, const CharT *s, size_t n);

template <js::AllowGC allowGC>
inline JSFlatString *
NewStringCopyN(ThreadSafeContext *cx, const char *s, size_t n)
{
    return NewStringCopyN<allowGC>(cx, reinterpret_cast<const Latin1Char *>(s), n);
}

/* Like NewStringCopyN, but doesn't try to deflate to Latin1. */
template <js::AllowGC allowGC, typename CharT>
extern JSFlatString *
NewStringCopyNDontDeflate(js::ThreadSafeContext *cx, const CharT *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC>
inline JSFlatString *
NewStringCopyZ(js::ExclusiveContext *cx, const jschar *s)
{
    return NewStringCopyN<allowGC>(cx, s, js_strlen(s));
}

template <js::AllowGC allowGC>
inline JSFlatString *
NewStringCopyZ(js::ThreadSafeContext *cx, const char *s)
{
    return NewStringCopyN<allowGC>(cx, s, strlen(s));
}

/*
 * Convert a non-string value to a string, returning null after reporting an
 * error, otherwise returning a new string reference.
 */
template <AllowGC allowGC>
extern JSString *
ToStringSlow(ExclusiveContext *cx, typename MaybeRooted<Value, allowGC>::HandleType arg);

/*
 * Convert the given value to a string.  This method includes an inline
 * fast-path for the case where the value is already a string; if the value is
 * known not to be a string, use ToStringSlow instead.
 */
template <AllowGC allowGC>
static MOZ_ALWAYS_INLINE JSString *
ToString(JSContext *cx, JS::HandleValue v)
{
    if (v.isString())
        return v.toString();
    return ToStringSlow<allowGC>(cx, v);
}

/*
 * This function implements E-262-3 section 9.8, toString. Convert the given
 * value to a string of jschars appended to the given buffer. On error, the
 * passed buffer may have partial results appended.
 */
inline bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb);

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JSString *
ValueToSource(JSContext *cx, HandleValue v);

/*
 * Convert a JSString to its source expression; returns null after reporting an
 * error, otherwise returns a new string reference. No Handle needed since the
 * input is dead after the GC.
 */
extern JSString *
StringToSource(JSContext *cx, JSString *str);

/*
 * Test if strings are equal. The caller can call the function even if str1
 * or str2 are not GC-allocated things.
 */
extern bool
EqualStrings(JSContext *cx, JSString *str1, JSString *str2, bool *result);

/* Use the infallible method instead! */
extern bool
EqualStrings(JSContext *cx, JSLinearString *str1, JSLinearString *str2, bool *result) MOZ_DELETE;

/* EqualStrings is infallible on linear strings. */
extern bool
EqualStrings(JSLinearString *str1, JSLinearString *str2);

extern bool
EqualChars(JSLinearString *str1, JSLinearString *str2);

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern bool
CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32_t *result);

extern int32_t
CompareAtoms(JSAtom *atom1, JSAtom *atom2);

/*
 * Return true if the string matches the given sequence of ASCII bytes.
 */
extern bool
StringEqualsAscii(JSLinearString *str, const char *asciiBytes);

/* Return true if the string contains a pattern anywhere inside it. */
extern bool
StringHasPattern(JSLinearString *text, const jschar *pat, uint32_t patlen);

extern int
StringFindPattern(JSLinearString *text, JSLinearString *pat, size_t start);

extern bool
StringHasRegExpMetaChars(JSLinearString *str);

template <typename Char1, typename Char2>
inline bool
EqualChars(const Char1 *s1, const Char2 *s2, size_t len);

template <typename Char1>
inline bool
EqualChars(const Char1 *s1, const Char1 *s2, size_t len)
{
    return mozilla::PodEqual(s1, s2, len);
}

template <typename Char1, typename Char2>
inline bool
EqualChars(const Char1 *s1, const Char2 *s2, size_t len)
{
    for (const Char1 *s1end = s1 + len; s1 < s1end; s1++, s2++) {
        if (*s1 != *s2)
            return false;
    }
    return true;
}

/*
 * Inflate bytes in ASCII encoding to jschars. Return null on error, otherwise
 * return the jschar that was malloc'ed. length is updated to the length of the
 * new string (in jschars). A null char is appended, but it is not included in
 * the length.
 */
extern jschar *
InflateString(ThreadSafeContext *cx, const char *bytes, size_t *length);

/*
 * Inflate bytes to JS chars in an existing buffer. 'dst' must be large
 * enough for 'srclen' jschars. The buffer is NOT null-terminated.
 */
inline void
CopyAndInflateChars(jschar *dst, const char *src, size_t srclen)
{
    for (size_t i = 0; i < srclen; i++)
        dst[i] = (unsigned char) src[i];
}

inline void
CopyAndInflateChars(jschar *dst, const JS::Latin1Char *src, size_t srclen)
{
    for (size_t i = 0; i < srclen; i++)
        dst[i] = src[i];
}

/*
 * Deflate JS chars to bytes into a buffer. 'bytes' must be large enough for
 * 'length chars. The buffer is NOT null-terminated. The destination length
 * must to be initialized with the buffer size and will contain on return the
 * number of copied bytes.
 */
extern bool
DeflateStringToBuffer(JSContext *maybecx, const jschar *chars,
                      size_t charsLength, char *bytes, size_t *length);

/*
 * The String.prototype.replace fast-native entry point is exported for joined
 * function optimization in js{interp,tracer}.cpp.
 */
extern bool
str_replace(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
str_fromCharCode(JSContext *cx, unsigned argc, Value *vp);

extern bool
str_fromCharCode_one_arg(JSContext *cx, HandleValue code, MutableHandleValue rval);

} /* namespace js */

extern bool
js_str_toString(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
js_str_charAt(JSContext *cx, unsigned argc, js::Value *vp);

namespace js {

extern bool
str_charCodeAt_impl(JSContext *cx, HandleString string, HandleValue index, MutableHandleValue res);

} /* namespace js */

extern bool
js_str_charCodeAt(JSContext *cx, unsigned argc, js::Value *vp);
/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 6 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern int
js_OneUcs4ToUtf8Char(uint8_t *utf8Buffer, uint32_t ucs4Char);

namespace js {

extern size_t
PutEscapedStringImpl(char *buffer, size_t size, FILE *fp, JSLinearString *str, uint32_t quote);

template <typename CharT>
extern size_t
PutEscapedStringImpl(char *buffer, size_t bufferSize, FILE *fp, const CharT *chars,
                     size_t length, uint32_t quote);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character
 * using \escapes for JS string literals.
 * Guarantees that a NUL is at the end of the buffer unless size is 0. Returns
 * the length of the written output, NOT including the NUL. Thus, a return
 * value of size or more means that the output was truncated. If buffer
 * is null, just returns the length of the output. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
*/
inline size_t
PutEscapedString(char *buffer, size_t size, JSLinearString *str, uint32_t quote)
{
    size_t n = PutEscapedStringImpl(buffer, size, nullptr, str, quote);

    /* PutEscapedStringImpl can only fail with a file. */
    JS_ASSERT(n != size_t(-1));
    return n;
}

template <typename CharT>
inline size_t
PutEscapedString(char *buffer, size_t bufferSize, const CharT *chars, size_t length, uint32_t quote)
{
    size_t n = PutEscapedStringImpl(buffer, bufferSize, nullptr, chars, length, quote);

    /* PutEscapedStringImpl can only fail with a file. */
    JS_ASSERT(n != size_t(-1));
    return n;
}

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * If quote is not 0, it must be a single or double quote character that
 * will quote the output.
*/
inline bool
FileEscapedString(FILE *fp, JSLinearString *str, uint32_t quote)
{
    return PutEscapedStringImpl(nullptr, 0, fp, str, quote) != size_t(-1);
}

bool
str_match(JSContext *cx, unsigned argc, Value *vp);

bool
str_search(JSContext *cx, unsigned argc, Value *vp);

bool
str_split(JSContext *cx, unsigned argc, Value *vp);

JSObject *
str_split_string(JSContext *cx, HandleTypeObject type, HandleString str, HandleString sep);

bool
str_resolve(JSContext *cx, HandleObject obj, HandleId id, MutableHandleObject objp);

bool
str_replace_regexp_raw(JSContext *cx, HandleString string, HandleObject regexp,
                       HandleString replacement, MutableHandleValue rval);

bool
str_replace_string_raw(JSContext *cx, HandleString string, HandleString pattern,
                       HandleString replacement, MutableHandleValue rval);

} /* namespace js */

extern bool
js_String(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsstr_h */
