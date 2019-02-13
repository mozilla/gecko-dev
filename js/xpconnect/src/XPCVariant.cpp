/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* nsIVariant implementation for xpconnect. */

#include "mozilla/Range.h"

#include "xpcprivate.h"

#include "jsfriendapi.h"
#include "jsprf.h"
#include "jswrapper.h"

using namespace JS;
using namespace mozilla;

NS_IMPL_CLASSINFO(XPCVariant, nullptr, 0, XPCVARIANT_CID)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(XPCVariant)
  NS_INTERFACE_MAP_ENTRY(nsIVariant)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_IMPL_QUERY_CLASSINFO(XPCVariant)
NS_INTERFACE_MAP_END
NS_IMPL_CI_INTERFACE_GETTER(XPCVariant, XPCVariant, nsIVariant)

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPCVariant)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPCVariant)

XPCVariant::XPCVariant(JSContext* cx, jsval aJSVal)
    : mJSVal(aJSVal), mCCGeneration(0)
{
    nsVariant::Initialize(&mData);
    if (!mJSVal.isPrimitive()) {
        // XXXbholley - The innerization here was from bug 638026. Blake says
        // the basic problem was that we were storing the C++ inner but the JS
        // outer, which meant that, after navigation, the JS inner could be
        // collected, which would cause us to try to recreate the JS inner at
        // some later point after teardown, which would crash. This is shouldn't
        // be a problem anymore because SetParentToWindow will do the right
        // thing, but I'm saving the cleanup here for another day. Blake thinks
        // that we should just not store the WN if we're creating a variant for
        // an outer window.
        JS::RootedObject obj(cx, &mJSVal.toObject());
        obj = JS_ObjectToInnerObject(cx, obj);
        mJSVal = JS::ObjectValue(*obj);

        JSObject* unwrapped = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
        mReturnRawObject = !(unwrapped && IS_WN_REFLECTOR(unwrapped));
    } else
        mReturnRawObject = false;
}

XPCTraceableVariant::~XPCTraceableVariant()
{
    jsval val = GetJSValPreserveColor();

    MOZ_ASSERT(val.isGCThing(), "Must be traceable or unlinked");

    nsVariant::Cleanup(&mData);

    if (!val.isNull())
        RemoveFromRootSet();
}

void XPCTraceableVariant::TraceJS(JSTracer* trc)
{
    MOZ_ASSERT(mJSVal.isMarkable());
    JS_CallValueTracer(trc, &mJSVal, "XPCTraceableVariant::mJSVal");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(XPCVariant)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPCVariant)
    JS::Value val = tmp->GetJSValPreserveColor();
    if (val.isObject()) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mJSVal");
        cb.NoteJSObject(&val.toObject());
    }

    nsVariant::Traverse(tmp->mData, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPCVariant)
    JS::Value val = tmp->GetJSValPreserveColor();

    nsVariant::Cleanup(&tmp->mData);

    if (val.isMarkable()) {
        XPCTraceableVariant* v = static_cast<XPCTraceableVariant*>(tmp);
        v->RemoveFromRootSet();
    }
    tmp->mJSVal = JS::NullValue();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// static
already_AddRefed<XPCVariant>
XPCVariant::newVariant(JSContext* cx, jsval aJSVal)
{
    nsRefPtr<XPCVariant> variant;

    if (!aJSVal.isMarkable())
        variant = new XPCVariant(cx, aJSVal);
    else
        variant = new XPCTraceableVariant(cx, aJSVal);

    if (!variant->InitializeData(cx))
        return nullptr;

    return variant.forget();
}

// Helper class to give us a namespace for the table based code below.
class XPCArrayHomogenizer
{
private:
    enum Type
    {
        tNull  = 0 ,  // null value
        tInt       ,  // Integer
        tDbl       ,  // Double
        tBool      ,  // Boolean
        tStr       ,  // String
        tID        ,  // ID
        tArr       ,  // Array
        tISup      ,  // nsISupports (really just a plain JSObject)
        tUnk       ,  // Unknown. Used only for initial state.

