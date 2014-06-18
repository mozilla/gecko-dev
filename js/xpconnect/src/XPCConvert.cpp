/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Data conversion between native and JavaScript types. */

#include "mozilla/ArrayUtils.h"

#include "xpcprivate.h"
#include "nsIAtom.h"
#include "nsWrapperCache.h"
#include "nsJSUtils.h"
#include "WrapperFactory.h"

#include "nsWrapperCacheInlines.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "JavaScriptParent.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/PrimitiveConversions.h"

using namespace xpc;
using namespace mozilla;
using namespace mozilla::dom;
using namespace JS;

//#define STRICT_CHECK_OF_UNICODE
#ifdef STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF80))
#else // STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF00))
#endif // STRICT_CHECK_OF_UNICODE

#define ILLEGAL_CHAR_RANGE(c) (0!=((c) & 0x80))

/***********************************************************/

// static
bool
XPCConvert::IsMethodReflectable(const XPTMethodDescriptor& info)
{
    if (XPT_MD_IS_NOTXPCOM(info.flags) || XPT_MD_IS_HIDDEN(info.flags))
        return false;

    for (int i = info.num_args-1; i >= 0; i--) {
        const nsXPTParamInfo& param = info.params[i];
        const nsXPTType& type = param.GetType();

        // Reflected methods can't use native types. All native types end up
        // getting tagged as void*, so this check is easy.
        if (type.TagPart() == nsXPTType::T_VOID)
            return false;
    }
    return true;
}

static JSObject*
UnwrapNativeCPOW(nsISupports* wrapper)
{
    nsCOMPtr<nsIXPConnectWrappedJS> underware = do_QueryInterface(wrapper);
    if (underware) {
        JSObject* mainObj = underware->GetJSObject();
        if (mainObj && mozilla::jsipc::IsCPOW(mainObj))
            return mainObj;
    }
    return nullptr;
}

/***************************************************************************/

