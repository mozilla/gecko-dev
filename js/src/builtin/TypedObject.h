/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypedObject_h
#define builtin_TypedObject_h

#include "jsobj.h"

#include "builtin/TypedObjectConstants.h"
#include "builtin/TypeRepresentation.h"

/*
 * -------------
 * Typed Objects
 * -------------
 *
 * Typed objects are a special kind of JS object where the data is
 * given well-structured form. To use a typed object, users first
 * create *type objects* (no relation to the type objects used in TI)
 * that define the type layout. For example, a statement like:
 *
 *    var PointType = new StructType({x: uint8, y: uint8});
 *
 * would create a type object PointType that is a struct with
 * two fields, each of uint8 type.
 *
 * This comment typically assumes familiary with the API.  For more
 * info on the API itself, see the Harmony wiki page at
 * http://wiki.ecmascript.org/doku.php?id=harmony:typed_objects or the
 * ES6 spec (not finalized at the time of this writing).
 *
 * - Initialization:
 *
 * Currently, all "globals" related to typed objects are packaged
 * within a single "module" object `TypedObject`. This module has its
 * own js::Class and when that class is initialized, we also create
 * and define all other values (in `js_InitTypedObjectModuleClass()`).
 *
 * - Type objects, meta type objects, and type representations:
 *
 * There are a number of pre-defined type objects, one for each
 * scalar type (`uint8` etc). Each of these has its own class_,
 * defined in `DefineNumericClass()`.
 *
 * There are also meta type objects (`ArrayType`, `StructType`).
 * These constructors are not themselves type objects but rather the
 * means for the *user* to construct new typed objects.
 *
 * Each type object is associated with a *type representation* (see
 * TypeRepresentation.h). Type representations are canonical versions
 * of type objects. We attach them to TI type objects and (eventually)
 * use them for shape guards etc. They are purely internal to the
 * engine and are not exposed to end users (though self-hosted code
 * sometimes accesses them).
 *
 * - Typed datums, objects, and handles:
 *
 * A typed object is an instance of a type object. A handle is a
 * relocatable pointer that points into other typed objects. Both of them
 * are basically represented the same way, though they have distinct
 * js::Class entries. They are both subtypes of `TypedDatum`.
 *
 * Both typed objects and handles are non-native objects that fully
 * override the property accessors etc. The overridden accessor
 * methods are the same in each and are defined in methods of
 * TypedDatum.
 *
 * Typed datums may be attached or unattached. An unattached typed
 * datum has no memory associated with it; it is basically a null
 * pointer.  This can only happen when a new handle is created, since
 * typed object instances are always associated with memory at the
 * point of creation.
 *
 * When a new typed object instance is created, fresh memory is
 * allocated and set as that typed object's private field. The object
 * is then considered the *owner* of that memory: when the object is
 * collected, its finalizer will free the memory. The fact that an
 * object `o` owns its memory is indicated by setting its reserved
 * slot JS_TYPEDOBJ_SLOT_OWNER to `o` (a trivial cycle, in other
 * words).
 *
 * Later, *derived* typed objects can be created, typically via an
 * access like `o.f` where `f` is some complex (non-scalar) type, but
 * also explicitly via Handle objects. In those cases, the memory
 * pointer of the derived object is set to alias the owner's memory
 * pointer, and the owner slot for the derived object is set to the
 * owner object, thus ensuring that the owner is not collected while
 * the derived object is alive. We always maintain the invariant that
 * JS_TYPEDOBJ_SLOT_OWNER is the true owner of the memory, meaning
 * that there is a shallow tree. This prevents an access pattern like
 * `a.b.c.d` from keeping all the intermediate objects alive.
 */

namespace js {

class StructTypeDescr;

/*
 * This object exists in order to encapsulate the typed object types
 * somewhat, rather than sticking them all into the global object.
 * Eventually it will go away and become a module.
 */
class TypedObjectModuleObject : public JSObject {
  public:
    enum Slot {
        ArrayTypePrototype,
        StructTypePrototype,
        SlotCount
    };

    static const Class class_;

    static bool getSuitableClaspAndProto(JSContext *cx,
                                         TypeRepresentation::Kind kind,
                                         const Class **clasp,
                                         MutableHandleObject proto);
};

/*
 * Helper method for converting a double into other scalar
 * types in the same way that JavaScript would. In particular,
 * simple C casting from double to int32_t gets things wrong
 * for values like 0xF0000000.
 */
template <typename T>
static T ConvertScalar(double d)
{
    if (TypeIsFloatingPoint<T>()) {
        return T(d);
    } else if (TypeIsUnsigned<T>()) {
        uint32_t n = ToUint32(d);
        return T(n);
    } else {
        int32_t n = ToInt32(d);
        return T(n);
    }
}

bool TypeDescrToSource(JSContext *cx, unsigned int argc, Value *vp);

class TypeDescr : public JSObject
{
  public:
    JSObject &typeRepresentationOwnerObj() const {
        return getReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR).toObject();
    }