        tTypeCount ,  // Just a count for table dimensioning.

        tVar       ,  // nsVariant - last ditch if no other common type found.
        tErr          // No valid state or type has this value.
    };

    // Table has tUnk as a state (column) but not as a type (row).
    static const Type StateTable[tTypeCount][tTypeCount-1];

public:
    static bool GetTypeForArray(JSContext* cx, HandleObject array,
                                uint32_t length,
                                nsXPTType* resultType, nsID* resultID);
};


// Current state is the column down the side.
// Current type is the row along the top.
// New state is in the box at the intersection.

const XPCArrayHomogenizer::Type
XPCArrayHomogenizer::StateTable[tTypeCount][tTypeCount-1] = {
/*          tNull,tInt ,tDbl ,tBool,tStr ,tID  ,tArr ,tISup */
/* tNull */{tNull,tVar ,tVar ,tVar ,tStr ,tID  ,tVar ,tISup },
/* tInt  */{tVar ,tInt ,tDbl ,tVar ,tVar ,tVar ,tVar ,tVar  },
/* tDbl  */{tVar ,tDbl ,tDbl ,tVar ,tVar ,tVar ,tVar ,tVar  },
/* tBool */{tVar ,tVar ,tVar ,tBool,tVar ,tVar ,tVar ,tVar  },
/* tStr  */{tStr ,tVar ,tVar ,tVar ,tStr ,tVar ,tVar ,tVar  },
/* tID   */{tID  ,tVar ,tVar ,tVar ,tVar ,tID  ,tVar ,tVar  },
/* tArr  */{tErr ,tErr ,tErr ,tErr ,tErr ,tErr ,tErr ,tErr  },
/* tISup */{tISup,tVar ,tVar ,tVar ,tVar ,tVar ,tVar ,tISup },
/* tUnk  */{tNull,tInt ,tDbl ,tBool,tStr ,tID  ,tVar ,tISup }};

// static
bool
XPCArrayHomogenizer::GetTypeForArray(JSContext* cx, HandleObject array,
                                     uint32_t length,
                                     nsXPTType* resultType, nsID* resultID)
{
    Type state = tUnk;
    Type type;

    RootedValue val(cx);
    RootedObject jsobj(cx);
    for (uint32_t i = 0; i < length; i++) {
        if (!JS_GetElement(cx, array, i, &val))
            return false;

        if (val.isInt32()) {
            type = tInt;
        } else if (val.isDouble()) {
            type = tDbl;
        } else if (val.isBoolean()) {
            type = tBool;
        } else if (val.isUndefined() || val.isSymbol()) {
            state = tVar;
            break;
        } else if (val.isNull()) {
            type = tNull;
        } else if (val.isString()) {
            type = tStr;
        } else {
            MOZ_ASSERT(val.isObject(), "invalid type of jsval!");
            jsobj = &val.toObject();
            if (JS_IsArrayObject(cx, jsobj))
                type = tArr;
            else if (xpc_JSObjectIsID(cx, jsobj))
                type = tID;
            else
                type = tISup;
        }

        MOZ_ASSERT(state != tErr, "bad state table!");
        MOZ_ASSERT(type  != tErr, "bad type!");
        MOZ_ASSERT(type  != tVar, "bad type!");
        MOZ_ASSERT(type  != tUnk, "bad type!");

        state = StateTable[state][type];

        MOZ_ASSERT(state != tErr, "bad state table!");
        MOZ_ASSERT(state != tUnk, "bad state table!");

        if (state == tVar)
            break;
    }

    switch (state) {
        case tInt :
            *resultType = nsXPTType((uint8_t)TD_INT32);
            break;
        case tDbl :
            *resultType = nsXPTType((uint8_t)TD_DOUBLE);
            break;
        case tBool:
            *resultType = nsXPTType((uint8_t)TD_BOOL);
            break;
        case tStr :
            *resultType = nsXPTType((uint8_t)TD_PWSTRING);
            break;
        case tID  :
            *resultType = nsXPTType((uint8_t)TD_PNSIID);
            break;
        case tISup:
            *resultType = nsXPTType((uint8_t)TD_INTERFACE_IS_TYPE);
            *resultID = NS_GET_IID(nsISupports);
            break;
        case tNull:
            // FALL THROUGH
        case tVar :
            *resultType = nsXPTType((uint8_t)TD_INTERFACE_IS_TYPE);
            *resultID = NS_GET_IID(nsIVariant);
            break;
        case tArr :
            // FALL THROUGH
        case tUnk :
            // FALL THROUGH
        case tErr :
            // FALL THROUGH
        default:
            NS_ERROR("bad state");
            return false;
    }
    return true;
}