// static
bool
XPCConvert::GetISupportsFromJSObject(JSObject* obj, nsISupports** iface)
{
    const JSClass* jsclass = js::GetObjectJSClass(obj);
    MOZ_ASSERT(jsclass, "obj has no class");
    if (jsclass &&
        (jsclass->flags & JSCLASS_HAS_PRIVATE) &&
        (jsclass->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
        *iface = (nsISupports*) xpc_GetJSPrivate(obj);
        return true;
    }
    *iface = UnwrapDOMObjectToISupports(obj);
    return !!*iface;
}

/***************************************************************************/

// static
bool
XPCConvert::NativeData2JS(MutableHandleValue d, const void* s,
                          const nsXPTType& type, const nsID* iid, nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");

    AutoJSContext cx;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    switch (type.TagPart()) {
    case nsXPTType::T_I8    :
        d.setInt32(*static_cast<const int8_t*>(s));
        return true;
    case nsXPTType::T_I16   :
        d.setInt32(*static_cast<const int16_t*>(s));
        return true;
    case nsXPTType::T_I32   :
        d.setInt32(*static_cast<const int32_t*>(s));
        return true;
    case nsXPTType::T_I64   :
        d.setNumber(static_cast<double>(*static_cast<const int64_t*>(s)));
        return true;
    case nsXPTType::T_U8    :
        d.setInt32(*static_cast<const uint8_t*>(s));
        return true;
    case nsXPTType::T_U16   :
        d.setInt32(*static_cast<const uint16_t*>(s));
        return true;
    case nsXPTType::T_U32   :
        d.setNumber(*static_cast<const uint32_t*>(s));
        return true;
    case nsXPTType::T_U64   :
        d.setNumber(static_cast<double>(*static_cast<const uint64_t*>(s)));
        return true;
    case nsXPTType::T_FLOAT :
        d.setNumber(*static_cast<const float*>(s));
        return true;
    case nsXPTType::T_DOUBLE:
        d.setNumber(*static_cast<const double*>(s));
        return true;
    case nsXPTType::T_BOOL  :
        d.setBoolean(*static_cast<const bool*>(s));
        return true;
    case nsXPTType::T_CHAR  :
    {
        char p = *static_cast<const char*>(s);

#ifdef STRICT_CHECK_OF_UNICODE
        MOZ_ASSERT(! ILLEGAL_CHAR_RANGE(p) , "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE

        JSString* str = JS_NewStringCopyN(cx, &p, 1);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }
    case nsXPTType::T_WCHAR :
    {
        jschar p = *static_cast<const jschar*>(s);

        JSString* str = JS_NewUCStringCopyN(cx, &p, 1);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_JSVAL :
    {
        d.set(*static_cast<const Value*>(s));
        return JS_WrapValue(cx, d);
    }

    case nsXPTType::T_VOID:
        XPC_LOG_ERROR(("XPCConvert::NativeData2JS : void* params not supported"));
        return false;

    case nsXPTType::T_IID:
    {
        nsID* iid2 = *static_cast<nsID* const *>(s);
        if (!iid2) {
            d.setNull();
            return true;
        }

        RootedObject scope(cx, JS::CurrentGlobalOrNull(cx));
        JSObject* obj = xpc_NewIDObject(cx, scope, *iid2);
        if (!obj)
            return false;

        d.setObject(*obj);
        return true;
    }

    case nsXPTType::T_ASTRING:
        // Fall through to T_DOMSTRING case

    case nsXPTType::T_DOMSTRING:
    {
        const nsAString* p = *static_cast<const nsAString* const *>(s);
        if (!p || p->IsVoid()) {
            d.setNull();
            return true;
        }

        nsStringBuffer* buf;
        if (!XPCStringConvert::ReadableToJSVal(cx, *p, &buf, d))
            return false;
        if (buf)
            buf->AddRef();
        return true;
    }

    case nsXPTType::T_CHAR_STR:
    {
        const char* p = *static_cast<const char* const *>(s);
        if (!p) {
            d.setNull();
            return true;
        }

#ifdef STRICT_CHECK_OF_UNICODE
        bool isAscii = true;
        for (char* t = p; *t && isAscii; t++) {
          if (ILLEGAL_CHAR_RANGE(*t))
              isAscii = false;
        }
        MOZ_ASSERT(isAscii, "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE

        JSString* str = JS_NewStringCopyZ(cx, p);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_WCHAR_STR:
    {
        const jschar* p = *static_cast<const jschar* const *>(s);
        if (!p) {
            d.setNull();
            return true;
        }

        JSString* str = JS_NewUCStringCopyZ(cx, p);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }
    case nsXPTType::T_UTF8STRING:
    {
        const nsACString* utf8String = *static_cast<const nsACString* const *>(s);

        if (!utf8String || utf8String->IsVoid()) {
            d.setNull();
            return true;
        }

        if (utf8String->IsEmpty()) {
            d.set(JS_GetEmptyStringValue(cx));
            return true;
        }

        const uint32_t len = CalcUTF8ToUnicodeLength(*utf8String);
        // The cString is not empty at this point, but the calculated
        // UTF-16 length is zero, meaning no valid conversion exists.
        if (!len)
            return false;

        const size_t buffer_size = (len + 1) * sizeof(char16_t);
        char16_t* buffer =
            static_cast<char16_t*>(JS_malloc(cx, buffer_size));
        if (!buffer)
            return false;

        uint32_t copied;
        if (!UTF8ToUnicodeBuffer(*utf8String, buffer, &copied) ||
            len != copied) {
            // Copy or conversion during copy failed. Did not copy the
            // whole string.
            JS_free(cx, buffer);
            return false;
        }

        // JS_NewUCString takes ownership on success, i.e. a
        // successful call will make it the responsiblity of the JS VM
        // to free the buffer.
        JSString* str = JS_NewUCString(cx, buffer, len);
        if (!str) {
            JS_free(cx, buffer);
            return false;
        }

        d.setString(str);
        return true;
    }
    case nsXPTType::T_CSTRING:
    {
        const nsACString* cString = *static_cast<const nsACString* const *>(s);

        if (!cString || cString->IsVoid()) {
            d.setNull();
            return true;
        }

        // c-strings (binary blobs) are deliberately not converted from
        // UTF-8 to UTF-16. T_UTF8Sting is for UTF-8 encoded strings
        // with automatic conversion.
        JSString* str = JS_NewStringCopyN(cx, cString->Data(),
                                          cString->Length());
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
        nsISupports* iface = *static_cast<nsISupports* const *>(s);
        if (!iface) {
            d.setNull();
            return true;
        }

        if (iid->Equals(NS_GET_IID(nsIVariant))) {
            nsCOMPtr<nsIVariant> variant = do_QueryInterface(iface);
            if (!variant)
                return false;

            return XPCVariant::VariantDataToJS(variant,
                                               pErr, d);
        }

        xpcObjectHelper helper(iface);
        return NativeInterface2JSObject(d, nullptr, helper, iid, nullptr, true, pErr);
    }

    default:
        NS_ERROR("bad type");
        return false;
    }
    return true;
}

/***************************************************************************/

#ifdef DEBUG
static bool
CheckJSCharInCharRange(jschar c)
{
    if (ILLEGAL_RANGE(c)) {
        /* U+0080/U+0100 - U+FFFF data lost. */
        static const size_t MSG_BUF_SIZE = 64;
        char msg[MSG_BUF_SIZE];
        JS_snprintf(msg, MSG_BUF_SIZE, "jschar out of char range; high bits of data lost: 0x%x", c);
        NS_WARNING(msg);
        return false;
    }

    return true;
}
#endif

template<typename T>
bool ConvertToPrimitive(JSContext *cx, HandleValue v, T *retval)
{
    return ValueToPrimitive<T, eDefault>(cx, v, retval);
}

// static
bool
XPCConvert::JSData2Native(void* d, HandleValue s,
                          const nsXPTType& type,
                          bool useAllocator, const nsID* iid,
                          nsresult* pErr)
{
    NS_PRECONDITION(d, "bad param");

    AutoJSContext cx;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    switch (type.TagPart()) {
    case nsXPTType::T_I8     :
        return ConvertToPrimitive(cx, s, static_cast<int8_t*>(d));
    case nsXPTType::T_I16    :
        return ConvertToPrimitive(cx, s, static_cast<int16_t*>(d));
    case nsXPTType::T_I32    :
        return ConvertToPrimitive(cx, s, static_cast<int32_t*>(d));
    case nsXPTType::T_I64    :
        return ConvertToPrimitive(cx, s, static_cast<int64_t*>(d));
    case nsXPTType::T_U8     :
        return ConvertToPrimitive(cx, s, static_cast<uint8_t*>(d));
    case nsXPTType::T_U16    :
        return ConvertToPrimitive(cx, s, static_cast<uint16_t*>(d));
    case nsXPTType::T_U32    :
        return ConvertToPrimitive(cx, s, static_cast<uint32_t*>(d));
    case nsXPTType::T_U64    :
        return ConvertToPrimitive(cx, s, static_cast<uint64_t*>(d));
    case nsXPTType::T_FLOAT  :
        return ConvertToPrimitive(cx, s, static_cast<float*>(d));
    case nsXPTType::T_DOUBLE :
        return ConvertToPrimitive(cx, s, static_cast<double*>(d));
    case nsXPTType::T_BOOL   :
        return ConvertToPrimitive(cx, s, static_cast<bool*>(d));
    case nsXPTType::T_CHAR   :
    {
        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }
        size_t length;
        const jschar* chars = JS_GetStringCharsAndLength(cx, str, &length);
        if (!chars) {
            return false;
        }
        jschar ch = length ? chars[0] : 0;
#ifdef DEBUG
        CheckJSCharInCharRange(ch);
#endif
        *((char*)d) = char(ch);
        break;
    }
    case nsXPTType::T_WCHAR  :
    {
        JSString* str;
        if (!(str = ToString(cx, s))) {
            return false;
        }
        size_t length;
        const jschar* chars = JS_GetStringCharsAndLength(cx, str, &length);
        if (!chars) {
            return false;
        }
        if (length == 0) {
            *((uint16_t*)d) = 0;
            break;
        }
        *((uint16_t*)d) = uint16_t(chars[0]);
        break;
    }
    case nsXPTType::T_JSVAL :
        *((jsval*)d) = s;
        break;
    case nsXPTType::T_VOID:
        XPC_LOG_ERROR(("XPCConvert::JSData2Native : void* params not supported"));
        NS_ERROR("void* params not supported");
        return false;
    case nsXPTType::T_IID:
    {
        const nsID* pid = nullptr;

        // There's no good reason to pass a null IID.
        if (s.isNullOrUndefined()) {
            if (pErr)
                *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;
            return false;
        }

        if (!s.isObject() ||
            (!(pid = xpc_JSObjectToID(cx, &s.toObject()))) ||
            (!(pid = (const nsID*) nsMemory::Clone(pid, sizeof(nsID))))) {
            return false;
        }
        *((const nsID**)d) = pid;
        return true;
    }

    case nsXPTType::T_ASTRING:
    {
        if (s.isUndefined()) {
            if (useAllocator)
                *((const nsAString**)d) = &NullString();
            else
                (**((nsAString**)d)).SetIsVoid(true);
            return true;
        }
        // Fall through to T_DOMSTRING case.
    }
    case nsXPTType::T_DOMSTRING:
    {
        if (s.isNull()) {
            if (useAllocator)
                *((const nsAString**)d) = &NullString();
            else
                (**((nsAString**)d)).SetIsVoid(true);
            return true;
        }
        size_t length = 0;
        const char16_t* chars = nullptr;
        JSString* str = nullptr;
        if (!s.isUndefined()) {
            str = ToString(cx, s);
            if (!str)
                return false;

            chars = useAllocator ? JS_GetStringCharsZAndLength(cx, str, &length)
                                 : JS_GetStringCharsAndLength(cx, str, &length);
            if (!chars)
                return false;

            if (!length) {
                if (useAllocator)
                    *((const nsAString**)d) = &EmptyString();
                else
                    (**((nsAString**)d)).Truncate();
                return true;
            }
        }

        nsString* ws;
        if (useAllocator) {
            ws = nsXPConnect::GetRuntimeInstance()->NewShortLivedString();
            *((const nsString**)d) = ws;
        } else {
            ws = *((nsString**)d);
        }

        if (!str) {
            ws->AssignLiteral(MOZ_UTF16("undefined"));
        } else if (XPCStringConvert::IsDOMString(str)) {
            // The characters represent an existing nsStringBuffer that
            // was shared by XPCStringConvert::ReadableToJSVal.
            nsStringBuffer::FromData((void *)chars)->ToString(length, *ws);
        } else if (XPCStringConvert::IsLiteral(str)) {
            // The characters represent a literal char16_t string constant
            // compiled into libxul, such as the string "undefined" above.
            ws->AssignLiteral(chars, length);
        } else if (useAllocator && STRING_TO_JSVAL(str) == s) {
            // The JS string will exist over the function call.
            // We don't need to copy the characters in this case.
            ws->Rebind(chars, length);
        } else {
            ws->Assign(chars, length);
        }
        return true;
    }

    case nsXPTType::T_CHAR_STR:
    {
        if (s.isUndefined() || s.isNull()) {
            *((char**)d) = nullptr;
            return true;
        }

        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }
#ifdef DEBUG
        const jschar* chars=nullptr;
        if (nullptr != (chars = JS_GetStringCharsZ(cx, str))) {
            bool legalRange = true;
            int len = JS_GetStringLength(str);
            const jschar* t;
            int32_t i=0;
            for (t=chars; (i< len) && legalRange ; i++,t++) {
                if (!CheckJSCharInCharRange(*t))
                    break;
            }
        }
#endif // DEBUG
        size_t length = JS_GetStringEncodingLength(cx, str);
        if (length == size_t(-1)) {
            return false;
        }
        char *buffer = static_cast<char *>(nsMemory::Alloc(length + 1));
        if (!buffer) {
            return false;
        }
        JS_EncodeStringToBuffer(cx, str, buffer, length);
        buffer[length] = '\0';
        *((void**)d) = buffer;
        return true;
    }

    case nsXPTType::T_WCHAR_STR:
    {
        const jschar* chars=nullptr;
        JSString* str;

        if (s.isUndefined() || s.isNull()) {
            *((jschar**)d) = nullptr;
            return true;
        }

        if (!(str = ToString(cx, s))) {
            return false;
        }
        if (!(chars = JS_GetStringCharsZ(cx, str))) {
            return false;
        }
        int len = JS_GetStringLength(str);
        int byte_len = (len+1)*sizeof(jschar);
        if (!(*((void**)d) = nsMemory::Alloc(byte_len))) {
            // XXX should report error
            return false;
        }
        jschar* destchars = *((jschar**)d);
        memcpy(destchars, chars, byte_len);
        destchars[len] = 0;

        return true;
    }

    case nsXPTType::T_UTF8STRING:
    {
        const jschar* chars;
        size_t length;
        JSString* str;

        if (s.isNull() || s.isUndefined()) {
            if (useAllocator) {
                *((const nsACString**)d) = &NullCString();
            } else {
                nsCString* rs = *((nsCString**)d);
                rs->SetIsVoid(true);
            }
            return true;
        }

        // The JS val is neither null nor void...

        if (!(str = ToString(cx, s))||
            !(chars = JS_GetStringCharsAndLength(cx, str, &length))) {
            return false;
        }

        if (!length) {
            if (useAllocator) {
                *((const nsACString**)d) = &EmptyCString();
            } else {
                nsCString* rs = *((nsCString**)d);
                rs->Truncate();
            }
            return true;
        }

        nsCString *rs;
        if (useAllocator) {
            // Use nsCString to enable sharing
            rs = new nsCString();
            if (!rs)
                return false;

            *((const nsCString**)d) = rs;
        } else {
            rs = *((nsCString**)d);
        }
        CopyUTF16toUTF8(Substring(chars, length), *rs);
        return true;
    }

    case nsXPTType::T_CSTRING:
    {
        if (s.isNull() || s.isUndefined()) {
            if (useAllocator) {
                nsACString *rs = new nsCString();
                if (!rs)
                    return false;

                rs->SetIsVoid(true);
                *((nsACString**)d) = rs;
            } else {
                nsACString* rs = *((nsACString**)d);
                rs->Truncate();
                rs->SetIsVoid(true);
            }
            return true;
        }

        // The JS val is neither null nor void...
        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }

        size_t length = JS_GetStringEncodingLength(cx, str);
        if (length == size_t(-1)) {
            return false;
        }

        if (!length) {
            if (useAllocator) {
                *((const nsACString**)d) = &EmptyCString();
            } else {
                nsCString* rs = *((nsCString**)d);
                rs->Truncate();
            }
            return true;
        }

        nsACString *rs;
        if (useAllocator) {
            rs = new nsCString();
            if (!rs)
                return false;
            *((const nsACString**)d) = rs;
        } else {
            rs = *((nsACString**)d);
        }

        rs->SetLength(uint32_t(length));
        if (rs->Length() != uint32_t(length)) {
            return false;
        }
        JS_EncodeStringToBuffer(cx, str, rs->BeginWriting(), length);

        return true;
    }

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
        MOZ_ASSERT(iid,"can't do interface conversions without iid");

        if (iid->Equals(NS_GET_IID(nsIVariant))) {
            nsCOMPtr<nsIVariant> variant = XPCVariant::newVariant(cx, s);
            if (!variant)
                return false;

            variant.forget(static_cast<nsISupports**>(d));
            return true;
        } else if (iid->Equals(NS_GET_IID(nsIAtom)) && s.isString()) {
            // We're trying to pass a string as an nsIAtom.  Let's atomize!
            JSString* str = s.toString();
            const char16_t* chars = JS_GetStringCharsZ(cx, str);
            if (!chars) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF;
                return false;
            }
            uint32_t length = JS_GetStringLength(str);
            nsCOMPtr<nsIAtom> atom =
                NS_NewAtom(nsDependentSubstring(chars, chars + length));
            atom.forget((nsISupports**)d);
            return true;
        }
        //else ...

        if (s.isNullOrUndefined()) {
            *((nsISupports**)d) = nullptr;
            return true;
        }

        // only wrap JSObjects
        if (!s.isObject()) {
            if (pErr && s.isInt32() && 0 == s.toInt32())
                *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL;
            return false;
        }

        RootedObject src(cx, &s.toObject());
        return JSObject2NativeInterface((void**)d, src, iid, nullptr, pErr);
    }
    default:
        NS_ERROR("bad type");
        return false;
    }
    return true;
}

