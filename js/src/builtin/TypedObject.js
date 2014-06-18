#include "TypedObjectConstants.h"

///////////////////////////////////////////////////////////////////////////
// Getters and setters for various slots.

// Type object slots

#define DESCR_KIND(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_KIND)
#define DESCR_STRING_REPR(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRING_REPR)
#define DESCR_ALIGNMENT(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_ALIGNMENT)
#define DESCR_SIZE(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_SIZE)
#define DESCR_OPAQUE(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_OPAQUE)
#define DESCR_TYPE(obj)   \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_TYPE)
#define DESCR_ARRAY_ELEMENT_TYPE(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_ARRAY_ELEM_TYPE)
#define DESCR_SIZED_ARRAY_LENGTH(obj) \
    TO_INT32(UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_SIZED_ARRAY_LENGTH))
#define DESCR_STRUCT_FIELD_NAMES(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_NAMES)
#define DESCR_STRUCT_FIELD_TYPES(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_TYPES)
#define DESCR_STRUCT_FIELD_OFFSETS(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS)

// Typed prototype slots

#define TYPROTO_DESCR(obj) \
    UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_DESCR)

// Typed object slots

#define TYPEDOBJ_BYTEOFFSET(obj) \
    TO_INT32(UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_BYTEOFFSET))
#define TYPEDOBJ_OWNER(obj) \
    UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_OWNER)
#define TYPEDOBJ_LENGTH(obj) \
    TO_INT32(UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_LENGTH))

#define HAS_PROPERTY(obj, prop) \
    callFunction(std_Object_hasOwnProperty, obj, prop)

function TypedObjectTypeDescr(typedObj) {
  return TYPROTO_DESCR(typedObj.__proto__);
}

///////////////////////////////////////////////////////////////////////////
// Getting values
//
// The methods in this section read from the memory pointed at
// by `this` and produce JS values. This process is called *reification*
// in the spec.

// Reifies the value referenced by the pointer, meaning that it
// returns a new object pointing at the value. If the value is
// a scalar, it will return a JS number, but otherwise the reified
// result will be a typedObj of the same class as the ptr's typedObj.
function TypedObjectGet(descr, typedObj, offset) {
  assert(IsObject(descr) && ObjectIsTypeDescr(descr),
         "get() called with bad type descr");

  if (!TypedObjectIsAttached(typedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  switch (DESCR_KIND(descr)) {
  case JS_TYPEREPR_SCALAR_KIND:
    return TypedObjectGetScalar(descr, typedObj, offset);

  case JS_TYPEREPR_REFERENCE_KIND:
    return TypedObjectGetReference(descr, typedObj, offset);

  case JS_TYPEREPR_X4_KIND:
    return TypedObjectGetX4(descr, typedObj, offset);

  case JS_TYPEREPR_SIZED_ARRAY_KIND:
  case JS_TYPEREPR_STRUCT_KIND:
    return TypedObjectGetDerived(descr, typedObj, offset);

  case JS_TYPEREPR_UNSIZED_ARRAY_KIND:
    assert(false, "Unhandled repr kind: " + DESCR_KIND(descr));
  }

  assert(false, "Unhandled kind: " + DESCR_KIND(descr));
  return undefined;
}

function TypedObjectGetDerived(descr, typedObj, offset) {
  assert(!TypeDescrIsSimpleType(descr),
         "getDerived() used with simple type");
  return NewDerivedTypedObject(descr, typedObj, offset);
}

function TypedObjectGetDerivedIf(descr, typedObj, offset, cond) {
  return (cond ? TypedObjectGetDerived(descr, typedObj, offset) : undefined);
}

function TypedObjectGetOpaque(descr, typedObj, offset) {
  assert(!TypeDescrIsSimpleType(descr),
         "getDerived() used with simple type");
  var opaqueTypedObj = NewOpaqueTypedObject(descr);
  AttachTypedObject(opaqueTypedObj, typedObj, offset);
  return opaqueTypedObj;
}

function TypedObjectGetOpaqueIf(descr, typedObj, offset, cond) {
  return (cond ? TypedObjectGetOpaque(descr, typedObj, offset) : undefined);
}

function TypedObjectGetScalar(descr, typedObj, offset) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPEREPR_INT8:
    return Load_int8(typedObj, offset);

  case JS_SCALARTYPEREPR_UINT8:
  case JS_SCALARTYPEREPR_UINT8_CLAMPED:
    return Load_uint8(typedObj, offset);

  case JS_SCALARTYPEREPR_INT16:
    return Load_int16(typedObj, offset);

  case JS_SCALARTYPEREPR_UINT16:
    return Load_uint16(typedObj, offset);

  case JS_SCALARTYPEREPR_INT32:
    return Load_int32(typedObj, offset);

  case JS_SCALARTYPEREPR_UINT32:
    return Load_uint32(typedObj, offset);

  case JS_SCALARTYPEREPR_FLOAT32:
    return Load_float32(typedObj, offset);

  case JS_SCALARTYPEREPR_FLOAT64:
    return Load_float64(typedObj, offset);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectGetReference(descr, typedObj, offset) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPEREPR_ANY:
    return Load_Any(typedObj, offset);

  case JS_REFERENCETYPEREPR_OBJECT:
    return Load_Object(typedObj, offset);

  case JS_REFERENCETYPEREPR_STRING:
    return Load_string(typedObj, offset);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectGetX4(descr, typedObj, offset) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_X4TYPEREPR_FLOAT32:
    var x = Load_float32(typedObj, offset + 0);
    var y = Load_float32(typedObj, offset + 4);
    var z = Load_float32(typedObj, offset + 8);
    var w = Load_float32(typedObj, offset + 12);
    return GetFloat32x4TypeDescr()(x, y, z, w);

  case JS_X4TYPEREPR_INT32:
    var x = Load_int32(typedObj, offset + 0);
    var y = Load_int32(typedObj, offset + 4);
    var z = Load_int32(typedObj, offset + 8);
    var w = Load_int32(typedObj, offset + 12);
    return GetInt32x4TypeDescr()(x, y, z, w);
  }

  assert(false, "Unhandled x4 type: " + type);
  return undefined;
}