bool XPCVariant::InitializeData(JSContext* cx)
{
    JS_CHECK_RECURSION(cx, return false);

    RootedValue val(cx, GetJSVal());

    if (val.isInt32())
        return NS_SUCCEEDED(nsVariant::SetFromInt32(&mData, val.toInt32()));
    if (val.isDouble())
        return NS_SUCCEEDED(nsVariant::SetFromDouble(&mData, val.toDouble()));
    if (val.isBoolean())
        return NS_SUCCEEDED(nsVariant::SetFromBool(&mData, val.toBoolean()));
    // We can't represent symbol on C++ side, so pretend it is void.
    if (val.isUndefined() || val.isSymbol())
        return NS_SUCCEEDED(nsVariant::SetToVoid(&mData));
    if (val.isNull())
        return NS_SUCCEEDED(nsVariant::SetToEmpty(&mData));
    if (val.isString()) {
        JSString* str = val.toString();
        if (!str)
            return false;

        MOZ_ASSERT(mData.mType == nsIDataType::VTYPE_EMPTY,
                   "Why do we already have data?");

        size_t length = JS_GetStringLength(str);
        if (!NS_SUCCEEDED(nsVariant::AllocateWStringWithSize(&mData, length)))
            return false;

        mozilla::Range<char16_t> destChars(mData.u.wstr.mWStringValue, length);
        if (!JS_CopyStringChars(cx, destChars, str))
            return false;

        MOZ_ASSERT(mData.u.wstr.mWStringValue[length] == '\0');
        return true;
    }

    // leaving only JSObject...
    MOZ_ASSERT(val.isObject(), "invalid type of jsval!");

    RootedObject jsobj(cx, &val.toObject());

    // Let's see if it is a xpcJSID.

    const nsID* id = xpc_JSObjectToID(cx, jsobj);
    if (id)
        return NS_SUCCEEDED(nsVariant::SetFromID(&mData, *id));

    // Let's see if it is a js array object.

    uint32_t len;

    if (JS_IsArrayObject(cx, jsobj) && JS_GetArrayLength(cx, jsobj, &len)) {
        if (!len) {
            // Zero length array
            nsVariant::SetToEmptyArray(&mData);
            return true;
        }

        nsXPTType type;
        nsID id;

        if (!XPCArrayHomogenizer::GetTypeForArray(cx, jsobj, len, &type, &id))
            return false;

        if (!XPCConvert::JSArray2Native(&mData.u.array.mArrayValue,
                                        val, len, type, &id, nullptr))
            return false;

        mData.mType = nsIDataType::VTYPE_ARRAY;
        if (type.IsInterfacePointer())
            mData.u.array.mArrayInterfaceID = id;
        mData.u.array.mArrayCount = len;
        mData.u.array.mArrayType = type.TagPart();

        return true;
    }

    // XXX This could be smarter and pick some more interesting iface.

    nsXPConnect*  xpc = nsXPConnect::XPConnect();
    nsCOMPtr<nsISupports> wrapper;
    const nsIID& iid = NS_GET_IID(nsISupports);

    return NS_SUCCEEDED(xpc->WrapJS(cx, jsobj,
                                    iid, getter_AddRefs(wrapper))) &&
           NS_SUCCEEDED(nsVariant::SetFromInterface(&mData, iid, wrapper));
}

NS_IMETHODIMP
XPCVariant::GetAsJSVal(MutableHandleValue result)
{
  result.set(GetJSVal());
  return NS_OK;
}