static inline bool
CreateHolderIfNeeded(HandleObject obj, MutableHandleValue d,
                     nsIXPConnectJSObjectHolder** dest)
{
    if (dest) {
        nsRefPtr<XPCJSObjectHolder> objHolder = XPCJSObjectHolder::newHolder(obj);
        if (!objHolder)
            return false;

        objHolder.forget(dest);
    }

    d.setObjectOrNull(obj);

    return true;
}

/***************************************************************************/
// static
bool
XPCConvert::NativeInterface2JSObject(MutableHandleValue d,
                                     nsIXPConnectJSObjectHolder** dest,
                                     xpcObjectHelper& aHelper,
                                     const nsID* iid,
                                     XPCNativeInterface** Interface,
                                     bool allowNativeWrapper,
                                     nsresult* pErr)
{
    MOZ_ASSERT_IF(Interface, iid);
    if (!iid)
        iid = &NS_GET_IID(nsISupports);

    d.setNull();
    if (dest)
        *dest = nullptr;
    if (!aHelper.Object())
        return true;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    // We used to have code here that unwrapped and simply exposed the
    // underlying JSObject. That caused anomolies when JSComponents were
    // accessed from other JS code - they didn't act like other xpconnect
    // wrapped components. So, instead, we create "double wrapped" objects
    // (that means an XPCWrappedNative around an nsXPCWrappedJS). This isn't
    // optimal -- we could detect this and roll the functionality into a
    // single wrapper, but the current solution is good enough for now.
    AutoJSContext cx;
    XPCWrappedNativeScope* xpcscope = GetObjectScope(JS::CurrentGlobalOrNull(cx));
    if (!xpcscope)
        return false;

    // First, see if this object supports the wrapper cache.
    // Note: If |cache->IsDOMBinding()| is true, then it means that the object
    // implementing it doesn't want a wrapped native as its JS Object, but
    // instead it provides its own proxy object. In that case, the object
    // to use is found as cache->GetWrapper(). If that is null, then the
    // object will create (and fill the cache) from its WrapObject call.
    nsWrapperCache *cache = aHelper.GetWrapperCache();

    RootedObject flat(cx, cache ? cache->GetWrapper() : nullptr);
    if (!flat && cache && cache->IsDOMBinding()) {
        RootedObject global(cx, xpcscope->GetGlobalJSObject());
        js::AssertSameCompartment(cx, global);
        flat = cache->WrapObject(cx);
        if (!flat)
            return false;
    }
    if (flat) {
        if (allowNativeWrapper && !JS_WrapObject(cx, &flat))
            return false;
        return CreateHolderIfNeeded(flat, d, dest);
    }

    // Don't double wrap CPOWs. This is a temporary measure for compatibility
    // with objects that don't provide necessary QIs (such as objects under
    // the new DOM bindings). We expect the other side of the CPOW to have
    // the appropriate wrappers in place.
    RootedObject cpow(cx, UnwrapNativeCPOW(aHelper.Object()));
    if (cpow) {
        if (!JS_WrapObject(cx, &cpow))
            return false;
        d.setObject(*cpow);
        return true;
    }

    // We can't simply construct a slim wrapper. Go ahead and create an
    // XPCWrappedNative for this object. At this point, |flat| could be
    // non-null, meaning that either we already have a wrapped native from
    // the cache (which might need to be QI'd to the new interface) or that
    // we found a slim wrapper that we'll have to morph.
    AutoMarkingNativeInterfacePtr iface(cx);
    if (iid) {
        if (Interface)
            iface = *Interface;

        if (!iface) {
            iface = XPCNativeInterface::GetNewOrUsed(iid);
            if (!iface)
                return false;

            if (Interface)
                *Interface = iface;
        }
    }

    MOZ_ASSERT(!flat || IS_WN_REFLECTOR(flat), "What kind of wrapper is this?");

    nsresult rv;
    XPCWrappedNative* wrapper;
    nsRefPtr<XPCWrappedNative> strongWrapper;
    if (!flat) {
        rv = XPCWrappedNative::GetNewOrUsed(aHelper, xpcscope, iface,
                                            getter_AddRefs(strongWrapper));

        wrapper = strongWrapper;
    } else {
        MOZ_ASSERT(IS_WN_REFLECTOR(flat));

        wrapper = XPCWrappedNative::Get(flat);

        // If asked to return the wrapper we'll return a strong reference,
        // otherwise we'll just return its JSObject in d (which should be
        // rooted in that case).
        if (dest)
            strongWrapper = wrapper;
        if (iface)
            wrapper->FindTearOff(iface, false, &rv);
        else
            rv = NS_OK;
    }

    if (NS_FAILED(rv) && pErr)
        *pErr = rv;

    // If creating the wrapped native failed, then return early.
    if (NS_FAILED(rv) || !wrapper)
        return false;

    // If we're not creating security wrappers, we can return the
    // XPCWrappedNative as-is here.
    flat = wrapper->GetFlatJSObject();
    jsval v = OBJECT_TO_JSVAL(flat);
    if (!allowNativeWrapper) {
        d.set(v);
        if (dest)
            strongWrapper.forget(dest);
        if (pErr)
            *pErr = NS_OK;
        return true;
    }

    // The call to wrap here handles both cross-compartment and same-compartment
    // security wrappers.
    RootedObject original(cx, flat);
    if (!JS_WrapObject(cx, &flat))
        return false;

    d.setObjectOrNull(flat);

    if (dest) {
        // The strongWrapper still holds the original flat object.
        if (flat == original) {
            strongWrapper.forget(dest);
        } else {
            nsRefPtr<XPCJSObjectHolder> objHolder =
                XPCJSObjectHolder::newHolder(flat);
            if (!objHolder)
                return false;

            objHolder.forget(dest);
        }
    }

    if (pErr)
        *pErr = NS_OK;

    return true;
}