///////////////////////////////////////////////////////////////////////////
// Setting values
//
// The methods in this section modify the data pointed at by `this`.

// Writes `fromValue` into the `typedObj` at offset `offset`, adapting
// it to `descr` as needed. This is the most general entry point
// and works for any type.
function TypedObjectSet(descr, typedObj, offset, fromValue) {
  if (!TypedObjectIsAttached(typedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  // Fast path: `fromValue` is a typed object with same type
  // representation as the destination. In that case, we can just do a
  // memcpy.
  if (IsObject(fromValue) && ObjectIsTypedObject(fromValue)) {
    if (!descr.variable && DescrsEquiv(descr, TypedObjectTypeDescr(fromValue))) {
      if (!TypedObjectIsAttached(fromValue))
        ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

      var size = DESCR_SIZE(descr);
      Memcpy(typedObj, offset, fromValue, 0, size);
      return;
    }
  }

  switch (DESCR_KIND(descr)) {
  case JS_TYPEREPR_SCALAR_KIND:
    TypedObjectSetScalar(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPEREPR_REFERENCE_KIND:
    TypedObjectSetReference(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPEREPR_X4_KIND:
    TypedObjectSetX4(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPEREPR_SIZED_ARRAY_KIND:
    var length = DESCR_SIZED_ARRAY_LENGTH(descr);
    if (TypedObjectSetArray(descr, length, typedObj, offset, fromValue))
      return;
    break;

  case JS_TYPEREPR_UNSIZED_ARRAY_KIND:
    var length = typedObj.length;
    if (TypedObjectSetArray(descr, length, typedObj, offset, fromValue))
      return;
    break;

  case JS_TYPEREPR_STRUCT_KIND:
    if (!IsObject(fromValue))
      break;

    // Adapt each field.
    var fieldNames = DESCR_STRUCT_FIELD_NAMES(descr);
    var fieldDescrs = DESCR_STRUCT_FIELD_TYPES(descr);
    var fieldOffsets = DESCR_STRUCT_FIELD_OFFSETS(descr);
    for (var i = 0; i < fieldNames.length; i++) {
      var fieldName = fieldNames[i];
      var fieldDescr = fieldDescrs[i];
      var fieldOffset = fieldOffsets[i];
      var fieldValue = fromValue[fieldName];
      TypedObjectSet(fieldDescr, typedObj, offset + fieldOffset, fieldValue);
    }
    return;
  }

  ThrowError(JSMSG_CANT_CONVERT_TO,
             typeof(fromValue),
             DESCR_STRING_REPR(descr));
}

function TypedObjectSetArray(descr, length, typedObj, offset, fromValue) {
  if (!IsObject(fromValue))
    return false;

  // Check that "array-like" fromValue has an appropriate length.
  if (fromValue.length !== length)
    return false;

  // Adapt each element.
  if (length > 0) {
    var elemDescr = DESCR_ARRAY_ELEMENT_TYPE(descr);
    var elemSize = DESCR_SIZE(elemDescr);
    var elemOffset = offset;
    for (var i = 0; i < length; i++) {
      TypedObjectSet(elemDescr, typedObj, elemOffset, fromValue[i]);
      elemOffset += elemSize;
    }
  }
  return true;
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
function TypedObjectSetScalar(descr, typedObj, offset, fromValue) {
  assert(DESCR_KIND(descr) === JS_TYPEREPR_SCALAR_KIND,
         "Expected scalar type descriptor");
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPEREPR_INT8:
    return Store_int8(typedObj, offset,
                     TO_INT32(fromValue) & 0xFF);

  case JS_SCALARTYPEREPR_UINT8:
    return Store_uint8(typedObj, offset,
                      TO_UINT32(fromValue) & 0xFF);

  case JS_SCALARTYPEREPR_UINT8_CLAMPED:
    var v = ClampToUint8(+fromValue);
    return Store_int8(typedObj, offset, v);

  case JS_SCALARTYPEREPR_INT16:
    return Store_int16(typedObj, offset,
                      TO_INT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPEREPR_UINT16:
    return Store_uint16(typedObj, offset,
                       TO_UINT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPEREPR_INT32:
    return Store_int32(typedObj, offset,
                      TO_INT32(fromValue));

  case JS_SCALARTYPEREPR_UINT32:
    return Store_uint32(typedObj, offset,
                       TO_UINT32(fromValue));

  case JS_SCALARTYPEREPR_FLOAT32:
    return Store_float32(typedObj, offset, +fromValue);

  case JS_SCALARTYPEREPR_FLOAT64:
    return Store_float64(typedObj, offset, +fromValue);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectSetReference(descr, typedObj, offset, fromValue) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPEREPR_ANY:
    return Store_Any(typedObj, offset, fromValue);

  case JS_REFERENCETYPEREPR_OBJECT:
    var value = (fromValue === null ? fromValue : ToObject(fromValue));
    return Store_Object(typedObj, offset, value);

  case JS_REFERENCETYPEREPR_STRING:
    return Store_string(typedObj, offset, ToString(fromValue));
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
function TypedObjectSetX4(descr, typedObj, offset, fromValue) {
  // It is only permitted to set a float32x4/int32x4 value from another
  // float32x4/int32x4; in that case, the "fast path" that uses memcopy will
  // have already matched. So if we get to this point, we're supposed
  // to "adapt" fromValue, but there are no legal adaptions.
  ThrowError(JSMSG_CANT_CONVERT_TO,
             typeof(fromValue),
             DESCR_STRING_REPR(descr));
}

///////////////////////////////////////////////////////////////////////////
// C++ Wrappers
//
// These helpers are invoked by C++ code or used as method bodies.

// Wrapper for use from C++ code.
function ConvertAndCopyTo(destDescr,
                          destTypedObj,
                          destOffset,
                          fromValue)
{
  assert(IsObject(destDescr) && ObjectIsTypeDescr(destDescr),
         "ConvertAndCopyTo: not type obj");
  assert(IsObject(destTypedObj) && ObjectIsTypedObject(destTypedObj),
         "ConvertAndCopyTo: not type typedObj");

  if (!TypedObjectIsAttached(destTypedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  TypedObjectSet(destDescr, destTypedObj, destOffset, fromValue);
}

// Wrapper for use from C++ code.
function Reify(sourceDescr,
               sourceTypedObj,
               sourceOffset) {
  assert(IsObject(sourceDescr) && ObjectIsTypeDescr(sourceDescr),
         "Reify: not type obj");
  assert(IsObject(sourceTypedObj) && ObjectIsTypedObject(sourceTypedObj),
         "Reify: not type typedObj");

  if (!TypedObjectIsAttached(sourceTypedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  return TypedObjectGet(sourceDescr, sourceTypedObj, sourceOffset);
}

// Warning: user exposed!
function TypeDescrEquivalent(otherDescr) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  if (!IsObject(otherDescr) || !ObjectIsTypeDescr(otherDescr))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return DescrsEquiv(this, otherDescr);
}

// TypedArray.redimension(newArrayType)
//
// Method that "repackages" the data from this array into a new typed
// object whose type is `newArrayType`. Once you strip away all the
// outer array dimensions, the type of `this` array and `newArrayType`
// must share the same innermost element type. Moreover, those
// stripped away dimensions must amount to the same total number of
// elements.
//
// For example, given two equivalent types `T` and `U`, it is legal to
// interconvert between arrays types like:
//     T[32]
//     U[2][16]
//     U[2][2][8]
// Because they all share the same total number (32) of equivalent elements.
// But it would be illegal to convert `T[32]` to `U[31]` or `U[2][17]`, since
// the number of elements differs. And it's just plain incompatible to convert
// if the base element types are not equivalent.
//
// Warning: user exposed!
function TypedArrayRedimension(newArrayType) {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (!IsObject(newArrayType) || !ObjectIsTypeDescr(newArrayType))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Peel away the outermost array layers from the type of `this` to find
  // the core element type. In the process, count the number of elements.
  var oldArrayType = TypedObjectTypeDescr(this);
  var oldArrayReprKind = DESCR_KIND(oldArrayType);
  var oldElementType = oldArrayType;
  var oldElementCount = 1;
  switch (oldArrayReprKind) {
  case JS_TYPEREPR_UNSIZED_ARRAY_KIND:
    oldElementCount *= this.length;
    oldElementType = oldElementType.elementType;
    break;

  case JS_TYPEREPR_SIZED_ARRAY_KIND:
    break;

  default:
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }
  while (DESCR_KIND(oldElementType) === JS_TYPEREPR_SIZED_ARRAY_KIND) {
    oldElementCount *= oldElementType.length;
    oldElementType = oldElementType.elementType;
  }

  // Peel away the outermost array layers from `newArrayType`. In the
  // process, count the number of elements.
  var newElementType = newArrayType;
  var newElementCount = 1;
  while (DESCR_KIND(newElementType) == JS_TYPEREPR_SIZED_ARRAY_KIND) {
    newElementCount *= newElementType.length;
    newElementType = newElementType.elementType;
  }

  // Check that the total number of elements does not change.
  if (oldElementCount !== newElementCount) {
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }

  // Check that the element types are equivalent.
  if (!DescrsEquiv(oldElementType, newElementType)) {
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }

  // Together, this should imply that the sizes are unchanged.
  assert(DESCR_SIZE(oldArrayType) == DESCR_SIZE(newArrayType),
         "Byte sizes should be equal");

  // Rewrap the data from `this` in a new type.
  return NewDerivedTypedObject(newArrayType, this, 0);
}

///////////////////////////////////////////////////////////////////////////
// X4

function X4ProtoString(type) {
  switch (type) {
  case JS_X4TYPEREPR_INT32:
    return "int32x4";
  case JS_X4TYPEREPR_FLOAT32:
    return "float32x4";
  }

  assert(false, "Unhandled type constant");
  return undefined;
}

function X4ToSource() {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "X4", "toSource", typeof this);

  var descr = TypedObjectTypeDescr(this);

  if (DESCR_KIND(descr) != JS_TYPEREPR_X4_KIND)
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "X4", "toSource", typeof this);

  var type = DESCR_TYPE(descr);
  return X4ProtoString(type)+"("+this.x+", "+this.y+", "+this.z+", "+this.w+")";
}

///////////////////////////////////////////////////////////////////////////
// Miscellaneous

function DescrsEquiv(descr1, descr2) {
  assert(IsObject(descr1) && ObjectIsTypeDescr(descr1), "descr1 not descr");
  assert(IsObject(descr2) && ObjectIsTypeDescr(descr2), "descr2 not descr");

  // Potential optimization: these two strings are guaranteed to be
  // atoms, and hence this string comparison can just be a pointer
  // comparison.  However, I don't think ion knows that. If this ever
  // becomes a bottleneck, we can add a intrinsic at some point that
  // is treated specially by Ion.  (Bug 976688)

  return DESCR_STRING_REPR(descr1) === DESCR_STRING_REPR(descr2);
}

// toSource() for type descriptors.
//
// Warning: user exposed!
function DescrToSource() {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "Type", "toSource", "value");

  return DESCR_STRING_REPR(this);
}

// Warning: user exposed!
function ArrayShorthand(...dims) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var T = GetTypedObjectModule();

  if (dims.length == 0)
    return new T.ArrayType(this);

  var accum = this;
  for (var i = dims.length - 1; i >= 0; i--)
    accum = new T.ArrayType(accum).dimension(dims[i]);
  return accum;
}

// This is the `storage()` function defined in the spec.  When
// provided with a *transparent* typed object, it returns an object
// containing buffer, byteOffset, byteLength. When given an opaque
// typed object, it returns null. Otherwise it throws.
//
// Warning: user exposed!
function StorageOfTypedObject(obj) {
  if (IsObject(obj)) {
    if (ObjectIsOpaqueTypedObject(obj))
      return null;

    if (ObjectIsTransparentTypedObject(obj)) {
      var descr = TypedObjectTypeDescr(obj);
      var byteLength;
      if (DESCR_KIND(descr) == JS_TYPEREPR_UNSIZED_ARRAY_KIND)
        byteLength = DESCR_SIZE(descr.elementType) * obj.length;
      else
        byteLength = DESCR_SIZE(descr);

      return { buffer: TYPEDOBJ_OWNER(obj),
               byteLength: byteLength,
               byteOffset: TYPEDOBJ_BYTEOFFSET(obj) };
    }
  }

  ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return null; // pacify silly "always returns a value" lint
}

// This is the `objectType()` function defined in the spec.
// It returns the type of its argument.
//
// Warning: user exposed!
function TypeOfTypedObject(obj) {
  if (IsObject(obj) && ObjectIsTypedObject(obj))
    return TypedObjectTypeDescr(obj);

  // Note: Do not create bindings for `Any`, `String`, etc in
  // Utilities.js, but rather access them through
  // `GetTypedObjectModule()`. The reason is that bindings
  // you create in Utilities.js are part of the self-hosted global,
  // vs the user-accessible global, and hence should not escape to
  // user script.
  var T = GetTypedObjectModule();
  switch (typeof obj) {
    case "object": return T.Object;
    case "function": return T.Object;
    case "string": return T.String;
    case "number": return T.float64;
    case "undefined": return T.Any;
    default: return T.Any;
  }
}

///////////////////////////////////////////////////////////////////////////
// TypedObject surface API methods (sequential implementations).

// Warning: user exposed!
function TypedObjectArrayTypeBuild(a,b,c) {
  // Arguments (this sized) : [depth], func
  // Arguments (this unsized) : length, [depth], func

  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var kind = DESCR_KIND(this);
  switch (kind) {
  case JS_TYPEREPR_SIZED_ARRAY_KIND:
    if (typeof a === "function") // XXX here and elsewhere: these type dispatches are fragile at best.
      return BuildTypedSeqImpl(this, this.length, 1, a);
    else if (typeof a === "number" && typeof b === "function")
      return BuildTypedSeqImpl(this, this.length, a, b);
    else if (typeof a === "number")
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
    else
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  case JS_TYPEREPR_UNSIZED_ARRAY_KIND:
    var len = a;
    if (typeof b === "function")
      return BuildTypedSeqImpl(this, len, 1, b);
    else if (typeof b === "number" && typeof c === "function")
      return BuildTypedSeqImpl(this, len, b, c);
    else if (typeof b === "number")
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
    else
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  default:
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }
}

// Warning: user exposed!
function TypedObjectArrayTypeFrom(a, b, c) {
  // Arguments: arrayLike, [depth], func

  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var untypedInput = !IsObject(a) || !ObjectIsTypedObject(a);

  // for untyped input array, the expectation (in terms of error
  // reporting for invalid parameters) is no-depth, despite
  // supporting an explicit depth of 1; while for typed input array,
  // the expectation is explicit depth.

  if (untypedInput) {
    var explicitDepth = (b === 1);
    if (explicitDepth && IsCallable(c))
      return MapUntypedSeqImpl(a, this, c);
    else if (IsCallable(b))
      return MapUntypedSeqImpl(a, this, b);
    else
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  } else {
    var explicitDepth = (typeof b === "number");
    if (explicitDepth && IsCallable(c))
      return MapTypedSeqImpl(a, b, this, c);
    else if (IsCallable(b))
      return MapTypedSeqImpl(a, 1, this, b);
    else if (explicitDepth)
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
    else
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }
}

// Warning: user exposed!
function TypedArrayMap(a, b) {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = TypedObjectTypeDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Arguments: [depth], func
  if (typeof a === "number" && typeof b === "function")
    return MapTypedSeqImpl(this, a, thisType, b);
  else if (typeof a === "function")
    return MapTypedSeqImpl(this, 1, thisType, a);
  return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
}

// Warning: user exposed!
function TypedArrayMapPar(a, b) {
  // Arguments: [depth], func

  // Defer to the sequential variant for error cases or
  // when not working with typed objects.
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return callFunction(TypedArrayMap, this, a, b);
  var thisType = TypedObjectTypeDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return callFunction(TypedArrayMap, this, a, b);

  if (typeof a === "number" && IsCallable(b))
    return MapTypedParImpl(this, a, thisType, b);
  else if (IsCallable(a))
    return MapTypedParImpl(this, 1, thisType, a);
  return callFunction(TypedArrayMap, this, a, b);
}

// Warning: user exposed!
function TypedArrayReduce(a, b) {
  // Arguments: func, [initial]
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = TypedObjectTypeDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (a !== undefined && typeof a !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var outputType = thisType.elementType;
  return ReduceTypedSeqImpl(this, outputType, a, b);
}

// Warning: user exposed!
function TypedArrayScatter(a, b, c, d) {
  // Arguments: outputArrayType, indices, defaultValue, conflictFunction
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = TypedObjectTypeDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (!IsObject(a) || !ObjectIsTypeDescr(a) || !TypeDescrIsSizedArrayType(a))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (d !== undefined && typeof d !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  return ScatterTypedSeqImpl(this, a, b, c, d);
}

// Warning: user exposed!
function TypedArrayFilter(func) {
  // Arguments: predicate
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = TypedObjectTypeDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (typeof func !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  return FilterTypedSeqImpl(this, func);
}

// placeholders

// Warning: user exposed!
function TypedObjectArrayTypeBuildPar(a,b,c) {
  return callFunction(TypedObjectArrayTypeBuild, this, a, b, c);
}

// Warning: user exposed!
function TypedObjectArrayTypeFromPar(a,b,c) {
  // Arguments: arrayLike, [depth], func

  // Use the sequential version for error cases or when arrayLike is
  // not a typed object.
  if (!IsObject(this) || !ObjectIsTypeDescr(this) || !TypeDescrIsArrayType(this))
    return callFunction(TypedObjectArrayTypeFrom, this, a, b, c);
  if (!IsObject(a) || !ObjectIsTypedObject(a))
    return callFunction(TypedObjectArrayTypeFrom, this, a, b, c);

  // Detect whether an explicit depth is supplied.
  if (typeof b === "number" && IsCallable(c))
    return MapTypedParImpl(a, b, this, c);
  if (IsCallable(b))
    return MapTypedParImpl(a, 1, this, b);
  return callFunction(TypedObjectArrayTypeFrom, this, a, b, c);
}

// Warning: user exposed!
function TypedArrayReducePar(a, b) {
  return callFunction(TypedArrayReduce, this, a, b);
}

// Warning: user exposed!
function TypedArrayScatterPar(a, b, c, d) {
  return callFunction(TypedArrayScatter, this, a, b, c, d);
}

// Warning: user exposed!
function TypedArrayFilterPar(func) {
  return callFunction(TypedArrayFilter, this, func);
}

// should eventually become macros
function NUM_BYTES(bits) {
  return (bits + 7) >> 3;
}
function SET_BIT(data, index) {
  var word = index >> 3;
  var mask = 1 << (index & 0x7);
  data[word] |= mask;
}
function GET_BIT(data, index) {
  var word = index >> 3;
  var mask = 1 << (index & 0x7);
  return (data[word] & mask) != 0;
}

// Bug 956914: make performance-tuned variants tailored to 1, 2, and 3 dimensions.
function BuildTypedSeqImpl(arrayType, len, depth, func) {
  assert(IsObject(arrayType) && ObjectIsTypeDescr(arrayType), "Build called on non-type-object");

  if (depth <= 0 || TO_INT32(depth) !== depth)
    // RangeError("bad depth")
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // For example, if we have as input
  //    ArrayType(ArrayType(T, 4), 5)
  // and a depth of 2, we get
  //    grainType = T
  //    iterationSpace = [5, 4]
  var [iterationSpace, grainType, totalLength] =
    ComputeIterationSpace(arrayType, depth, len);

  // Create a zeroed instance with no data
  var result = arrayType.variable ? new arrayType(len) : new arrayType();

  var indices = NewDenseArray(depth);
  for (var i = 0; i < depth; i++) {
    indices[i] = 0;
  }

  var grainTypeIsComplex = !TypeDescrIsSimpleType(grainType);
  var size = DESCR_SIZE(grainType);
  var outOffset = 0;
  for (i = 0; i < totalLength; i++) {
    // Position out-pointer to point at &result[...indices], if appropriate.
    var userOutPointer = TypedObjectGetOpaqueIf(grainType, result, outOffset,
                                                grainTypeIsComplex);

    // Invoke func(...indices, userOutPointer) and store the result
    callFunction(std_Array_push, indices, userOutPointer);
    var r = callFunction(std_Function_apply, func, undefined, indices);
    callFunction(std_Array_pop, indices);
    if (r !== undefined)
      TypedObjectSet(grainType, result, outOffset, r); // result[...indices] = r;

    // Increment indices.
    IncrementIterationSpace(indices, iterationSpace);
    outOffset += size;
  }

  return result;
}

function ComputeIterationSpace(arrayType, depth, len) {
  assert(IsObject(arrayType) && ObjectIsTypeDescr(arrayType), "ComputeIterationSpace called on non-type-object");
  assert(TypeDescrIsArrayType(arrayType), "ComputeIterationSpace called on non-array-type");
  assert(depth > 0, "ComputeIterationSpace called on non-positive depth");
  var iterationSpace = NewDenseArray(depth);
  iterationSpace[0] = len;
  var totalLength = len;
  var grainType = arrayType.elementType;

  for (var i = 1; i < depth; i++) {
    if (TypeDescrIsArrayType(grainType)) {
      var grainLen = grainType.length;
      iterationSpace[i] = grainLen;
      totalLength *= grainLen;
      grainType = grainType.elementType;
    } else {
      // RangeError("Depth "+depth+" too high");
      ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
    }
  }
  return [iterationSpace, grainType, totalLength];
}

function IncrementIterationSpace(indices, iterationSpace) {
  // Increment something like
  //     [5, 5, 7, 8]
  // in an iteration space of
  //     [9, 9, 9, 9]
  // to
  //     [5, 5, 8, 0]

  assert(indices.length === iterationSpace.length,
         "indices dimension must equal iterationSpace dimension.");
  var n = indices.length - 1;
  while (true) {
    indices[n] += 1;
    if (indices[n] < iterationSpace[n])
      return;

    assert(indices[n] === iterationSpace[n],
         "Components of indices must match those of iterationSpace.");
    indices[n] = 0;
    if (n == 0)
      return;

    n -= 1;
  }
}

// Implements |from| method for untyped |inArray|.  (Depth is implicitly 1 for untyped input.)
function MapUntypedSeqImpl(inArray, outputType, maybeFunc) {
  assert(IsObject(outputType), "1. Map/From called on non-object outputType");
  assert(ObjectIsTypeDescr(outputType), "1. Map/From called on non-type-object outputType");
  inArray = ToObject(inArray);
  assert(TypeDescrIsArrayType(outputType), "Map/From called on non array-type outputType");

  if (!IsCallable(maybeFunc))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, maybeFunc));
  var func = maybeFunc;

  // Skip check for compatible iteration spaces; any normal JS array
  // is trivially compatible with any iteration space of depth 1.

  var outLength = outputType.variable ? inArray.length : outputType.length;
  var outGrainType = outputType.elementType;

  // Create a zeroed instance with no data
  var result = outputType.variable ? new outputType(inArray.length) : new outputType();

  var outUnitSize = DESCR_SIZE(outGrainType);
  var outGrainTypeIsComplex = !TypeDescrIsSimpleType(outGrainType);
  var outOffset = 0;

  // Core of map computation starts here (comparable to
  // DoMapTypedSeqDepth1 and DoMapTypedSeqDepthN below).

  for (var i = 0; i < outLength; i++) {
    // In this loop, since depth is 1, "indices" denotes singleton array [i].

    if (i in inArray) { // Check for holes (only needed for untyped case).
      // Extract element value.
      var element = inArray[i];

      // Create out pointer to point at &array[...indices] for result array.
      var out = TypedObjectGetOpaqueIf(outGrainType, result, outOffset,
                                       outGrainTypeIsComplex);

      // Invoke: var r = func(element, ...indices, collection, out);
      var r = func(element, i, inArray, out);

      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[i] = r
    }

    // Update offset and (implicitly) increment indices.
    outOffset += outUnitSize;
  }

  return result;
}

// Implements |map| and |from| methods for typed |inArray|.
function MapTypedSeqImpl(inArray, depth, outputType, func) {
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType), "2. Map/From called on non-type-object outputType");
  assert(IsObject(inArray) && ObjectIsTypedObject(inArray), "Map/From called on non-object or untyped input array.");
  assert(TypeDescrIsArrayType(outputType), "Map/From called on non array-type outputType");

  if (depth <= 0 || TO_INT32(depth) !== depth)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Compute iteration space for input and output and check for compatibility.
  var inputType = TypeOfTypedObject(inArray);
  var [inIterationSpace, inGrainType, _] =
    ComputeIterationSpace(inputType, depth, inArray.length);
  if (!IsObject(inGrainType) || !ObjectIsTypeDescr(inGrainType))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var [iterationSpace, outGrainType, totalLength] =
    ComputeIterationSpace(outputType, depth, outputType.variable ? inArray.length : outputType.length);
  for (var i = 0; i < depth; i++)
    if (inIterationSpace[i] !== iterationSpace[i])
      // TypeError("Incompatible iteration space in input and output type");
      ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);

  // Create a zeroed instance with no data
  var result = outputType.variable ? new outputType(inArray.length) : new outputType();

  var inGrainTypeIsComplex = !TypeDescrIsSimpleType(inGrainType);
  var outGrainTypeIsComplex = !TypeDescrIsSimpleType(outGrainType);

  var inOffset = 0;
  var outOffset = 0;

  var isDepth1Simple = depth == 1 && !(inGrainTypeIsComplex || outGrainTypeIsComplex);

  var inUnitSize = isDepth1Simple ? 0 : DESCR_SIZE(inGrainType);
  var outUnitSize = isDepth1Simple ? 0 : DESCR_SIZE(outGrainType);

  // Bug 956914: add additional variants for depth = 2, 3, etc.

  function DoMapTypedSeqDepth1() {
    for (var i = 0; i < totalLength; i++) {
      // In this loop, since depth is 1, "indices" denotes singleton array [i].

      // Prepare input element/handle and out pointer
      var element = TypedObjectGet(inGrainType, inArray, inOffset);
      var out = TypedObjectGetOpaqueIf(outGrainType, result, outOffset,
                                       outGrainTypeIsComplex);

      // Invoke: var r = func(element, ...indices, collection, out);
      var r = func(element, i, inArray, out);
      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[i] = r

      // Update offsets and (implicitly) increment indices.
      inOffset += inUnitSize;
      outOffset += outUnitSize;
    }

    return result;
  }

  function DoMapTypedSeqDepth1Simple(inArray, totalLength, func, result) {
    for (var i = 0; i < totalLength; i++) {
      var r = func(inArray[i], i, inArray, undefined);
      if (r !== undefined)
        result[i] = r;
    }

    return result;
  }

  function DoMapTypedSeqDepthN() {
    var indices = new Uint32Array(depth);

    for (var i = 0; i < totalLength; i++) {
      // Prepare input element and out pointer
      var element = TypedObjectGet(inGrainType, inArray, inOffset);
      var out = TypedObjectGetOpaqueIf(outGrainType, result, outOffset,
                                       outGrainTypeIsComplex);

      // Invoke: var r = func(element, ...indices, collection, out);
      var args = [element];
      callFunction(std_Function_apply, std_Array_push, args, indices);
      callFunction(std_Array_push, args, inArray, out);
      var r = callFunction(std_Function_apply, func, void 0, args);
      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[...indices] = r

      // Update offsets and explicitly increment indices.
      inOffset += inUnitSize;
      outOffset += outUnitSize;
      IncrementIterationSpace(indices, iterationSpace);
    }

    return result;
  }

  if (isDepth1Simple)
    return DoMapTypedSeqDepth1Simple(inArray, totalLength, func, result);

  if (depth == 1)
    return DoMapTypedSeqDepth1();

  return DoMapTypedSeqDepthN();
}

// Implements |map| and |from| methods for typed |inArray|.
function MapTypedParImpl(inArray, depth, outputType, func) {
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType),
         "Map/From called on non-type-object outputType");
  assert(IsObject(inArray) && ObjectIsTypedObject(inArray),
         "Map/From called on non-object or untyped input array.");
  assert(TypeDescrIsArrayType(outputType),
         "Map/From called on non array-type outputType");
  assert(typeof depth === "number",
         "Map/From called with non-numeric depth");
  assert(IsCallable(func),
         "Map/From called on something not callable");

  var inArrayType = TypeOfTypedObject(inArray);

  if (ShouldForceSequential() ||
      depth <= 0 ||
      TO_INT32(depth) !== depth ||
      !TypeDescrIsArrayType(inArrayType) ||
      !TypeDescrIsUnsizedArrayType(outputType))
  {
    // defer error cases to seq implementation:
    return MapTypedSeqImpl(inArray, depth, outputType, func);
  }

  switch (depth) {
  case 1:
    return MapTypedParImplDepth1(inArray, inArrayType, outputType, func);
  default:
    return MapTypedSeqImpl(inArray, depth, outputType, func);
  }
}

function RedirectPointer(typedObj, offset, outputIsScalar) {
  if (!outputIsScalar || !InParallelSection()) {
    // ^ Subtle note: always check InParallelSection() last, because
    // otherwise the other if conditions will not execute during
    // sequential mode and we will not gather enough type
    // information.

    // Here `typedObj` represents the input or output pointer we will
    // pass to the user function. Ideally, we will just update the
    // offset of `typedObj` in place so that it moves along the
    // input/output buffer without incurring any allocation costs. But
    // we can only do this if these changes are invisible to the user.
    //
    // Under normal uses, such changes *should* be invisible -- the
    // in/out pointers are only intended to be used during the
    // callback and then discarded, but of course in the general case
    // nothing prevents them from escaping.
    //
    // However, if we are in parallel mode, we know that the pointers
    // will not escape into global state. They could still escape by
    // being returned into the resulting array, but even that avenue
    // is impossible if the result array cannot contain objects.
    //
    // Therefore, we reuse a pointer if we are both in parallel mode
    // and we have a transparent output type.  It'd be nice to loosen
    // this condition later by using fancy ion optimizations that
    // assume the value won't escape and copy it if it does. But those
    // don't exist yet. Moreover, checking if the type is transparent
    // is an overapproximation: users can manually declare opaque
    // types that nonetheless only contain scalar data.

    typedObj = NewDerivedTypedObject(TypedObjectTypeDescr(typedObj),
                                     typedObj, 0);
  }

  SetTypedObjectOffset(typedObj, offset);
  return typedObj;
}
SetScriptHints(RedirectPointer,         { inline: true });

function MapTypedParImplDepth1(inArray, inArrayType, outArrayType, func) {
  assert(IsObject(inArrayType) && ObjectIsTypeDescr(inArrayType) &&
         TypeDescrIsArrayType(inArrayType),
         "DoMapTypedParDepth1: invalid inArrayType");
  assert(IsObject(outArrayType) && ObjectIsTypeDescr(outArrayType) &&
         TypeDescrIsArrayType(outArrayType),
         "DoMapTypedParDepth1: invalid outArrayType");
  assert(IsObject(inArray) && ObjectIsTypedObject(inArray),
         "DoMapTypedParDepth1: invalid inArray");

  // Determine the grain types of the input and output.
  const inGrainType = inArrayType.elementType;
  const outGrainType = outArrayType.elementType;
  const inGrainTypeSize = DESCR_SIZE(inGrainType);
  const outGrainTypeSize = DESCR_SIZE(outGrainType);
  const inGrainTypeIsComplex = !TypeDescrIsSimpleType(inGrainType);
  const outGrainTypeIsComplex = !TypeDescrIsSimpleType(outGrainType);

  const length = inArray.length;
  const mode = undefined;

  const outArray = new outArrayType(length);
  if (length === 0)
    return outArray;

  const outGrainTypeIsTransparent = ObjectIsTransparentTypedObject(outArray);

  // Construct the slices and initial pointers for each worker:
  const slicesInfo = ComputeSlicesInfo(length);
  const numWorkers = ForkJoinNumWorkers();
  assert(numWorkers > 0, "Should have at least the main thread");
  const pointers = [];
  for (var i = 0; i < numWorkers; i++) {
    const inTypedObject = TypedObjectGetDerivedIf(inGrainType, inArray, 0,
                                                  inGrainTypeIsComplex);
    const outTypedObject = TypedObjectGetOpaqueIf(outGrainType, outArray, 0,
                                                  outGrainTypeIsComplex);
    ARRAY_PUSH(pointers, ({ inTypedObject: inTypedObject,
                            outTypedObject: outTypedObject }));
  }

  // Below we will be adjusting offsets within the input to point at
  // successive entries; we'll need to know the offset of inArray
  // relative to its owner (which is often but not always 0).
  const inBaseOffset = TYPEDOBJ_BYTEOFFSET(inArray);

  ForkJoin(mapThread, 0, slicesInfo.count, ForkJoinMode(mode), outArray);
  return outArray;

  function mapThread(workerId, sliceStart, sliceEnd) {
    assert(TO_INT32(workerId) === workerId,
           "workerId not int: " + workerId);
    assert(workerId < pointers.length,
           "workerId too large: " + workerId + " >= " + pointers.length);

    var pointerIndex = InParallelSection() ? workerId : 0;
    assert(!!pointers[pointerIndex],
          "no pointer data for workerId: " + workerId);

    const { inTypedObject, outTypedObject } = pointers[pointerIndex];
    const sliceShift = slicesInfo.shift;
    var sliceId;

    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      const indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      const indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);

      var inOffset = inBaseOffset + std_Math_imul(inGrainTypeSize, indexStart);
      var outOffset = std_Math_imul(outGrainTypeSize, indexStart);

      // Set the target region so that user is only permitted to write
      // within the range set aside for this slice. This prevents user
      // from writing to typed objects that escaped from prior slices
      // during sequential iteration. Note that, for any particular
      // iteration of the loop below, it's only valid to write to the
      // memory range corresponding to the index `i` -- however, since
      // the different iterations cannot communicate typed object
      // pointers to one another during parallel exec, we need only
      // fear escaped typed objects from *other* slices, so we can
      // just set the target region once.
      const endOffset = std_Math_imul(outGrainTypeSize, indexEnd);
      SetForkJoinTargetRegion(outArray, outOffset, endOffset);

      for (var i = indexStart; i < indexEnd; i++) {
        var inVal = (inGrainTypeIsComplex
                     ? RedirectPointer(inTypedObject, inOffset,
                                       outGrainTypeIsTransparent)
                     : inArray[i]);
        var outVal = (outGrainTypeIsComplex
                      ? RedirectPointer(outTypedObject, outOffset,
                                        outGrainTypeIsTransparent)
                      : undefined);
        const r = func(inVal, i, inArray, outVal);
        if (r !== undefined) {
          if (outGrainTypeIsComplex)
            SetTypedObjectValue(outGrainType, outArray, outOffset, r);
          else
            UnsafePutElements(outArray, i, r);
        }
        inOffset += inGrainTypeSize;
        outOffset += outGrainTypeSize;

#ifndef JSGC_FJGENERATIONAL
        // A transparent result type cannot contain references, and
        // hence there is no way for a pointer to a thread-local object
        // to escape.
        //
        // This has been disabled for the PJS generational collector
        // as it probably has little effect in that setting and adds
        // per-iteration cost.
        if (outGrainTypeIsTransparent)
          ClearThreadLocalArenas();
#endif
      }
    }

    return sliceId;
  }

  return undefined;
}
SetScriptHints(MapTypedParImplDepth1,         { cloneAtCallsite: true });

function ReduceTypedSeqImpl(array, outputType, func, initial) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Reduce called on non-object or untyped input array.");
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType), "Reduce called on non-type-object outputType");

  var start, value;

  if (initial === undefined && array.length < 1)
    // RangeError("reduce requires array of length > 0")
    ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);

  // FIXME bug 950106 Should reduce method supply an outptr handle?
  // For now, reduce never supplies an outptr, regardless of outputType.

  if (TypeDescrIsSimpleType(outputType)) {
    if (initial === undefined) {
      start = 1;
      value = array[0];
    } else {
      start = 0;
      value = outputType(initial);
    }

    for (var i = start; i < array.length; i++)
      value = outputType(func(value, array[i]));

  } else {
    if (initial === undefined) {
      start = 1;
      value = new outputType(array[0]);
    } else {
      start = 0;
      value = initial;
    }

    for (var i = start; i < array.length; i++)
      value = func(value, array[i]);
  }

  return value;
}