// static
bool
XPCVariant::VariantDataToJS(nsIVariant* variant,
                            nsresult* pErr, MutableHandleValue pJSVal)
{
    // Get the type early because we might need to spoof it below.
    uint16_t type;
    if (NS_FAILED(variant->GetDataType(&type)))
        return false;

    AutoJSContext cx;
    RootedValue realVal(cx);
    nsresult rv = variant->GetAsJSVal(&realVal);

    if (NS_SUCCEEDED(rv) &&
        (realVal.isPrimitive() ||
         type == nsIDataType::VTYPE_ARRAY ||
         type == nsIDataType::VTYPE_EMPTY_ARRAY ||
         type == nsIDataType::VTYPE_ID)) {
        if (!JS_WrapValue(cx, &realVal))
            return false;
        pJSVal.set(realVal);
        return true;
    }

    nsCOMPtr<XPCVariant> xpcvariant = do_QueryInterface(variant);
    if (xpcvariant && xpcvariant->mReturnRawObject) {
        MOZ_ASSERT(type == nsIDataType::VTYPE_INTERFACE ||
                   type == nsIDataType::VTYPE_INTERFACE_IS,
                   "Weird variant");

        if (!JS_WrapValue(cx, &realVal))
            return false;
        pJSVal.set(realVal);
        return true;
    }

    // else, it's an object and we really need to double wrap it if we've
    // already decided that its 'natural' type is as some sort of interface.

    // We just fall through to the code below and let it do what it does.

    // The nsIVariant is not a XPCVariant (or we act like it isn't).
    // So we extract the data and do the Right Thing.

    // We ASSUME that the variant implementation can do these conversions...

    nsID iid;

    switch (type) {
        case nsIDataType::VTYPE_INT8:
        case nsIDataType::VTYPE_INT16:
        case nsIDataType::VTYPE_INT32:
        case nsIDataType::VTYPE_INT64:
        case nsIDataType::VTYPE_UINT8:
        case nsIDataType::VTYPE_UINT16:
        case nsIDataType::VTYPE_UINT32:
        case nsIDataType::VTYPE_UINT64:
        case nsIDataType::VTYPE_FLOAT:
        case nsIDataType::VTYPE_DOUBLE:
        {
            double d;
            if (NS_FAILED(variant->GetAsDouble(&d)))
                return false;
            pJSVal.setNumber(d);
            return true;
        }
        case nsIDataType::VTYPE_BOOL:
        {
            bool b;
            if (NS_FAILED(variant->GetAsBool(&b)))
                return false;
            pJSVal.setBoolean(b);
            return true;
        }
        case nsIDataType::VTYPE_CHAR:
        {
            char c;
            if (NS_FAILED(variant->GetAsChar(&c)))
                return false;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&c, TD_CHAR, &iid, pErr);
        }
        case nsIDataType::VTYPE_WCHAR:
        {
            char16_t wc;
            if (NS_FAILED(variant->GetAsWChar(&wc)))
                return false;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&wc, TD_WCHAR, &iid, pErr);
        }
        case nsIDataType::VTYPE_ID:
        {
            if (NS_FAILED(variant->GetAsID(&iid)))
                return false;
            nsID* v = &iid;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&v, TD_PNSIID, &iid, pErr);
        }
        case nsIDataType::VTYPE_ASTRING:
        {
            nsAutoString astring;
            if (NS_FAILED(variant->GetAsAString(astring)))
                return false;
            nsAutoString* v = &astring;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&v, TD_ASTRING, &iid, pErr);
        }
        case nsIDataType::VTYPE_DOMSTRING:
        {
            nsAutoString astring;
            if (NS_FAILED(variant->GetAsAString(astring)))
                return false;
            nsAutoString* v = &astring;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&v,
                                             TD_DOMSTRING, &iid, pErr);
        }
        case nsIDataType::VTYPE_CSTRING:
        {
            nsAutoCString cString;
            if (NS_FAILED(variant->GetAsACString(cString)))
                return false;
            nsAutoCString* v = &cString;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&v,
                                             TD_CSTRING, &iid, pErr);
        }
        case nsIDataType::VTYPE_UTF8STRING:
        {
            nsUTF8String utf8String;
            if (NS_FAILED(variant->GetAsAUTF8String(utf8String)))
                return false;
            nsUTF8String* v = &utf8String;
            return XPCConvert::NativeData2JS(pJSVal, (const void*)&v,
                                             TD_UTF8STRING, &iid, pErr);
        }
        case nsIDataType::VTYPE_CHAR_STR:
        {
            char* pc;
            if (NS_FAILED(variant->GetAsString(&pc)))
                return false;
            bool success = XPCConvert::NativeData2JS(pJSVal, (const void*)&pc,
                                                     TD_PSTRING, &iid, pErr);
            free(pc);
            return success;
        }
        case nsIDataType::VTYPE_STRING_SIZE_IS:
        {
            char* pc;
            uint32_t size;
            if (NS_FAILED(variant->GetAsStringWithSize(&size, &pc)))
                return false;
            bool success = XPCConvert::NativeStringWithSize2JS(pJSVal, (const void*)&pc,
                                                               TD_PSTRING_SIZE_IS, size, pErr);
            free(pc);
            return success;
        }
        case nsIDataType::VTYPE_WCHAR_STR:
        {
            char16_t* pwc;
            if (NS_FAILED(variant->GetAsWString(&pwc)))
                return false;
            bool success = XPCConvert::NativeData2JS(pJSVal, (const void*)&pwc,
                                                     TD_PSTRING, &iid, pErr);
            free(pwc);
            return success;
        }
        case nsIDataType::VTYPE_WSTRING_SIZE_IS:
        {
            char16_t* pwc;
            uint32_t size;
            if (NS_FAILED(variant->GetAsWStringWithSize(&size, &pwc)))
                return false;
            bool success = XPCConvert::NativeStringWithSize2JS(pJSVal, (const void*)&pwc,
                                                               TD_PWSTRING_SIZE_IS, size, pErr);
            free(pwc);
            return success;
        }
        case nsIDataType::VTYPE_INTERFACE:
        case nsIDataType::VTYPE_INTERFACE_IS:
        {
            nsISupports* pi;
            nsID* piid;
            if (NS_FAILED(variant->GetAsInterface(&piid, (void**)&pi)))
                return false;

            iid = *piid;
            free((char*)piid);

            bool success = XPCConvert::NativeData2JS(pJSVal, (const void*)&pi,
                                                     TD_INTERFACE_IS_TYPE, &iid, pErr);
            if (pi)
                pi->Release();
            return success;
        }
        case nsIDataType::VTYPE_ARRAY:
        {
            nsDiscriminatedUnion du;
            nsVariant::Initialize(&du);
            nsresult rv;

            rv = variant->GetAsArray(&du.u.array.mArrayType,
                                     &du.u.array.mArrayInterfaceID,
                                     &du.u.array.mArrayCount,
                                     &du.u.array.mArrayValue);
            if (NS_FAILED(rv))
                return false;

            // must exit via VARIANT_DONE from here on...
            du.mType = nsIDataType::VTYPE_ARRAY;

            nsXPTType conversionType;
            uint16_t elementType = du.u.array.mArrayType;
            const nsID* pid = nullptr;

            switch (elementType) {
                case nsIDataType::VTYPE_INT8:
                case nsIDataType::VTYPE_INT16:
                case nsIDataType::VTYPE_INT32:
                case nsIDataType::VTYPE_INT64:
                case nsIDataType::VTYPE_UINT8:
                case nsIDataType::VTYPE_UINT16:
                case nsIDataType::VTYPE_UINT32:
                case nsIDataType::VTYPE_UINT64:
                case nsIDataType::VTYPE_FLOAT:
                case nsIDataType::VTYPE_DOUBLE:
                case nsIDataType::VTYPE_BOOL:
                case nsIDataType::VTYPE_CHAR:
                case nsIDataType::VTYPE_WCHAR:
                    conversionType = nsXPTType((uint8_t)elementType);
                    break;

                case nsIDataType::VTYPE_ID:
                case nsIDataType::VTYPE_CHAR_STR:
                case nsIDataType::VTYPE_WCHAR_STR:
                    conversionType = nsXPTType((uint8_t)elementType);
                    break;

                case nsIDataType::VTYPE_INTERFACE:
                    pid = &NS_GET_IID(nsISupports);
                    conversionType = nsXPTType((uint8_t)elementType);
                    break;

                case nsIDataType::VTYPE_INTERFACE_IS:
                    pid = &du.u.array.mArrayInterfaceID;
                    conversionType = nsXPTType((uint8_t)elementType);
                    break;

                // The rest are illegal.
                case nsIDataType::VTYPE_VOID:
                case nsIDataType::VTYPE_ASTRING:
                case nsIDataType::VTYPE_DOMSTRING:
                case nsIDataType::VTYPE_CSTRING:
                case nsIDataType::VTYPE_UTF8STRING:
                case nsIDataType::VTYPE_WSTRING_SIZE_IS:
                case nsIDataType::VTYPE_STRING_SIZE_IS:
                case nsIDataType::VTYPE_ARRAY:
                case nsIDataType::VTYPE_EMPTY_ARRAY:
                case nsIDataType::VTYPE_EMPTY:
                default:
                    NS_ERROR("bad type in array!");
                    nsVariant::Cleanup(&du);
                    return false;
            }

            bool success =
                XPCConvert::NativeArray2JS(pJSVal,
                                           (const void**)&du.u.array.mArrayValue,
                                           conversionType, pid,
                                           du.u.array.mArrayCount, pErr);

            nsVariant::Cleanup(&du);
            return success;
        }
        case nsIDataType::VTYPE_EMPTY_ARRAY:
        {
            JSObject* array = JS_NewArrayObject(cx, 0);
            if (!array)
                return false;
            pJSVal.setObject(*array);
            return true;
        }
        case nsIDataType::VTYPE_VOID:
            pJSVal.setUndefined();
            return true;
        case nsIDataType::VTYPE_EMPTY:
            pJSVal.setNull();
            return true;
        default:
            NS_ERROR("bad type in variant!");
            return false;
    }
}