/***************************************************************************/

// static
bool
XPCConvert::JSObject2NativeInterface(void** dest, HandleObject src,
                                     const nsID* iid,
                                     nsISupports* aOuter,
                                     nsresult* pErr)
{
    MOZ_ASSERT(dest, "bad param");
    MOZ_ASSERT(src, "bad param");
    MOZ_ASSERT(iid, "bad param");

    AutoJSContext cx;
    JSAutoCompartment ac(cx, src);

    *dest = nullptr;
     if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    nsISupports* iface;

    if (!aOuter) {
        // Note that if we have a non-null aOuter then it means that we are
        // forcing the creation of a wrapper even if the object *is* a
        // wrappedNative or other wise has 'nsISupportness'.
        // This allows wrapJSAggregatedToNative to work.

        // If we're looking at a security wrapper, see now if we're allowed to
        // pass it to C++. If we are, then fall through to the code below. If
        // we aren't, throw an exception eagerly.
        //
        // NB: It's very important that we _don't_ unwrap in the aOuter case,
        // because the caller may explicitly want to create the XPCWrappedJS
        // around a security wrapper. XBL does this with Xrays from the XBL
        // scope - see nsBindingManager::GetBindingImplementation.
        JSObject* inner = js::CheckedUnwrap(src, /* stopAtOuter = */ false);

        // Hack - For historical reasons, wrapped chrome JS objects have been
        // passable as native interfaces. We'd like to fix this, but it
        // involves fixing the contacts API and PeerConnection to stop using
        // COWs. This needs to happen, but for now just preserve the old
        // behavior.
        //
        // Note that there is an identical hack in getWrapper which should be
        // removed if this one is.
        if (!inner && MOZ_UNLIKELY(xpc::WrapperFactory::IsCOW(src)))
            inner = js::UncheckedUnwrap(src);
        if (!inner) {
            if (pErr)
                *pErr = NS_ERROR_XPC_SECURITY_MANAGER_VETO;
            return false;
        }

        // Is this really a native xpcom object with a wrapper?
        XPCWrappedNative* wrappedNative = nullptr;
        if (IS_WN_REFLECTOR(inner))
            wrappedNative = XPCWrappedNative::Get(inner);
        if (wrappedNative) {
            iface = wrappedNative->GetIdentityObject();
            return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));
        }
        // else...

        // Deal with slim wrappers here.
        if (GetISupportsFromJSObject(inner ? inner : src, &iface)) {
            if (iface)
                return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));

            return false;
        }
    }

    // else...

    nsXPCWrappedJS* wrapper;
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(src, *iid, &wrapper);
    if (pErr)
        *pErr = rv;
    if (NS_SUCCEEDED(rv) && wrapper) {
        // If the caller wanted to aggregate this JS object to a native,
        // attach it to the wrapper. Note that we allow a maximum of one
        // aggregated native for a given XPCWrappedJS.
        if (aOuter)
            wrapper->SetAggregatedNativeObject(aOuter);

        // We need to go through the QueryInterface logic to make this return
        // the right thing for the various 'special' interfaces; e.g.
        // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
        // there is an outer to avoid nasty recursion.
        rv = aOuter ? wrapper->AggregatedQueryInterface(*iid, dest) :
                      wrapper->QueryInterface(*iid, dest);
        if (pErr)
            *pErr = rv;
        NS_RELEASE(wrapper);
        return NS_SUCCEEDED(rv);
    }

    // else...
    return false;
}