    TypeRepresentation *typeRepresentation() const {
        return TypeRepresentation::fromOwnerObject(typeRepresentationOwnerObj());
    }

    TypeRepresentation::Kind kind() const {
        return typeRepresentation()->kind();
    }
};

typedef Handle<TypeDescr*> HandleTypeDescr;

bool InitializeCommonTypeDescriptorProperties(JSContext *cx,
                                              HandleTypeDescr obj,
                                              HandleObject typeReprOwnerObj);

class SizedTypeDescr : public TypeDescr
{
  public:
    SizedTypeRepresentation *typeRepresentation() const {
        return ((TypeDescr*)this)->typeRepresentation()->asSized();
    }

    size_t size() {
        return typeRepresentation()->size();
    }
};

typedef Handle<SizedTypeDescr*> HandleSizedTypeDescr;

class SimpleTypeDescr : public SizedTypeDescr
{
};

// Type for scalar type constructors like `uint8`. All such type
// constructors share a common js::Class and JSFunctionSpec. Scalar
// types are non-opaque (their storage is visible unless combined with
// an opaque reference type.)
class ScalarTypeDescr : public SimpleTypeDescr
{
  public:
    static const Class class_;
    static const JSFunctionSpec typeObjectMethods[];
    typedef ScalarTypeRepresentation TypeRepr;

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

// Type for reference type constructors like `Any`, `String`, and
// `Object`. All such type constructors share a common js::Class and
// JSFunctionSpec. All these types are opaque.
class ReferenceTypeDescr : public SimpleTypeDescr
{
  public:
    static const Class class_;
    static const JSFunctionSpec typeObjectMethods[];
    typedef ReferenceTypeRepresentation TypeRepr;

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

/*
 * Type descriptors `float32x4` and `int32x4`
 */
class X4TypeDescr : public SizedTypeDescr
{
  private:
  public:
    static const Class class_;

    static bool call(JSContext *cx, unsigned argc, Value *vp);
    static bool is(const Value &v);
};

/*
 * Properties and methods of the `ArrayType` meta type object. There
 * is no `class_` field because `ArrayType` is just a native
 * constructor function.
 */
class ArrayMetaTypeDescr : public JSObject
{
  private:
    friend class UnsizedArrayTypeDescr;

    // Helper for creating a new ArrayType object, either sized or unsized.
    // The template parameter `T` should be either `UnsizedArrayTypeDescr`
    // or `SizedArrayTypeDescr`.
    //
    // - `arrayTypePrototype` - prototype for the new object to be created,
    //                          either ArrayType.prototype or
    //                          unsizedArrayType.__proto__ depending on
    //                          whether this is a sized or unsized array
    // - `arrayTypeReprObj` - a type representation object for the array
    // - `elementType` - type object for the elements in the array
    template<class T>
    static T *create(JSContext *cx,
                     HandleObject arrayTypePrototype,
                     HandleObject arrayTypeReprObj,
                     HandleSizedTypeDescr elementType);

  public:
    // Properties and methods to be installed on ArrayType.prototype,
    // and hence inherited by all array type objects:
    static const JSPropertySpec typeObjectProperties[];
    static const JSFunctionSpec typeObjectMethods[];

    // Properties and methods to be installed on ArrayType.prototype.prototype,
    // and hence inherited by all array *typed* objects:
    static const JSPropertySpec typedObjectProperties[];
    static const JSFunctionSpec typedObjectMethods[];

    // This is the function that gets called when the user
    // does `new ArrayType(elem)`. It produces an array type object.
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};

/*
 * Type descriptor created by `new ArrayType(typeObj)`
 */
class UnsizedArrayTypeDescr : public TypeDescr
{
  public:
    static const Class class_;

    // This is the sized method on unsized array type objects.  It
    // produces a sized variant.
    static bool dimension(JSContext *cx, unsigned int argc, jsval *vp);

    SizedTypeDescr &elementType() {
        return getReservedSlot(JS_TYPEOBJ_SLOT_ARRAY_ELEM_TYPE).toObject().as<SizedTypeDescr>();
    }
};

/*
 * Type descriptor created by `unsizedArrayTypeObj.dimension()`
 */
class SizedArrayTypeDescr : public SizedTypeDescr
{
  public:
    static const Class class_;