/***************************************************************************/
/***************************************************************************/
// XXX These default implementations need to be improved to allow for
// some more interesting conversions.


/* readonly attribute uint16_t dataType; */
NS_IMETHODIMP XPCVariant::GetDataType(uint16_t* aDataType)
{
    *aDataType = mData.mType;
    return NS_OK;
}

/* uint8_t getAsInt8 (); */
NS_IMETHODIMP XPCVariant::GetAsInt8(uint8_t* _retval)
{
    return nsVariant::ConvertToInt8(mData, _retval);
}

/* int16_t getAsInt16 (); */
NS_IMETHODIMP XPCVariant::GetAsInt16(int16_t* _retval)
{
    return nsVariant::ConvertToInt16(mData, _retval);
}

/* int32_t getAsInt32 (); */
NS_IMETHODIMP XPCVariant::GetAsInt32(int32_t* _retval)
{
    return nsVariant::ConvertToInt32(mData, _retval);
}

/* int64_t getAsInt64 (); */
NS_IMETHODIMP XPCVariant::GetAsInt64(int64_t* _retval)
{
    return nsVariant::ConvertToInt64(mData, _retval);
}

/* uint8_t getAsUint8 (); */
NS_IMETHODIMP XPCVariant::GetAsUint8(uint8_t* _retval)
{
    return nsVariant::ConvertToUint8(mData, _retval);
}