/***************************************************************************/
/***************************************************************************/

// static
nsresult
XPCConvert::ConstructException(nsresult rv, const char* message,
                               const char* ifaceName, const char* methodName,
                               nsISupports* data,
                               nsIException** exceptn,
                               JSContext* cx,
                               jsval* jsExceptionPtr)
{
    MOZ_ASSERT(!cx == !jsExceptionPtr, "Expected cx and jsExceptionPtr to cooccur.");

    static const char format[] = "\'%s\' when calling method: [%s::%s]";
    const char * msg = message;
    nsXPIDLString xmsg;
    nsAutoCString sxmsg;

    nsCOMPtr<nsIScriptError> errorObject = do_QueryInterface(data);
    if (errorObject) {
        if (NS_SUCCEEDED(errorObject->GetMessageMoz(getter_Copies(xmsg)))) {
            CopyUTF16toUTF8(xmsg, sxmsg);
            msg = sxmsg.get();
        }
    }
    if (!msg)
        if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &msg) || ! msg)
            msg = "<error>";

    nsCString msgStr(msg);
    if (ifaceName && methodName)
        msgStr.AppendPrintf(format, msg, ifaceName, methodName);

    nsRefPtr<Exception> e = new Exception(msgStr, rv, EmptyCString(), nullptr, data);

    if (cx && jsExceptionPtr) {
        e->StowJSVal(*jsExceptionPtr);
    }

    e.forget(exceptn);
    return NS_OK;
}

/********************************/

class MOZ_STACK_CLASS AutoExceptionRestorer
{
public:
    AutoExceptionRestorer(JSContext *cx, Value v)
        : mContext(cx), tvr(cx, v)
    {
        JS_ClearPendingException(mContext);
    }

    ~AutoExceptionRestorer()
    {
        JS_SetPendingException(mContext, tvr);
    }

private:
    JSContext * const mContext;
    RootedValue tvr;
};