function ScatterTypedSeqImpl(array, outputType, indices, defaultValue, conflictFunc) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Scatter called on non-object or untyped input array.");
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType), "Scatter called on non-type-object outputType");
  assert(TypeDescrIsSizedArrayType(outputType), "Scatter called on non-sized array type");
  assert(conflictFunc === undefined || typeof conflictFunc === "function", "Scatter called with invalid conflictFunc");

  var result = new outputType();
  var bitvec = new Uint8Array(result.length);
  var elemType = outputType.elementType;
  var i, j;
  if (defaultValue !== elemType(undefined)) {
    for (i = 0; i < result.length; i++) {
      result[i] = defaultValue;
    }
  }

  for (i = 0; i < indices.length; i++) {
    j = indices[i];
    if (!GET_BIT(bitvec, j)) {
      result[j] = array[i];
      SET_BIT(bitvec, j);
    } else if (conflictFunc === undefined) {
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_CONFLICT);
    } else {
      result[j] = conflictFunc(result[j], elemType(array[i]));
    }
  }
  return result;
}

function FilterTypedSeqImpl(array, func) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Filter called on non-object or untyped input array.");
  assert(typeof func === "function", "Filter called with non-function predicate");

  var arrayType = TypeOfTypedObject(array);
  if (!TypeDescrIsArrayType(arrayType))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var elementType = arrayType.elementType;
  var flags = new Uint8Array(NUM_BYTES(array.length));
  var count = 0;
  var size = DESCR_SIZE(elementType);
  var inOffset = 0;
  for (var i = 0; i < array.length; i++) {
    var v = TypedObjectGet(elementType, array, inOffset);
    if (func(v, i, array)) {
      SET_BIT(flags, i);
      count++;
    }
    inOffset += size;
  }

  var resultType = (arrayType.variable ? arrayType : arrayType.unsized);
  var result = new resultType(count);
  for (var i = 0, j = 0; i < array.length; i++) {
    if (GET_BIT(flags, i))
      result[j++] = array[i];
  }
  return result;
}