/* uint16_t getAsUint16 (); */
NS_IMETHODIMP XPCVariant::GetAsUint16(uint16_t* _retval)
{
    return nsVariant::ConvertToUint16(mData, _retval);
}

/* uint32_t getAsUint32 (); */
NS_IMETHODIMP XPCVariant::GetAsUint32(uint32_t* _retval)
{
    return nsVariant::ConvertToUint32(mData, _retval);
}

/* uint64_t getAsUint64 (); */
NS_IMETHODIMP XPCVariant::GetAsUint64(uint64_t* _retval)
{
    return nsVariant::ConvertToUint64(mData, _retval);
}

/* float getAsFloat (); */
NS_IMETHODIMP XPCVariant::GetAsFloat(float* _retval)
{
    return nsVariant::ConvertToFloat(mData, _retval);
}

/* double getAsDouble (); */
NS_IMETHODIMP XPCVariant::GetAsDouble(double* _retval)
{
    return nsVariant::ConvertToDouble(mData, _retval);
}

/* bool getAsBool (); */
NS_IMETHODIMP XPCVariant::GetAsBool(bool* _retval)
{
    return nsVariant::ConvertToBool(mData, _retval);
}

/* char getAsChar (); */
NS_IMETHODIMP XPCVariant::GetAsChar(char* _retval)
{
    return nsVariant::ConvertToChar(mData, _retval);
}