// static
nsresult
XPCConvert::JSValToXPCException(MutableHandleValue s,
                                const char* ifaceName,
                                const char* methodName,
                                nsIException** exceptn)
{
    AutoJSContext cx;
    AutoExceptionRestorer aer(cx, s);

    if (!s.isPrimitive()) {
        // we have a JSObject
        RootedObject obj(cx, s.toObjectOrNull());

        if (!obj) {
            NS_ERROR("when is an object not an object?");
            return NS_ERROR_FAILURE;
        }

        // is this really a native xpcom object with a wrapper?
        JSObject *unwrapped = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
        if (!unwrapped)
            return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
        XPCWrappedNative* wrapper = IS_WN_REFLECTOR(unwrapped) ? XPCWrappedNative::Get(unwrapped)
                                                               : nullptr;
        if (wrapper) {
            nsISupports* supports = wrapper->GetIdentityObject();
            nsCOMPtr<nsIException> iface = do_QueryInterface(supports);
            if (iface) {
                // just pass through the exception (with extra ref and all)
                nsCOMPtr<nsIException> temp = iface;
                temp.forget(exceptn);
                return NS_OK;
            } else {
                // it is a wrapped native, but not an exception!
                return ConstructException(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT,
                                          nullptr, ifaceName, methodName, supports,
                                          exceptn, nullptr, nullptr);
            }
        } else {
            // It is a JSObject, but not a wrapped native...

            // If it is an engine Error with an error report then let's
            // extract the report and build an xpcexception from that
            const JSErrorReport* report;
            if (nullptr != (report = JS_ErrorFromException(cx, obj))) {
                JSAutoByteString message;
                JSString* str;
                if (nullptr != (str = ToString(cx, s)))
                    message.encodeLatin1(cx, str);
                return JSErrorToXPCException(message.ptr(), ifaceName,
                                             methodName, report, exceptn);
            }


            bool found;

            // heuristic to see if it might be usable as an xpcexception
            if (!JS_HasProperty(cx, obj, "message", &found))
                return NS_ERROR_FAILURE;

            if (found && !JS_HasProperty(cx, obj, "result", &found))
                return NS_ERROR_FAILURE;

            if (found) {
                // lets try to build a wrapper around the JSObject
                nsXPCWrappedJS* jswrapper;
                nsresult rv =
                    nsXPCWrappedJS::GetNewOrUsed(obj, NS_GET_IID(nsIException), &jswrapper);
                if (NS_FAILED(rv))
                    return rv;

                *exceptn = static_cast<nsIException *>(jswrapper->GetXPTCStub());
                return NS_OK;
            }


            // XXX we should do a check against 'js_ErrorClass' here and
            // do the right thing - even though it has no JSErrorReport,
            // The fact that it is a JSError exceptions means we can extract
            // particular info and our 'result' should reflect that.

            // otherwise we'll just try to convert it to a string

            JSString* str = ToString(cx, s);
            if (!str)
                return NS_ERROR_FAILURE;

            JSAutoByteString strBytes(cx, str);
            if (!strBytes)
                return NS_ERROR_FAILURE;

            return ConstructException(NS_ERROR_XPC_JS_THREW_JS_OBJECT,
                                      strBytes.ptr(), ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        }
    }

    if (s.isUndefined() || s.isNull()) {
        return ConstructException(NS_ERROR_XPC_JS_THREW_NULL,
                                  nullptr, ifaceName, methodName, nullptr,
                                  exceptn, cx, s.address());
    }

    if (s.isNumber()) {
        // lets see if it looks like an nsresult
        nsresult rv;
        double number;
        bool isResult = false;

        if (s.isInt32()) {
            rv = (nsresult) s.toInt32();
            if (NS_FAILED(rv))
                isResult = true;
            else
                number = (double) s.toInt32();
        } else {
            number = s.toDouble();
            if (number > 0.0 &&
                number < (double)0xffffffff &&
                0.0 == fmod(number,1)) {
                // Visual Studio 9 doesn't allow casting directly from a
                // double to an enumeration type, contrary to 5.2.9(10) of
                // C++11, so add an intermediate cast.
                rv = (nsresult)(uint32_t) number;
                if (NS_FAILED(rv))
                    isResult = true;
            }
        }

        if (isResult)
            return ConstructException(rv, nullptr, ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        else {
            // XXX all this nsISupportsDouble code seems a little redundant
            // now that we're storing the jsval in the exception...
            nsISupportsDouble* data;
            nsCOMPtr<nsIComponentManager> cm;
            if (NS_FAILED(NS_GetComponentManager(getter_AddRefs(cm))) || !cm ||
                NS_FAILED(cm->CreateInstanceByContractID(NS_SUPPORTS_DOUBLE_CONTRACTID,
                                                         nullptr,
                                                         NS_GET_IID(nsISupportsDouble),
                                                         (void**)&data)))
                return NS_ERROR_FAILURE;
            data->SetData(number);
            rv = ConstructException(NS_ERROR_XPC_JS_THREW_NUMBER, nullptr,
                                    ifaceName, methodName, data, exceptn, cx, s.address());
            NS_RELEASE(data);
            return rv;
        }
    }

    // otherwise we'll just try to convert it to a string
    // Note: e.g., bools get converted to JSStrings by this code.

    JSString* str = ToString(cx, s);
    if (str) {
        JSAutoByteString strBytes(cx, str);
        if (!!strBytes) {
            return ConstructException(NS_ERROR_XPC_JS_THREW_STRING,
                                      strBytes.ptr(), ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        }
    }
    return NS_ERROR_FAILURE;
}

/********************************/

// static
nsresult
XPCConvert::JSErrorToXPCException(const char* message,
                                  const char* ifaceName,
                                  const char* methodName,
                                  const JSErrorReport* report,
                                  nsIException** exceptn)
{
    AutoJSContext cx;
    nsresult rv = NS_ERROR_FAILURE;
    nsRefPtr<nsScriptError> data;
    if (report) {
        nsAutoString bestMessage;
        if (report && report->ucmessage) {
            bestMessage = static_cast<const char16_t*>(report->ucmessage);
        } else if (message) {
            CopyASCIItoUTF16(message, bestMessage);
        } else {
            bestMessage.AssignLiteral("JavaScript Error");
        }

        const char16_t* uclinebuf =
            static_cast<const char16_t*>(report->uclinebuf);

        data = new nsScriptError();
        data->InitWithWindowID(
            bestMessage,
            NS_ConvertASCIItoUTF16(report->filename),
            uclinebuf ? nsDependentString(uclinebuf) : EmptyString(),
            report->lineno,
            report->uctokenptr - report->uclinebuf, report->flags,
            NS_LITERAL_CSTRING("XPConnect JavaScript"),
            nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx));
    }

    if (data) {
        nsAutoCString formattedMsg;
        data->ToString(formattedMsg);

        rv = ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS,
                                formattedMsg.get(), ifaceName, methodName,
                                static_cast<nsIScriptError*>(data.get()),
                                exceptn, nullptr, nullptr);
    } else {
        rv = ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR,
                                nullptr, ifaceName, methodName, nullptr,
                                exceptn, nullptr, nullptr);
    }
    return rv;
}

/***************************************************************************/

// array fun...

#ifdef POPULATE
#undef POPULATE
#endif

// static
bool
XPCConvert::NativeArray2JS(MutableHandleValue d, const void** s,
                           const nsXPTType& type, const nsID* iid,
                           uint32_t count, nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");

    AutoJSContext cx;

    // XXX add support for putting chars in a string rather than an array

    // XXX add support to indicate *which* array element was not convertable

    RootedObject array(cx, JS_NewArrayObject(cx, count));
    if (!array)
        return false;

    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    uint32_t i;
    RootedValue current(cx, JSVAL_NULL);

#define POPULATE(_t)                                                                    \
    PR_BEGIN_MACRO                                                                      \
        for (i = 0; i < count; i++) {                                                   \
            if (!NativeData2JS(&current, ((_t*)*s)+i, type, iid, pErr) ||               \
                !JS_SetElement(cx, array, i, current))                                  \
                goto failure;                                                           \
        }                                                                               \
    PR_END_MACRO

    // XXX check IsPtr - esp. to handle array of nsID (as opposed to nsID*)

    switch (type.TagPart()) {
    case nsXPTType::T_I8            : POPULATE(int8_t);         break;
    case nsXPTType::T_I16           : POPULATE(int16_t);        break;
    case nsXPTType::T_I32           : POPULATE(int32_t);        break;
    case nsXPTType::T_I64           : POPULATE(int64_t);        break;
    case nsXPTType::T_U8            : POPULATE(uint8_t);        break;
    case nsXPTType::T_U16           : POPULATE(uint16_t);       break;
    case nsXPTType::T_U32           : POPULATE(uint32_t);       break;
    case nsXPTType::T_U64           : POPULATE(uint64_t);       break;
    case nsXPTType::T_FLOAT         : POPULATE(float);          break;
    case nsXPTType::T_DOUBLE        : POPULATE(double);         break;
    case nsXPTType::T_BOOL          : POPULATE(bool);           break;
    case nsXPTType::T_CHAR          : POPULATE(char);           break;
    case nsXPTType::T_WCHAR         : POPULATE(jschar);         break;
    case nsXPTType::T_VOID          : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_IID           : POPULATE(nsID*);          break;
    case nsXPTType::T_DOMSTRING     : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_CHAR_STR      : POPULATE(char*);          break;
    case nsXPTType::T_WCHAR_STR     : POPULATE(jschar*);        break;
    case nsXPTType::T_INTERFACE     : POPULATE(nsISupports*);   break;
    case nsXPTType::T_INTERFACE_IS  : POPULATE(nsISupports*);   break;
    case nsXPTType::T_UTF8STRING    : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_CSTRING       : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_ASTRING       : NS_ERROR("bad type"); goto failure;
    default                         : NS_ERROR("bad type"); goto failure;
    }

    if (pErr)
        *pErr = NS_OK;
    d.setObject(*array);
    return true;

failure:
    return false;

#undef POPULATE
}



