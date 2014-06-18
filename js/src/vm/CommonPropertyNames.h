/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating all cached property names. */

#ifndef vm_CommonPropertyNames_h
#define vm_CommonPropertyNames_h

#include "jsprototypes.h"

#define FOR_EACH_COMMON_PROPERTYNAME(macro) \
    macro(anonymous, anonymous, "anonymous") \
    macro(Any, Any, "Any") \
    macro(apply, apply, "apply") \
    macro(arguments, arguments, "arguments") \
    macro(as, as, "as") \
    macro(ArrayIteratorNext, ArrayIteratorNext, "ArrayIteratorNext") \
    macro(ArrayType, ArrayType, "ArrayType") \
    macro(ArrayValues, ArrayValues, "ArrayValues") \
    macro(buffer, buffer, "buffer") \
    macro(builder, builder, "builder") \
    macro(byteLength, byteLength, "byteLength") \
    macro(byteAlignment, byteAlignment, "byteAlignment") \
    macro(byteOffset, byteOffset, "byteOffset") \
    macro(bytes, bytes, "bytes") \
    macro(BYTES_PER_ELEMENT, BYTES_PER_ELEMENT, "BYTES_PER_ELEMENT") \
    macro(call, call, "call") \
    macro(callee, callee, "callee") \
    macro(caller, caller, "caller") \
    macro(callFunction, callFunction, "callFunction") \
    macro(caseFirst, caseFirst, "caseFirst") \
    macro(Collator, Collator, "Collator") \
    macro(CollatorCompareGet, CollatorCompareGet, "Intl_Collator_compare_get") \
    macro(columnNumber, columnNumber, "columnNumber") \
    macro(comma, comma, ",") \
    macro(compare, compare, "compare") \
    macro(configurable, configurable, "configurable") \
    macro(construct, construct, "construct") \
    macro(constructor, constructor, "constructor") \
    macro(ConvertAndCopyTo, ConvertAndCopyTo, "ConvertAndCopyTo") \
    macro(currency, currency, "currency") \
    macro(currencyDisplay, currencyDisplay, "currencyDisplay") \
    macro(std_iterator, std_iterator, "@@iterator") \
    macro(DateTimeFormat, DateTimeFormat, "DateTimeFormat") \
    macro(DateTimeFormatFormatGet, DateTimeFormatFormatGet, "Intl_DateTimeFormat_format_get") \
    macro(decodeURI, decodeURI, "decodeURI") \
    macro(decodeURIComponent, decodeURIComponent, "decodeURIComponent") \
    macro(default_, default_, "default") \
    macro(defineProperty, defineProperty, "defineProperty") \
    macro(defineGetter, defineGetter, "__defineGetter__") \
    macro(defineSetter, defineSetter, "__defineSetter__") \
    macro(delete, delete_, "delete") \
    macro(deleteProperty, deleteProperty, "deleteProperty") \
    macro(displayURL, displayURL, "displayURL") \
    macro(done, done, "done") \
    macro(each, each, "each") \
    macro(elementType, elementType, "elementType") \
    macro(empty, empty, "") \
    macro(encodeURI, encodeURI, "encodeURI") \
    macro(encodeURIComponent, encodeURIComponent, "encodeURIComponent") \
    macro(enumerable, enumerable, "enumerable") \
    macro(enumerate, enumerate, "enumerate") \
    macro(escape, escape, "escape") \
    macro(eval, eval, "eval") \
    macro(false, false_, "false") \
    macro(fieldOffsets, fieldOffsets, "fieldOffsets") \
    macro(fieldTypes, fieldTypes, "fieldTypes") \
    macro(fileName, fileName, "fileName") \
    macro(fix, fix, "fix") \
    macro(float32, float32, "float32") \
    macro(float32x4, float32x4, "float32x4") \
    macro(float64, float64, "float64") \
    macro(format, format, "format") \
    macro(from, from, "from") \
    macro(get, get, "get") \
    macro(getInternals, getInternals, "getInternals") \
    macro(getOwnPropertyDescriptor, getOwnPropertyDescriptor, "getOwnPropertyDescriptor") \
    macro(getOwnPropertyNames, getOwnPropertyNames, "getOwnPropertyNames") \
    macro(getPropertyDescriptor, getPropertyDescriptor, "getPropertyDescriptor") \
    macro(global, global, "global") \
    macro(Handle, Handle, "Handle") \
    macro(has, has, "has") \
    macro(hasOwn, hasOwn, "hasOwn") \
    macro(hasOwnProperty, hasOwnProperty, "hasOwnProperty") \
    macro(ignoreCase, ignoreCase, "ignoreCase") \
    macro(ignorePunctuation, ignorePunctuation, "ignorePunctuation") \
    macro(index, index, "index") \
    macro(InitializeCollator, InitializeCollator, "InitializeCollator") \
    macro(InitializeDateTimeFormat, InitializeDateTimeFormat, "InitializeDateTimeFormat") \
    macro(InitializeNumberFormat, InitializeNumberFormat, "InitializeNumberFormat") \
    macro(innermost, innermost, "innermost") \
    macro(input, input, "input") \
    macro(int32x4, int32x4, "int32x4") \
    macro(isFinite, isFinite, "isFinite") \
    macro(isNaN, isNaN, "isNaN") \
    macro(isPrototypeOf, isPrototypeOf, "isPrototypeOf") \
    macro(iterate, iterate, "iterate") \
    macro(Infinity, Infinity, "Infinity") \
    macro(int8, int8, "int8") \
    macro(int16, int16, "int16") \
    macro(int32, int32, "int32") \
    macro(isExtensible, isExtensible, "isExtensible") \
    macro(iterator, iterator, "iterator") \
    macro(iteratorIntrinsic, iteratorIntrinsic, "__iterator__") \
    macro(join, join, "join") \
    macro(keys, keys, "keys") \
    macro(lastIndex, lastIndex, "lastIndex") \
    macro(length, length, "length") \
    macro(let, let, "let") \
    macro(line, line, "line") \
    macro(lineNumber, lineNumber, "lineNumber") \
    macro(loc, loc, "loc") \
    macro(locale, locale, "locale") \
    macro(lookupGetter, lookupGetter, "__lookupGetter__") \
    macro(lookupSetter, lookupSetter, "__lookupSetter__") \
    macro(maximumFractionDigits, maximumFractionDigits, "maximumFractionDigits") \
    macro(maximumSignificantDigits, maximumSignificantDigits, "maximumSignificantDigits") \
    macro(message, message, "message") \
    macro(minimumFractionDigits, minimumFractionDigits, "minimumFractionDigits") \
    macro(minimumIntegerDigits, minimumIntegerDigits, "minimumIntegerDigits") \
    macro(minimumSignificantDigits, minimumSignificantDigits, "minimumSignificantDigits") \
    macro(module, module, "module") \
    macro(multiline, multiline, "multiline") \
    macro(name, name, "name") \
    macro(NaN, NaN, "NaN") \
    macro(next, next, "next") \
    macro(NFC, NFC, "NFC") \
    macro(NFD, NFD, "NFD") \
    macro(NFKC, NFKC, "NFKC") \
    macro(NFKD, NFKD, "NFKD") \
    macro(noSuchMethod, noSuchMethod, "__noSuchMethod__") \
    macro(NumberFormat, NumberFormat, "NumberFormat") \
    macro(NumberFormatFormatGet, NumberFormatFormatGet, "Intl_NumberFormat_format_get") \
    macro(numeric, numeric, "numeric") \
    macro(objectArray, objectArray, "[object Array]") \
    macro(objectFunction, objectFunction, "[object Function]") \
    macro(objectNull, objectNull, "[object Null]") \
    macro(objectNumber, objectNumber, "[object Number]") \
    macro(objectObject, objectObject, "[object Object]") \
    macro(objectString, objectString, "[object String]") \
    macro(objectUndefined, objectUndefined, "[object Undefined]") \
    macro(objectWindow, objectWindow, "[object Window]") \
    macro(of, of, "of") \
    macro(offset, offset, "offset") \
    macro(optimizedOut, optimizedOut, "optimizedOut") \
    macro(missingArguments, missingArguments, "missingArguments") \
    macro(outOfMemory, outOfMemory, "out of memory") \
    macro(parseFloat, parseFloat, "parseFloat") \
    macro(parseInt, parseInt, "parseInt") \
    macro(pattern, pattern, "pattern") \
    macro(preventExtensions, preventExtensions, "preventExtensions") \
    macro(propertyIsEnumerable, propertyIsEnumerable, "propertyIsEnumerable") \
    macro(proto, proto, "__proto__") \
    macro(prototype, prototype, "prototype") \
    macro(Reify, Reify, "Reify") \
    macro(return, return_, "return") \
    macro(sensitivity, sensitivity, "sensitivity") \
    macro(set, set, "set") \
    macro(shape, shape, "shape") \
    macro(source, source, "source") \
    macro(stack, stack, "stack") \
    macro(sticky, sticky, "sticky") \
    macro(StructType, StructType, "StructType") \
    macro(style, style, "style") \
    macro(test, test, "test") \
    macro(throw, throw_, "throw") \
    macro(timeZone, timeZone, "timeZone") \
    macro(toGMTString, toGMTString, "toGMTString") \
    macro(toISOString, toISOString, "toISOString") \
    macro(toJSON, toJSON, "toJSON") \
    macro(toLocaleString, toLocaleString, "toLocaleString") \
    macro(toSource, toSource, "toSource") \
    macro(toString, toString, "toString") \
    macro(toUTCString, toUTCString, "toUTCString") \
    macro(true, true_, "true") \
    macro(unescape, unescape, "unescape") \
    macro(uneval, uneval, "uneval") \
    macro(uint8, uint8, "uint8") \
    macro(uint8Clamped, uint8Clamped, "uint8Clamped") \
    macro(uint16, uint16, "uint16") \
    macro(uint32, uint32, "uint32") \
    macro(unsized, unsized, "unsized") \
    macro(unwatch, unwatch, "unwatch") \
    macro(url, url, "url") \
    macro(usage, usage, "usage") \
    macro(useGrouping, useGrouping, "useGrouping") \
    macro(useAsm, useAsm, "use asm") \
    macro(useStrict, useStrict, "use strict") \
    macro(value, value, "value") \
    macro(valueOf, valueOf, "valueOf") \
    macro(var, var, "var") \
    macro(variable, variable, "variable") \
    macro(void0, void0, "(void 0)") \
    macro(watch, watch, "watch") \
    macro(writable, writable, "writable") \
    macro(w, w, "w") \
    macro(x, x, "x") \
    macro(y, y, "y") \
    macro(yield, yield, "yield") \
    macro(z, z, "z") \
    /* Type names must be contiguous and ordered; see js::TypeName. */ \
    macro(undefined, undefined, "undefined") \
    macro(object, object, "object") \
    macro(function, function, "function") \
    macro(string, string, "string") \
    macro(number, number, "number") \
    macro(boolean, boolean, "boolean") \
    macro(null, null, "null")

#endif /* vm_CommonPropertyNames_h */