/* wchar getAsWChar (); */
NS_IMETHODIMP XPCVariant::GetAsWChar(char16_t* _retval)
{
    return nsVariant::ConvertToWChar(mData, _retval);
}

/* [notxpcom] nsresult getAsID (out nsID retval); */
NS_IMETHODIMP_(nsresult) XPCVariant::GetAsID(nsID* retval)
{
    return nsVariant::ConvertToID(mData, retval);
}

/* AString getAsAString (); */
NS_IMETHODIMP XPCVariant::GetAsAString(nsAString & _retval)
{
    return nsVariant::ConvertToAString(mData, _retval);
}

/* DOMString getAsDOMString (); */
NS_IMETHODIMP XPCVariant::GetAsDOMString(nsAString & _retval)
{
    // A DOMString maps to an AString internally, so we can re-use
    // ConvertToAString here.
    return nsVariant::ConvertToAString(mData, _retval);
}

/* ACString getAsACString (); */
NS_IMETHODIMP XPCVariant::GetAsACString(nsACString & _retval)
{
    return nsVariant::ConvertToACString(mData, _retval);
}

/* AUTF8String getAsAUTF8String (); */
NS_IMETHODIMP XPCVariant::GetAsAUTF8String(nsAUTF8String & _retval)
{
    return nsVariant::ConvertToAUTF8String(mData, _retval);
}

/* string getAsString (); */
NS_IMETHODIMP XPCVariant::GetAsString(char** _retval)
{
    return nsVariant::ConvertToString(mData, _retval);
}

/* wstring getAsWString (); */
NS_IMETHODIMP XPCVariant::GetAsWString(char16_t** _retval)
{
    return nsVariant::ConvertToWString(mData, _retval);
}

/* nsISupports getAsISupports (); */
NS_IMETHODIMP XPCVariant::GetAsISupports(nsISupports** _retval)
{
    return nsVariant::ConvertToISupports(mData, _retval);
}

/* void getAsInterface (out nsIIDPtr iid, [iid_is (iid), retval] out nsQIResult iface); */
NS_IMETHODIMP XPCVariant::GetAsInterface(nsIID * *iid, void * *iface)
{
    return nsVariant::ConvertToInterface(mData, iid, iface);
}


/* [notxpcom] nsresult getAsArray (out uint16_t type, out nsIID iid, out uint32_t count, out voidPtr ptr); */
NS_IMETHODIMP_(nsresult) XPCVariant::GetAsArray(uint16_t* type, nsIID* iid, uint32_t* count, void * *ptr)
{
    return nsVariant::ConvertToArray(mData, type, iid, count, ptr);
}

/* void getAsStringWithSize (out uint32_t size, [size_is (size), retval] out string str); */
NS_IMETHODIMP XPCVariant::GetAsStringWithSize(uint32_t* size, char** str)
{
    return nsVariant::ConvertToStringWithSize(mData, size, str);
}

/* void getAsWStringWithSize (out uint32_t size, [size_is (size), retval] out wstring str); */
NS_IMETHODIMP XPCVariant::GetAsWStringWithSize(uint32_t* size, char16_t** str)
{
    return nsVariant::ConvertToWStringWithSize(mData, size, str);
}