// Check that the tag part of the type matches the type
// of the array. If the check succeeds, check that the size
// of the output does not exceed UINT32_MAX bytes. Allocate
// the memory and copy the elements by memcpy.
static bool
CheckTargetAndPopulate(const nsXPTType& type,
                       uint8_t requiredType,
                       size_t typeSize,
                       uint32_t count,
                       JSObject* tArr,
                       void** output,
                       nsresult* pErr)
{
    // Check that the element type expected by the interface matches
    // the type of the elements in the typed array exactly, including
    // signedness.
    if (type.TagPart() != requiredType) {
        if (pErr)
            *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

        return false;
    }

    // Calulate the maximum number of elements that can fit in
    // UINT32_MAX bytes.
    size_t max = UINT32_MAX / typeSize;

    // This could overflow on 32-bit systems so check max first.
    size_t byteSize = count * typeSize;
    if (count > max || !(*output = nsMemory::Alloc(byteSize))) {
        if (pErr)
            *pErr = NS_ERROR_OUT_OF_MEMORY;

        return false;
    }

    memcpy(*output, JS_GetArrayBufferViewData(tArr), byteSize);
    return true;
}

// Fast conversion of typed arrays to native using memcpy.
// No float or double canonicalization is done. Called by
// JSarray2Native whenever a TypedArray is met. ArrayBuffers
// are not accepted; create a properly typed array view on them
// first. The element type of array must match the XPCOM
// type in size, type and signedness exactly. As an exception,
// Uint8ClampedArray is allowed for arrays of uint8_t. DataViews
// are not supported.