    SizedTypeDescr &elementType() {
        return getReservedSlot(JS_TYPEOBJ_SLOT_ARRAY_ELEM_TYPE).toObject().as<SizedTypeDescr>();
    }
};

/*
 * Properties and methods of the `StructType` meta type object. There
 * is no `class_` field because `StructType` is just a native
 * constructor function.
 */
class StructMetaTypeDescr : public JSObject
{
  private:
    static JSObject *create(JSContext *cx, HandleObject structTypeGlobal,
                            HandleObject fields);

    /*
     * Sets up structType slots based on calculated memory size
     * and alignment and stores fieldmap as well.
     */
    static bool layout(JSContext *cx,
                       Handle<StructTypeDescr*> structType,
                       HandleObject fields);

  public:
    // Properties and methods to be installed on StructType.prototype,
    // and hence inherited by all struct type objects:
    static const JSPropertySpec typeObjectProperties[];
    static const JSFunctionSpec typeObjectMethods[];

    // Properties and methods to be installed on StructType.prototype.prototype,
    // and hence inherited by all struct *typed* objects:
    static const JSPropertySpec typedObjectProperties[];
    static const JSFunctionSpec typedObjectMethods[];

    // This is the function that gets called when the user
    // does `new StructType(...)`. It produces a struct type object.
    static bool construct(JSContext *cx, unsigned argc, Value *vp);

    static bool convertAndCopyTo(JSContext *cx,
                                 StructTypeRepresentation *typeRepr,
                                 HandleValue from, uint8_t *mem);
};

class StructTypeDescr : public SizedTypeDescr {
  public:
    static const Class class_;
};

typedef Handle<StructTypeDescr*> HandleStructTypeDescr;

/*
 * Base type for typed objects and handles. Basically any type whose
 * contents consist of typed memory.
 */
class TypedDatum : public JSObject
{
  private:
    static const bool IsTypedDatumClass = true;

    template<class T>
    static bool obj_getArrayElement(JSContext *cx,
                                    Handle<TypedDatum*> datum,
                                    Handle<TypeDescr*> typeDescr,
                                    uint32_t index,
                                    MutableHandleValue vp);

    template<class T>
    static bool obj_setArrayElement(JSContext *cx,
                                    Handle<TypedDatum*> datum,
                                    Handle<TypeDescr*> typeDescr,
                                    uint32_t index,
                                    MutableHandleValue vp);

  protected:
    static void obj_finalize(js::FreeOp *op, JSObject *obj);

    static void obj_trace(JSTracer *trace, JSObject *object);

    static bool obj_lookupGeneric(JSContext *cx, HandleObject obj,
                                  HandleId id, MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_lookupProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name,
                                   MutableHandleObject objp,
                                   MutableHandleShape propp);

    static bool obj_lookupElement(JSContext *cx, HandleObject obj,
                                  uint32_t index, MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_lookupSpecial(JSContext *cx, HandleObject obj,
                                  HandleSpecialId sid,
                                  MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name, HandleValue v,
                                   PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                               HandleId id, MutableHandleValue vp);

    static bool obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                HandlePropertyName name, MutableHandleValue vp);

    static bool obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                               uint32_t index, MutableHandleValue vp);

    static bool obj_getUnsizedArrayElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                                         uint32_t index, MutableHandleValue vp);

