/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ctypes/Library.h"

#include "prlink.h"

#include "ctypes/CTypes.h"

namespace js {
namespace ctypes {

/*******************************************************************************
** JSAPI function prototypes
*******************************************************************************/

namespace Library
{
  static void Finalize(JSFreeOp *fop, JSObject* obj);

  static bool Close(JSContext* cx, unsigned argc, jsval* vp);
  static bool Declare(JSContext* cx, unsigned argc, jsval* vp);
}

/*******************************************************************************
** JSObject implementation
*******************************************************************************/

typedef Rooted<JSFlatString*>    RootedFlatString;

static const JSClass sLibraryClass = {
  "Library",
  JSCLASS_HAS_RESERVED_SLOTS(LIBRARY_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, Library::Finalize
};

#define CTYPESFN_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

static const JSFunctionSpec sLibraryFunctions[] = {
  JS_FN("close",   Library::Close,   0, CTYPESFN_FLAGS),
  JS_FN("declare", Library::Declare, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

bool
Library::Name(JSContext* cx, unsigned argc, jsval *vp)
{
  if (argc != 1) {
    JS_ReportError(cx, "libraryName takes one argument");
    return false;
  }

  jsval arg = JS_ARGV(cx, vp)[0];
  JSString* str = nullptr;
  if (JSVAL_IS_STRING(arg)) {
    str = JSVAL_TO_STRING(arg);
  }
  else {
    JS_ReportError(cx, "name argument must be a string");
      return false;
  }

  AutoString resultString;
  AppendString(resultString, DLL_PREFIX);
  AppendString(resultString, str);
  AppendString(resultString, DLL_SUFFIX);

  JSString *result = JS_NewUCStringCopyN(cx, resultString.begin(),
                                         resultString.length());
  if (!result)
    return false;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return true;
}

JSObject*
Library::Create(JSContext* cx, jsval path_, JSCTypesCallbacks* callbacks)
{
  RootedValue path(cx, path_);
  RootedObject libraryObj(cx,
                          JS_NewObject(cx, &sLibraryClass, NullPtr(), NullPtr()));
  if (!libraryObj)
    return nullptr;

  // initialize the library
  JS_SetReservedSlot(libraryObj, SLOT_LIBRARY, PRIVATE_TO_JSVAL(nullptr));

  // attach API functions
  if (!JS_DefineFunctions(cx, libraryObj, sLibraryFunctions))
    return nullptr;

  if (!JSVAL_IS_STRING(path)) {
    JS_ReportError(cx, "open takes a string argument");
    return nullptr;
  }

  PRLibSpec libSpec;
  RootedFlatString pathStr(cx, JS_FlattenString(cx, JSVAL_TO_STRING(path)));
  if (!pathStr)
    return nullptr;
#ifdef XP_WIN
  // On Windows, converting to native charset may corrupt path string.
  // So, we have to use Unicode path directly.
  const char16_t* pathChars = JS_GetFlatStringChars(pathStr);
  if (!pathChars)
    return nullptr;

  libSpec.value.pathname_u = pathChars;
  libSpec.type = PR_LibSpec_PathnameU;
#else
  // Convert to platform native charset if the appropriate callback has been
  // provided.
  char* pathBytes;
  if (callbacks && callbacks->unicodeToNative) {
    pathBytes = 
      callbacks->unicodeToNative(cx, pathStr->chars(), pathStr->length());
    if (!pathBytes)
      return nullptr;

  } else {
    // Fallback: assume the platform native charset is UTF-8. This is true
    // for Mac OS X, Android, and probably Linux.
    size_t nbytes =
      GetDeflatedUTF8StringLength(cx, pathStr->chars(), pathStr->length());
    if (nbytes == (size_t) -1)
      return nullptr;

    pathBytes = static_cast<char*>(JS_malloc(cx, nbytes + 1));
    if (!pathBytes)
      return nullptr;

    ASSERT_OK(DeflateStringToUTF8Buffer(cx, pathStr->chars(),
                pathStr->length(), pathBytes, &nbytes));
    pathBytes[nbytes] = 0;
  }

  libSpec.value.pathname = pathBytes;
  libSpec.type = PR_LibSpec_Pathname;
#endif

  PRLibrary* library = PR_LoadLibraryWithFlags(libSpec, 0);

  if (!library) {
#ifdef XP_WIN
    JS_ReportError(cx, "couldn't open library %hs", pathChars);
#else
    JS_ReportError(cx, "couldn't open library %s", pathBytes);
    JS_free(cx, pathBytes);
#endif
    return nullptr;
  }

#ifndef XP_WIN
  JS_free(cx, pathBytes);
#endif

  // stash the library
  JS_SetReservedSlot(libraryObj, SLOT_LIBRARY, PRIVATE_TO_JSVAL(library));

  return libraryObj;
}

bool
Library::IsLibrary(JSObject* obj)
{
  return JS_GetClass(obj) == &sLibraryClass;
}

PRLibrary*
Library::GetLibrary(JSObject* obj)
{
  JS_ASSERT(IsLibrary(obj));

  jsval slot = JS_GetReservedSlot(obj, SLOT_LIBRARY);
  return static_cast<PRLibrary*>(JSVAL_TO_PRIVATE(slot));
}

static void
UnloadLibrary(JSObject* obj)
{
  PRLibrary* library = Library::GetLibrary(obj);
  if (library)
    PR_UnloadLibrary(library);
}

void
Library::Finalize(JSFreeOp *fop, JSObject* obj)
{
  UnloadLibrary(obj);
}

bool
Library::Open(JSContext* cx, unsigned argc, jsval *vp)
{
  JSObject* ctypesObj = JS_THIS_OBJECT(cx, vp);
  if (!ctypesObj)
    return false;
  if (!IsCTypesGlobal(ctypesObj)) {
    JS_ReportError(cx, "not a ctypes object");
    return false;
  }

  if (argc != 1 || JSVAL_IS_VOID(JS_ARGV(cx, vp)[0])) {
    JS_ReportError(cx, "open requires a single argument");
    return false;
  }

  JSObject* library = Create(cx, JS_ARGV(cx, vp)[0], GetCallbacks(ctypesObj));
  if (!library)
    return false;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(library));
  return true;
}

bool
Library::Close(JSContext* cx, unsigned argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!IsLibrary(obj)) {
    JS_ReportError(cx, "not a library");
    return false;
  }

  if (argc != 0) {
    JS_ReportError(cx, "close doesn't take any arguments");
    return false;
  }

  // delete our internal objects
  UnloadLibrary(obj);
  JS_SetReservedSlot(obj, SLOT_LIBRARY, PRIVATE_TO_JSVAL(nullptr));

  JS_SET_RVAL(cx, vp, JSVAL_VOID);
  return true;
}

bool
Library::Declare(JSContext* cx, unsigned argc, jsval* vp)
{
  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!IsLibrary(obj)) {
    JS_ReportError(cx, "not a library");
    return false;
  }

  PRLibrary* library = GetLibrary(obj);
  if (!library) {
    JS_ReportError(cx, "library not open");
    return false;
  }

  // We allow two API variants:
  // 1) library.declare(name, abi, returnType, argType1, ...)
  //    declares a function with the given properties, and resolves the symbol
  //    address in the library.
  // 2) library.declare(name, type)
  //    declares a symbol of 'type', and resolves it. The object that comes
  //    back will be of type 'type', and will point into the symbol data.
  //    This data will be both readable and writable via the usual CData
  //    accessors. If 'type' is a PointerType to a FunctionType, the result will
  //    be a function pointer, as with 1). 
  if (argc < 2) {
    JS_ReportError(cx, "declare requires at least two arguments");
    return false;
  }

  jsval* argv = JS_ARGV(cx, vp);
  if (!JSVAL_IS_STRING(argv[0])) {
    JS_ReportError(cx, "first argument must be a string");
    return false;
  }

  RootedObject fnObj(cx, nullptr);
  RootedObject typeObj(cx);
  bool isFunction = argc > 2;
  if (isFunction) {
    // Case 1).
    // Create a FunctionType representing the function.
    fnObj = FunctionType::CreateInternal(cx,
              argv[1], argv[2], &argv[3], argc - 3);
    if (!fnObj)
      return false;

    // Make a function pointer type.
    typeObj = PointerType::CreateInternal(cx, fnObj);
    if (!typeObj)
      return false;
  } else {
    // Case 2).
    if (JSVAL_IS_PRIMITIVE(argv[1]) ||
        !CType::IsCType(JSVAL_TO_OBJECT(argv[1])) ||
        !CType::IsSizeDefined(JSVAL_TO_OBJECT(argv[1]))) {
      JS_ReportError(cx, "second argument must be a type of defined size");
      return false;
    }

    typeObj = JSVAL_TO_OBJECT(argv[1]);
    if (CType::GetTypeCode(typeObj) == TYPE_pointer) {
      fnObj = PointerType::GetBaseType(typeObj);
      isFunction = fnObj && CType::GetTypeCode(fnObj) == TYPE_function;
    }
  }

  void* data;
  PRFuncPtr fnptr;
  JSString* nameStr = JSVAL_TO_STRING(argv[0]);
  AutoCString symbol;
  if (isFunction) {
    // Build the symbol, with mangling if necessary.
    FunctionType::BuildSymbolName(nameStr, fnObj, symbol);
    AppendString(symbol, "\0");

    // Look up the function symbol.
    fnptr = PR_FindFunctionSymbol(library, symbol.begin());
    if (!fnptr) {
      JS_ReportError(cx, "couldn't find function symbol in library");
      return false;
    }
    data = &fnptr;

  } else {
    // 'typeObj' is another data type. Look up the data symbol.
    AppendString(symbol, nameStr);
    AppendString(symbol, "\0");

    data = PR_FindSymbol(library, symbol.begin());
    if (!data) {
      JS_ReportError(cx, "couldn't find symbol in library");
      return false;
    }
  }

  RootedObject result(cx, CData::Create(cx, typeObj, obj, data, isFunction));
  if (!result)
    return false;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  // Seal the CData object, to prevent modification of the function pointer.
  // This permanently associates this object with the library, and avoids
  // having to do things like reset SLOT_REFERENT when someone tries to
  // change the pointer value.
  // XXX This will need to change when bug 541212 is fixed -- CData::ValueSetter
  // could be called on a sealed object.
  if (isFunction && !JS_FreezeObject(cx, result))
    return false;

  return true;
}

}
}