// static
bool
XPCConvert::JSTypedArray2Native(void** d,
                                JSObject* jsArray,
                                uint32_t count,
                                const nsXPTType& type,
                                nsresult* pErr)
{
    MOZ_ASSERT(jsArray, "bad param");
    MOZ_ASSERT(d, "bad param");
    MOZ_ASSERT(JS_IsTypedArrayObject(jsArray), "not a typed array");

    // Check the actual length of the input array against the
    // given size_is.
    uint32_t len = JS_GetTypedArrayLength(jsArray);
    if (len < count) {
        if (pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;

        return false;
    }

    void* output = nullptr;

    switch (JS_GetArrayBufferViewType(jsArray)) {
    case js::ArrayBufferView::TYPE_INT8:
        if (!CheckTargetAndPopulate(nsXPTType::T_I8, type,
                                    sizeof(int8_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_UINT8:
    case js::ArrayBufferView::TYPE_UINT8_CLAMPED:
        if (!CheckTargetAndPopulate(nsXPTType::T_U8, type,
                                    sizeof(uint8_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_INT16:
        if (!CheckTargetAndPopulate(nsXPTType::T_I16, type,
                                    sizeof(int16_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_UINT16:
        if (!CheckTargetAndPopulate(nsXPTType::T_U16, type,
                                    sizeof(uint16_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_INT32:
        if (!CheckTargetAndPopulate(nsXPTType::T_I32, type,
                                    sizeof(int32_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_UINT32:
        if (!CheckTargetAndPopulate(nsXPTType::T_U32, type,
                                    sizeof(uint32_t), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_FLOAT32:
        if (!CheckTargetAndPopulate(nsXPTType::T_FLOAT, type,
                                    sizeof(float), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    case js::ArrayBufferView::TYPE_FLOAT64:
        if (!CheckTargetAndPopulate(nsXPTType::T_DOUBLE, type,
                                    sizeof(double), count,
                                    jsArray, &output, pErr)) {
            return false;
        }
        break;

    // Yet another array type was defined? It is not supported yet...
    default:
        if (pErr)
            *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

        return false;
    }

    *d = output;
    if (pErr)
        *pErr = NS_OK;

    return true;
}

// static
bool
XPCConvert::JSArray2Native(void** d, HandleValue s,
                           uint32_t count, const nsXPTType& type,
                           const nsID* iid, nsresult* pErr)
{
    MOZ_ASSERT(d, "bad param");

    AutoJSContext cx;

    // XXX add support for getting chars from strings

    // XXX add support to indicate *which* array element was not convertable

    if (s.isNullOrUndefined()) {
        if (0 != count) {
            if (pErr)
                *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
            return false;
        }

        *d = nullptr;
        return true;
    }

    if (!s.isObject()) {
        if (pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY;
        return false;
    }

    RootedObject jsarray(cx, &s.toObject());

    // If this is a typed array, then try a fast conversion with memcpy.
    if (JS_IsTypedArrayObject(jsarray)) {
        return JSTypedArray2Native(d, jsarray, count, type, pErr);
    }

    if (!JS_IsArrayObject(cx, jsarray)) {
        if (pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY;
        return false;
    }

    uint32_t len;
    if (!JS_GetArrayLength(cx, jsarray, &len) || len < count) {
        if (pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
        return false;
    }

    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

#define POPULATE(_mode, _t)                                                    \
    PR_BEGIN_MACRO                                                             \
        cleanupMode = _mode;                                                   \
        size_t max = UINT32_MAX / sizeof(_t);                                  \
        if (count > max ||                                                     \
            nullptr == (array = nsMemory::Alloc(count * sizeof(_t)))) {        \
            if (pErr)                                                          \
                *pErr = NS_ERROR_OUT_OF_MEMORY;                                \
            goto failure;                                                      \
        }                                                                      \
        for (initedCount = 0; initedCount < count; initedCount++) {            \
            if (!JS_GetElement(cx, jsarray, initedCount, &current) ||          \
                !JSData2Native(((_t*)array)+initedCount, current, type,        \
                               true, iid, pErr))                               \
                goto failure;                                                  \
        }                                                                      \
    PR_END_MACRO

    // No Action, FRee memory, RElease object
    enum CleanupMode {na, fr, re};

    CleanupMode cleanupMode;

    void *array = nullptr;
    uint32_t initedCount;
    RootedValue current(cx);

    // XXX check IsPtr - esp. to handle array of nsID (as opposed to nsID*)
    // XXX make extra space at end of char* and wchar* and null termintate

    switch (type.TagPart()) {
    case nsXPTType::T_I8            : POPULATE(na, int8_t);         break;
    case nsXPTType::T_I16           : POPULATE(na, int16_t);        break;
    case nsXPTType::T_I32           : POPULATE(na, int32_t);        break;
    case nsXPTType::T_I64           : POPULATE(na, int64_t);        break;
    case nsXPTType::T_U8            : POPULATE(na, uint8_t);        break;
    case nsXPTType::T_U16           : POPULATE(na, uint16_t);       break;
    case nsXPTType::T_U32           : POPULATE(na, uint32_t);       break;
    case nsXPTType::T_U64           : POPULATE(na, uint64_t);       break;
    case nsXPTType::T_FLOAT         : POPULATE(na, float);          break;
    case nsXPTType::T_DOUBLE        : POPULATE(na, double);         break;
    case nsXPTType::T_BOOL          : POPULATE(na, bool);           break;
    case nsXPTType::T_CHAR          : POPULATE(na, char);           break;
    case nsXPTType::T_WCHAR         : POPULATE(na, jschar);         break;
    case nsXPTType::T_VOID          : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_IID           : POPULATE(fr, nsID*);          break;
    case nsXPTType::T_DOMSTRING     : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_CHAR_STR      : POPULATE(fr, char*);          break;
    case nsXPTType::T_WCHAR_STR     : POPULATE(fr, jschar*);        break;
    case nsXPTType::T_INTERFACE     : POPULATE(re, nsISupports*);   break;
    case nsXPTType::T_INTERFACE_IS  : POPULATE(re, nsISupports*);   break;
    case nsXPTType::T_UTF8STRING    : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_CSTRING       : NS_ERROR("bad type"); goto failure;
    case nsXPTType::T_ASTRING       : NS_ERROR("bad type"); goto failure;
    default                         : NS_ERROR("bad type"); goto failure;
    }

    *d = array;
    if (pErr)
        *pErr = NS_OK;
    return true;

failure:
    // we may need to cleanup the partially filled array of converted stuff
    if (array) {
        if (cleanupMode == re) {
            nsISupports** a = (nsISupports**) array;
            for (uint32_t i = 0; i < initedCount; i++) {
                nsISupports* p = a[i];
                NS_IF_RELEASE(p);
            }
        } else if (cleanupMode == fr) {
            void** a = (void**) array;
            for (uint32_t i = 0; i < initedCount; i++) {
                void* p = a[i];
                if (p) nsMemory::Free(p);
            }
        }
        nsMemory::Free(array);
    }

    return false;

#undef POPULATE
}

// static
bool
XPCConvert::NativeStringWithSize2JS(MutableHandleValue d, const void* s,
                                    const nsXPTType& type,
                                    uint32_t count,
                                    nsresult* pErr)
{
    NS_PRECONDITION(s, "bad param");

    AutoJSContext cx;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    switch (type.TagPart()) {
        case nsXPTType::T_PSTRING_SIZE_IS:
        {
            char* p = *((char**)s);
            if (!p)
                break;
            JSString* str;
            if (!(str = JS_NewStringCopyN(cx, p, count)))
                return false;
            d.setString(str);
            break;
        }
        case nsXPTType::T_PWSTRING_SIZE_IS:
        {
            jschar* p = *((jschar**)s);
            if (!p)
                break;
            JSString* str;
            if (!(str = JS_NewUCStringCopyN(cx, p, count)))
                return false;
            d.setString(str);
            break;
        }
        default:
            XPC_LOG_ERROR(("XPCConvert::NativeStringWithSize2JS : unsupported type"));
            return false;
    }
    return true;
}

// static
bool
XPCConvert::JSStringWithSize2Native(void* d, HandleValue s,
                                    uint32_t count, const nsXPTType& type,
                                    nsresult* pErr)
{
    NS_PRECONDITION(!s.isNull(), "bad param");
    NS_PRECONDITION(d, "bad param");

    AutoJSContext cx;
    uint32_t len;

    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    switch (type.TagPart()) {
        case nsXPTType::T_PSTRING_SIZE_IS:
        {
            if (s.isUndefined() || s.isNull()) {
                if (0 != count) {
                    if (pErr)
                        *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                    return false;
                }
                if (0 != count) {
                    len = (count + 1) * sizeof(char);
                    if (!(*((void**)d) = nsMemory::Alloc(len)))
                        return false;
                    return true;
                }
                // else ...

                *((char**)d) = nullptr;
                return true;
            }

            JSString* str = ToString(cx, s);
            if (!str) {
                return false;
            }

            size_t length = JS_GetStringEncodingLength(cx, str);
            if (length == size_t(-1)) {
                return false;
            }
            if (length > count) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            len = uint32_t(length);

            if (len < count)
                len = count;

            uint32_t alloc_len = (len + 1) * sizeof(char);
            char *buffer = static_cast<char *>(nsMemory::Alloc(alloc_len));
            if (!buffer) {
                return false;
            }
            JS_EncodeStringToBuffer(cx, str, buffer, len);
            buffer[len] = '\0';
            *((char**)d) = buffer;

            return true;
        }

        case nsXPTType::T_PWSTRING_SIZE_IS:
        {
            const jschar* chars=nullptr;
            JSString* str;

            if (s.isUndefined() || s.isNull()) {
                if (0 != count) {
                    if (pErr)
                        *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                    return false;
                }

                if (0 != count) {
                    len = (count + 1) * sizeof(jschar);
                    if (!(*((void**)d) = nsMemory::Alloc(len)))
                        return false;
                    return true;
                }

                // else ...
                *((const jschar**)d) = nullptr;
                return true;
            }

            if (!(str = ToString(cx, s))) {
                return false;
            }

            len = JS_GetStringLength(str);
            if (len > count) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            if (len < count)
                len = count;

            if (!(chars = JS_GetStringCharsZ(cx, str))) {
                return false;
            }
            uint32_t alloc_len = (len + 1) * sizeof(jschar);
            if (!(*((void**)d) = nsMemory::Alloc(alloc_len))) {
                // XXX should report error
                return false;
            }
            memcpy(*((jschar**)d), chars, alloc_len);
            (*((jschar**)d))[count] = 0;

            return true;
        }
        default:
            XPC_LOG_ERROR(("XPCConvert::JSStringWithSize2Native : unsupported type"));
            return false;
    }
}