    static bool obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver,
                               HandleSpecialId sid, MutableHandleValue vp);

    static bool obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                               MutableHandleValue vp, bool strict);
    static bool obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                MutableHandleValue vp, bool strict);
    static bool obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                               MutableHandleValue vp, bool strict);
    static bool obj_setSpecial(JSContext *cx, HandleObject obj,
                               HandleSpecialId sid, MutableHandleValue vp, bool strict);

    static bool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);
    static bool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);

    static bool obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                   bool *succeeded);
    static bool obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                  bool *succeeded);
    static bool obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                  bool *succeeded);

    static bool obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                              MutableHandleValue statep, MutableHandleId idp);

  public:
    // Each typed object contains a void* pointer pointing at the
    // binary data that it represents. (That data may be owned by this
    // object or this object may alias data owned by someone else.)
    // This function returns the offset in bytes within the object
    // where the `void*` pointer can be found. It is intended for use
    // by the JIT.
    static size_t dataOffset();

    static TypedDatum *createUnattachedWithClass(JSContext *cx,
                                                 const Class *clasp,
                                                 HandleTypeDescr type,
                                                 int32_t length);

    // Creates an unattached typed object or handle (depending on the
    // type parameter T). Note that it is only legal for unattached
    // handles to escape to the end user; for non-handles, the caller
    // should always invoke one of the `attach()` methods below.
    //
    // Arguments:
    // - type: type object for resulting object
    // - length: 0 unless this is an array, otherwise the length
    template<class T>
    static T *createUnattached(JSContext *cx, HandleTypeDescr type,
                               int32_t length);

    // Creates a datum that aliases the memory pointed at by `owner`
    // at the given offset. The datum will be a handle iff type is a
    // handle and a typed object otherwise.
    static TypedDatum *createDerived(JSContext *cx,
                                     HandleSizedTypeDescr type,
                                     Handle<TypedDatum*> typedContents,
                                     size_t offset);


    // If `this` is the owner of the memory, use this.
    void attach(uint8_t *mem);

    // Otherwise, use this to attach to memory referenced by another datum.
    void attach(TypedDatum &datum, uint32_t offset);

    TypedDatum &owner() const {
        return getReservedSlot(JS_DATUM_SLOT_OWNER).toObject().as<TypedDatum>();
    }

    TypeDescr &typeDescr() const {
        return getReservedSlot(JS_DATUM_SLOT_TYPE_DESCR).toObject().as<TypeDescr>();
    }

    TypeRepresentation *typeRepresentation() const {
        return typeDescr().typeRepresentation();
    }

    uint8_t *typedMem() const {
        return (uint8_t*) getPrivate();
    }

    size_t length() const {
        JS_ASSERT(typeRepresentation()->isAnyArray());
        return getReservedSlot(JS_DATUM_SLOT_LENGTH).toInt32();
    }

    size_t size() const {
        TypeRepresentation *typeRepr = typeRepresentation();
        switch (typeRepr->kind()) {
          case TypeRepresentation::Scalar:
          case TypeRepresentation::X4:
          case TypeRepresentation::Reference:
          case TypeRepresentation::Struct:
          case TypeRepresentation::SizedArray:
            return typeRepr->asSized()->size();

          case TypeRepresentation::UnsizedArray:
            return typeRepr->asUnsizedArray()->element()->size() * length();
        }
        MOZ_ASSUME_UNREACHABLE("unhandled typerepresentation kind");
    }

    uint8_t *typedMem(size_t offset) const {
        JS_ASSERT(offset < size());
        return typedMem() + offset;
    }
};

typedef Handle<TypedDatum*> HandleTypedDatum;

class TypedObject : public TypedDatum
{
  public:
    static const Class class_;

    // Creates a new typed object whose memory is freshly allocated
    // and initialized with zeroes (or, in the case of references, an
    // appropriate default value).
    static TypedObject *createZeroed(JSContext *cx,
                                     HandleTypeDescr typeObj,
                                     int32_t length);

    // user-accessible constructor (`new TypeDescriptor(...)`)
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};

typedef Handle<TypedObject*> HandleTypedObject;

class TypedHandle : public TypedDatum
{
  public:
    static const Class class_;
    static const JSFunctionSpec handleStaticMethods[];
};

/*
 * Usage: NewTypedHandle(typeObj)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewTypedHandle(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: NewTypedHandle(typeObj)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewTypedHandle(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: NewDerivedTypedDatum(typeObj, owner, offset)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewDerivedTypedDatum(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: AttachHandle(handle, newOwner, newOffset)
 *
 * Moves `handle` to point at the memory owned by `newOwner` with
 * the offset `newOffset`.
 */
bool AttachHandle(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo AttachHandleJitInfo;

/*
 * Usage: ObjectIsTypeDescr(obj)
 *
 * True if `obj` is a type object.
 */
bool ObjectIsTypeDescr(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypeDescrJitInfo;

/*
 * Usage: ObjectIsTypeRepresentation(obj)
 *
 * True if `obj` is a type representation object.
 */
bool ObjectIsTypeRepresentation(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypeRepresentationJitInfo;

/*
 * Usage: ObjectIsTypedHandle(obj)
 *
 * True if `obj` is a handle.
 */
bool ObjectIsTypedHandle(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypedHandleJitInfo;

/*
 * Usage: ObjectIsTypedObject(obj)
 *
 * True if `obj` is a typed object.
 */
bool ObjectIsTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypedObjectJitInfo;

/*
 * Usage: IsAttached(obj)
 *
 * Given a TypedDatum `obj`, returns true if `obj` is
 * "attached" (i.e., its data pointer is nullptr).
 */
bool IsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo IsAttachedJitInfo;

/*
 * Usage: ClampToUint8(v)
 *
 * Same as the C function ClampDoubleToUint8. `v` must be a number.
 */
bool ClampToUint8(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ClampToUint8JitInfo;

/*
 * Usage: Memcpy(targetDatum, targetOffset,
 *               sourceDatum, sourceOffset,
 *               size)
 *
 * Intrinsic function. Copies size bytes from the data for
 * `sourceDatum` at `sourceOffset` into the data for
 * `targetDatum` at `targetOffset`.
 *
 * Both `sourceDatum` and `targetDatum` must be attached.
 */
bool Memcpy(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo MemcpyJitInfo;

/*
 * Usage: GetTypedObjectModule()
 *
 * Returns the global "typed object" module, which provides access
 * to the various builtin type descriptors. These are currently
 * exported as immutable properties so it is safe for self-hosted code
 * to access them; eventually this should be linked into the module
 * system.
 */
bool GetTypedObjectModule(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: GetFloat32x4TypeDescr()
 *
 * Returns the float32x4 type object. SIMD pseudo-module must have 
 * been initialized for this to be safe.
 */
bool GetFloat32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: GetInt32x4TypeDescr()
 *
 * Returns the int32x4 type object. SIMD pseudo-module must have 
 * been initialized for this to be safe.
 */
bool GetInt32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: Store_int8(targetDatum, targetOffset, value)
 *        ...
 *        Store_uint8(targetDatum, targetOffset, value)
 *        ...
 *        Store_float32(targetDatum, targetOffset, value)
 *        Store_float64(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores `value` into the memory referenced by
 * `targetDatum` at the offset `targetOffset`.
 *
 * Assumes (and asserts) that:
 * - `targetDatum` is attached
 * - `targetOffset` is a valid offset within the bounds of `targetDatum`
 * - `value` is a number
 */
#define JS_STORE_SCALAR_CLASS_DEFN(_constant, T, _name)                       \
class StoreScalar##T {                                                        \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: Store_Any(targetDatum, targetOffset, value)
 *        Store_Object(targetDatum, targetOffset, value)
 *        Store_string(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores `value` into the memory referenced by
 * `targetDatum` at the offset `targetOffset`.
 *
 * Assumes (and asserts) that:
 * - `targetDatum` is attached
 * - `targetOffset` is a valid offset within the bounds of `targetDatum`
 * - `value` is an object (`Store_Object`) or string (`Store_string`).
 */
#define JS_STORE_REFERENCE_CLASS_DEFN(_constant, T, _name)                    \
class StoreReference##T {                                                     \
  private:                                                                    \
    static void store(T* heap, const Value &v);                               \
                                                                              \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: LoadScalar(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Loads value (which must be an int32 or uint32)
 * by `scalarTypeRepr` (which must be a type repr obj) and loads the
 * value at the memory for `targetDatum` at offset `targetOffset`.
 * `targetDatum` must be attached.
 */
#define JS_LOAD_SCALAR_CLASS_DEFN(_constant, T, _name)                        \
class LoadScalar##T {                                                         \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: LoadReference(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores value (which must be an int32 or uint32)
 * by `scalarTypeRepr` (which must be a type repr obj) and stores the
 * value at the memory for `targetDatum` at offset `targetOffset`.
 * `targetDatum` must be attached.
 */
#define JS_LOAD_REFERENCE_CLASS_DEFN(_constant, T, _name)                     \
class LoadReference##T {                                                      \
  private:                                                                    \
    static void load(T* heap, MutableHandleValue v);                          \
                                                                              \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

// I was using templates for this stuff instead of macros, but ran
// into problems with the Unagi compiler.
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_STORE_SCALAR_CLASS_DEFN)
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_LOAD_SCALAR_CLASS_DEFN)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_STORE_REFERENCE_CLASS_DEFN)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_LOAD_REFERENCE_CLASS_DEFN)

} // namespace js

JSObject *
js_InitTypedObjectModuleObject(JSContext *cx, JS::HandleObject obj);

template <>
inline bool
JSObject::is<js::SimpleTypeDescr>() const
{
    return is<js::ScalarTypeDescr>() ||
           is<js::ReferenceTypeDescr>();
}

template <>
inline bool
JSObject::is<js::SizedTypeDescr>() const
{
    return is<js::SimpleTypeDescr>() ||
           is<js::StructTypeDescr>() ||
           is<js::SizedArrayTypeDescr>() ||
           is<js::X4TypeDescr>();
}

template <>
inline bool
JSObject::is<js::TypeDescr>() const
{
    return is<js::SizedTypeDescr>() ||
           is<js::UnsizedArrayTypeDescr>();
}

template <>
inline bool
JSObject::is<js::TypedDatum>() const
{
    return is<js::TypedObject>() || is<js::TypedHandle>();
}

#endif /* builtin_TypedObject_h */

