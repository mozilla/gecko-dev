/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS::Value implementation. */

#ifndef js_Value_h
#define js_Value_h

#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/Compiler.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"

#include <limits> /* for std::numeric_limits */

#include "js-config.h"
#include "jstypes.h"

#include "js/GCAPI.h"
#include "js/RootingAPI.h"
#include "js/Utility.h"

namespace JS {
union Value;
}

/* JS::Value can store a full int32_t. */
#define JSVAL_INT_BITS 32
#define JSVAL_INT_MIN ((int32_t)0x80000000)
#define JSVAL_INT_MAX ((int32_t)0x7fffffff)

#if defined(JS_PUNBOX64)
#define JSVAL_TAG_SHIFT 47
#endif

// Use enums so that printing a JS::Value in the debugger shows nice
// symbolic type tags.

// Work around a GCC bug. See comment above #undef JS_ENUM_HEADER.
#if MOZ_IS_GCC
#define JS_ENUM_HEADER(id, type) enum id
#define JS_ENUM_FOOTER(id) __attribute__((packed))
#else
#define JS_ENUM_HEADER(id, type) enum id : type
#define JS_ENUM_FOOTER(id)
#endif

enum JSValueType : uint8_t {
  JSVAL_TYPE_DOUBLE = 0x00,
  JSVAL_TYPE_INT32 = 0x01,
  JSVAL_TYPE_BOOLEAN = 0x02,
  JSVAL_TYPE_UNDEFINED = 0x03,
  JSVAL_TYPE_NULL = 0x04,
  JSVAL_TYPE_MAGIC = 0x05,
  JSVAL_TYPE_STRING = 0x06,
  JSVAL_TYPE_SYMBOL = 0x07,
  JSVAL_TYPE_PRIVATE_GCTHING = 0x08,
#ifdef ENABLE_BIGINT
  JSVAL_TYPE_BIGINT = 0x09,
#endif
  JSVAL_TYPE_OBJECT = 0x0c,

  // These never appear in a jsval; they are only provided as an out-of-band
  // value.
  JSVAL_TYPE_UNKNOWN = 0x20,
  JSVAL_TYPE_MISSING = 0x21
};

static_assert(sizeof(JSValueType) == 1,
              "compiler typed enum support is apparently buggy");

#if defined(JS_NUNBOX32)

JS_ENUM_HEADER(JSValueTag, uint32_t){
    JSVAL_TAG_CLEAR = 0xFFFFFF80,
    JSVAL_TAG_INT32 = JSVAL_TAG_CLEAR | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED = JSVAL_TAG_CLEAR | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_NULL = JSVAL_TAG_CLEAR | JSVAL_TYPE_NULL,
    JSVAL_TAG_BOOLEAN = JSVAL_TAG_CLEAR | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC = JSVAL_TAG_CLEAR | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_STRING = JSVAL_TAG_CLEAR | JSVAL_TYPE_STRING,
    JSVAL_TAG_SYMBOL = JSVAL_TAG_CLEAR | JSVAL_TYPE_SYMBOL,
    JSVAL_TAG_PRIVATE_GCTHING = JSVAL_TAG_CLEAR | JSVAL_TYPE_PRIVATE_GCTHING,
#ifdef ENABLE_BIGINT
    JSVAL_TAG_BIGINT = JSVAL_TAG_CLEAR | JSVAL_TYPE_BIGINT,
#endif
    JSVAL_TAG_OBJECT = JSVAL_TAG_CLEAR |
                       JSVAL_TYPE_OBJECT} JS_ENUM_FOOTER(JSValueTag);

static_assert(sizeof(JSValueTag) == sizeof(uint32_t),
              "compiler typed enum support is apparently buggy");

#elif defined(JS_PUNBOX64)

JS_ENUM_HEADER(JSValueTag, uint32_t){
    JSVAL_TAG_MAX_DOUBLE = 0x1FFF0,
    JSVAL_TAG_INT32 = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_INT32,
    JSVAL_TAG_UNDEFINED = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_UNDEFINED,
    JSVAL_TAG_NULL = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_NULL,
    JSVAL_TAG_BOOLEAN = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BOOLEAN,
    JSVAL_TAG_MAGIC = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_MAGIC,
    JSVAL_TAG_STRING = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_STRING,
    JSVAL_TAG_SYMBOL = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_SYMBOL,
    JSVAL_TAG_PRIVATE_GCTHING = JSVAL_TAG_MAX_DOUBLE |
                                JSVAL_TYPE_PRIVATE_GCTHING,
#ifdef ENABLE_BIGINT
    JSVAL_TAG_BIGINT = JSVAL_TAG_MAX_DOUBLE | JSVAL_TYPE_BIGINT,
#endif
    JSVAL_TAG_OBJECT = JSVAL_TAG_MAX_DOUBLE |
                       JSVAL_TYPE_OBJECT} JS_ENUM_FOOTER(JSValueTag);

static_assert(sizeof(JSValueTag) == sizeof(uint32_t),
              "compiler typed enum support is apparently buggy");

enum JSValueShiftedTag : uint64_t {
  JSVAL_SHIFTED_TAG_MAX_DOUBLE =
      ((((uint64_t)JSVAL_TAG_MAX_DOUBLE) << JSVAL_TAG_SHIFT) | 0xFFFFFFFF),
  JSVAL_SHIFTED_TAG_INT32 = (((uint64_t)JSVAL_TAG_INT32) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_UNDEFINED =
      (((uint64_t)JSVAL_TAG_UNDEFINED) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_NULL = (((uint64_t)JSVAL_TAG_NULL) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_BOOLEAN =
      (((uint64_t)JSVAL_TAG_BOOLEAN) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_MAGIC = (((uint64_t)JSVAL_TAG_MAGIC) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_STRING = (((uint64_t)JSVAL_TAG_STRING) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_SYMBOL = (((uint64_t)JSVAL_TAG_SYMBOL) << JSVAL_TAG_SHIFT),
  JSVAL_SHIFTED_TAG_PRIVATE_GCTHING =
      (((uint64_t)JSVAL_TAG_PRIVATE_GCTHING) << JSVAL_TAG_SHIFT),
#ifdef ENABLE_BIGINT
  JSVAL_SHIFTED_TAG_BIGINT = (((uint64_t)JSVAL_TAG_BIGINT) << JSVAL_TAG_SHIFT),
#endif
  JSVAL_SHIFTED_TAG_OBJECT = (((uint64_t)JSVAL_TAG_OBJECT) << JSVAL_TAG_SHIFT)
};

static_assert(sizeof(JSValueShiftedTag) == sizeof(uint64_t),
              "compiler typed enum support is apparently buggy");

#endif

/*
 * All our supported compilers implement C++11 |enum Foo : T| syntax, so don't
 * expose these macros. (This macro exists *only* because gcc bug 51242
 * <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51242> makes bit-fields of
 * typed enums trigger a warning that can't be turned off. Don't expose it
 * beyond this file!)
 */
#undef JS_ENUM_HEADER
#undef JS_ENUM_FOOTER

#if defined(JS_NUNBOX32)

#define JSVAL_TYPE_TO_TAG(type) ((JSValueTag)(JSVAL_TAG_CLEAR | (type)))

#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET JSVAL_TAG_STRING

#elif defined(JS_PUNBOX64)

// This should only be used in toGCThing, see the 'Spectre mitigations' comment.
#define JSVAL_PAYLOAD_MASK_GCTHING 0x00007FFFFFFFFFFFLL

#define JSVAL_TAG_MASK 0xFFFF800000000000LL
#define JSVAL_TYPE_TO_TAG(type) ((JSValueTag)(JSVAL_TAG_MAX_DOUBLE | (type)))
#define JSVAL_TYPE_TO_SHIFTED_TAG(type) \
  (((uint64_t)JSVAL_TYPE_TO_TAG(type)) << JSVAL_TAG_SHIFT)

#define JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET JSVAL_TAG_OBJECT
#define JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET JSVAL_TAG_INT32
#define JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET JSVAL_TAG_STRING

#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET JSVAL_SHIFTED_TAG_OBJECT
#define JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET JSVAL_SHIFTED_TAG_BOOLEAN
#define JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET JSVAL_SHIFTED_TAG_STRING

// JSVAL_TYPE_OBJECT and JSVAL_TYPE_NULL differ by one bit. We can use this to
// implement toObjectOrNull more efficiently.
#define JSVAL_OBJECT_OR_NULL_BIT (uint64_t(0x8) << JSVAL_TAG_SHIFT)
static_assert(
    (JSVAL_SHIFTED_TAG_NULL ^ JSVAL_SHIFTED_TAG_OBJECT) ==
        JSVAL_OBJECT_OR_NULL_BIT,
    "JSVAL_OBJECT_OR_NULL_BIT must be consistent with object and null tags");

#endif /* JS_PUNBOX64 */

enum JSWhyMagic {
  /** a hole in a native object's elements */
  JS_ELEMENTS_HOLE,

  /** there is not a pending iterator value */
  JS_NO_ITER_VALUE,

  /** exception value thrown when closing a generator */
  JS_GENERATOR_CLOSING,

  /** used in debug builds to catch tracing errors */
  JS_ARG_POISON,

  /** an empty subnode in the AST serializer */
  JS_SERIALIZE_NO_NODE,

  /** optimized-away 'arguments' value */
  JS_OPTIMIZED_ARGUMENTS,

  /** magic value passed to natives to indicate construction */
  JS_IS_CONSTRUCTING,

  /** see class js::HashableValue */
  JS_HASH_KEY_EMPTY,

  /** error while running Ion code */
  JS_ION_ERROR,

  /** missing recover instruction result */
  JS_ION_BAILOUT,

  /** optimized out slot */
  JS_OPTIMIZED_OUT,

  /** uninitialized lexical bindings that produce ReferenceError on touch. */
  JS_UNINITIALIZED_LEXICAL,

  /** standard constructors are not created for off-thread parsing. */
  JS_OFF_THREAD_CONSTRUCTOR,

  /** used in jit::TrySkipAwait */
  JS_CANNOT_SKIP_AWAIT,

  /** for local use */
  JS_GENERIC_MAGIC,

  JS_WHY_MAGIC_COUNT
};

namespace js {
static inline JS::Value PoisonedObjectValue(uintptr_t poison);
}  // namespace js

namespace JS {

namespace detail {

constexpr int CanonicalizedNaNSignBit = 0;
constexpr uint64_t CanonicalizedNaNSignificand = 0x8000000000000ULL;

constexpr uint64_t CanonicalizedNaNBits =
    mozilla::SpecificNaNBits<double, detail::CanonicalizedNaNSignBit,
                             detail::CanonicalizedNaNSignificand>::value;

}  // namespace detail

/**
 * Returns a generic quiet NaN value, with all payload bits set to zero.
 *
 * Among other properties, this NaN's bit pattern conforms to JS::Value's
 * bit pattern restrictions.
 */
static MOZ_ALWAYS_INLINE double GenericNaN() {
  return mozilla::SpecificNaN<double>(detail::CanonicalizedNaNSignBit,
                                      detail::CanonicalizedNaNSignificand);
}

static inline double CanonicalizeNaN(double d) {
  if (MOZ_UNLIKELY(mozilla::IsNaN(d))) {
    return GenericNaN();
  }
  return d;
}

/**
 * [SMDOC] JS::Value type
 *
 * JS::Value is the interface for a single JavaScript Engine value.  A few
 * general notes on JS::Value:
 *
 * - JS::Value has setX() and isX() members for X in
 *
 *     { Int32, Double, String, Symbol, BigInt, Boolean, Undefined, Null,
 *       Object, Magic }
 *
 *   JS::Value also contains toX() for each of the non-singleton types.
 *
 * - Magic is a singleton type whose payload contains either a JSWhyMagic
 *   "reason" for the magic value or a uint32_t value. By providing JSWhyMagic
 *   values when creating and checking for magic values, it is possible to
 *   assert, at runtime, that only magic values with the expected reason flow
 *   through a particular value. For example, if cx->exception has a magic
 *   value, the reason must be JS_GENERATOR_CLOSING.
 *
 * - The JS::Value operations are preferred.  The JSVAL_* operations remain for
 *   compatibility; they may be removed at some point.  These operations mostly
 *   provide similar functionality.  But there are a few key differences.  One
 *   is that JS::Value gives null a separate type.
 *   Also, to help prevent mistakenly boxing a nullable JSObject* as an object,
 *   Value::setObject takes a JSObject&. (Conversely, Value::toObject returns a
 *   JSObject&.)  A convenience member Value::setObjectOrNull is provided.
 *
 * - Note that JS::Value is 8 bytes on 32 and 64-bit architectures. Thus, on
 *   32-bit user code should avoid copying jsval/JS::Value as much as possible,
 *   preferring to pass by const Value&.
 *
 * Spectre mitigations
 * ===================
 * To mitigate Spectre attacks, we do the following:
 *
 * - On 64-bit platforms, when unboxing a Value, we XOR the bits with the
 *   expected type tag (instead of masking the payload bits). This guarantees
 *   that toString, toObject, toSymbol will return an invalid pointer (because
 *   some high bits will be set) when called on a Value with a different type
 *   tag.
 *
 * - On 32-bit platforms,when unboxing an object/string/symbol Value, we use a
 *   conditional move (not speculated) to zero the payload register if the type
 *   doesn't match.
 */
union alignas(8) Value {
 private:
  uint64_t asBits_;
  double asDouble_;

#if defined(JS_PUNBOX64) && !defined(_WIN64)
  // MSVC doesn't pack these correctly :-(
  struct {
#if MOZ_LITTLE_ENDIAN
    uint64_t payload47_ : 47;
    JSValueTag tag_ : 17;
#else
    JSValueTag tag_ : 17;
    uint64_t payload47_ : 47;
#endif  // MOZ_LITTLE_ENDIAN
  } debugView_;
#endif  // defined(JS_PUNBOX64) && !defined(_WIN64)

  struct {
#if defined(JS_PUNBOX64)
#if MOZ_BIG_ENDIAN
    uint32_t : 32;  // padding
#endif              // MOZ_BIG_ENDIAN
    union {
      int32_t i32_;
      uint32_t u32_;
      JSWhyMagic why_;
    } payload_;
#elif defined(JS_NUNBOX32)
#if MOZ_BIG_ENDIAN
    JSValueTag tag_;
#endif  // MOZ_BIG_ENDIAN
    union {
      int32_t i32_;
      uint32_t u32_;
      uint32_t boo_;  // Don't use |bool| -- it must be four bytes.
      JSString* str_;
      JS::Symbol* sym_;
#ifdef ENABLE_BIGINT
      JS::BigInt* bi_;
#endif
      JSObject* obj_;
      js::gc::Cell* cell_;
      void* ptr_;
      JSWhyMagic why_;
    } payload_;
#if MOZ_LITTLE_ENDIAN
    JSValueTag tag_;
#endif  // MOZ_LITTLE_ENDIAN
#endif  // defined(JS_PUNBOX64)
  } s_;

 public:
  constexpr Value() : asBits_(bitsFromTagAndPayload(JSVAL_TAG_UNDEFINED, 0)) {}
  Value(const Value& v) = default;

 private:
  explicit constexpr Value(uint64_t asBits) : asBits_(asBits) {}
  explicit constexpr Value(double d) : asDouble_(d) {}

  static_assert(sizeof(JSValueType) == 1,
                "type bits must fit in a single byte");
  static_assert(sizeof(JSValueTag) == 4,
                "32-bit Value's tag_ must have size 4 to complement the "
                "payload union's size 4");
  static_assert(sizeof(JSWhyMagic) <= 4,
                "32-bit Value's JSWhyMagic payload field must not inflate "
                "the payload beyond 4 bytes");

 public:
#if defined(JS_NUNBOX32)
  using PayloadType = uint32_t;
#elif defined(JS_PUNBOX64)
  using PayloadType = uint64_t;
#endif

  static constexpr uint64_t bitsFromTagAndPayload(JSValueTag tag,
                                                  PayloadType payload) {
#if defined(JS_NUNBOX32)
    return (uint64_t(uint32_t(tag)) << 32) | payload;
#elif defined(JS_PUNBOX64)
    return (uint64_t(uint32_t(tag)) << JSVAL_TAG_SHIFT) | payload;
#endif
  }

  static constexpr Value fromTagAndPayload(JSValueTag tag,
                                           PayloadType payload) {
    return fromRawBits(bitsFromTagAndPayload(tag, payload));
  }

  static constexpr Value fromRawBits(uint64_t asBits) { return Value(asBits); }

  static constexpr Value fromInt32(int32_t i) {
    return fromTagAndPayload(JSVAL_TAG_INT32, uint32_t(i));
  }

  static constexpr Value fromDouble(double d) { return Value(d); }

 public:
  /**
   * Returns false if creating a NumberValue containing the given type would
   * be lossy, true otherwise.
   */
  template <typename T>
  static bool isNumberRepresentable(const T t) {
    return T(double(t)) == t;
  }

  /*** Mutators ***/

  void setNull() { asBits_ = bitsFromTagAndPayload(JSVAL_TAG_NULL, 0); }

  void setUndefined() {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_UNDEFINED, 0);
  }

  void setInt32(int32_t i) {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_INT32, uint32_t(i));
  }

  void setDouble(double d) {
    // Don't assign to asDouble_ to fix a miscompilation with GCC 5.2.1 and
    // 5.3.1. See bug 1312488.
    *this = Value(d);
    MOZ_ASSERT(isDouble());
  }

  void setNaN() { setDouble(GenericNaN()); }

  void setString(JSString* str) {
    MOZ_ASSERT(js::gc::IsCellPointerValid(str));
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_STRING, PayloadType(str));
  }

  void setSymbol(JS::Symbol* sym) {
    MOZ_ASSERT(js::gc::IsCellPointerValid(sym));
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_SYMBOL, PayloadType(sym));
  }

#ifdef ENABLE_BIGINT
  void setBigInt(JS::BigInt* bi) {
    MOZ_ASSERT(js::gc::IsCellPointerValid(bi));
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_BIGINT, PayloadType(bi));
  }
#endif

  void setObject(JSObject& obj) {
    MOZ_ASSERT(js::gc::IsCellPointerValid(&obj));

#if defined(JS_PUNBOX64)
    // VisualStudio cannot contain parenthesized C++ style cast and shift
    // inside decltype in template parameter:
    //   AssertionConditionType<decltype((uintptr_t(x) >> 1))>
    // It throws syntax error.
    MOZ_ASSERT((((uintptr_t)&obj) >> JSVAL_TAG_SHIFT) == 0);
#endif
    setObjectNoCheck(&obj);
  }

 private:
  void setObjectNoCheck(JSObject* obj) {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_OBJECT, PayloadType(obj));
  }

  friend inline Value js::PoisonedObjectValue(uintptr_t poison);

 public:
  void setBoolean(bool b) {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_BOOLEAN, uint32_t(b));
  }

  void setMagic(JSWhyMagic why) {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_MAGIC, uint32_t(why));
  }

  void setMagicUint32(uint32_t payload) {
    asBits_ = bitsFromTagAndPayload(JSVAL_TAG_MAGIC, payload);
  }

  bool setNumber(uint32_t ui) {
    if (ui > JSVAL_INT_MAX) {
      setDouble((double)ui);
      return false;
    } else {
      setInt32((int32_t)ui);
      return true;
    }
  }

  bool setNumber(double d) {
    int32_t i;
    if (mozilla::NumberIsInt32(d, &i)) {
      setInt32(i);
      return true;
    }

    setDouble(d);
    return false;
  }

  void setObjectOrNull(JSObject* arg) {
    if (arg) {
      setObject(*arg);
    } else {
      setNull();
    }
  }

  void swap(Value& rhs) {
    uint64_t tmp = rhs.asBits_;
    rhs.asBits_ = asBits_;
    asBits_ = tmp;
  }

 private:
  JSValueTag toTag() const {
#if defined(JS_NUNBOX32)
    return s_.tag_;
#elif defined(JS_PUNBOX64)
    return JSValueTag(asBits_ >> JSVAL_TAG_SHIFT);
#endif
  }

 public:
  /*** JIT-only interfaces to interact with and create raw Values ***/
#if defined(JS_NUNBOX32)
  PayloadType toNunboxPayload() const {
    return static_cast<PayloadType>(s_.payload_.i32_);
  }

  JSValueTag toNunboxTag() const { return s_.tag_; }
#elif defined(JS_PUNBOX64)
  const void* bitsAsPunboxPointer() const {
    return reinterpret_cast<void*>(asBits_);
  }
#endif

  /*** Value type queries ***/

  /*
   * N.B. GCC, in some but not all cases, chooses to emit signed comparison
   * of JSValueTag even though its underlying type has been forced to be
   * uint32_t.  Thus, all comparisons should explicitly cast operands to
   * uint32_t.
   */

  bool isUndefined() const {
#if defined(JS_NUNBOX32)
    return toTag() == JSVAL_TAG_UNDEFINED;
#elif defined(JS_PUNBOX64)
    return asBits_ == JSVAL_SHIFTED_TAG_UNDEFINED;
#endif
  }

  bool isNull() const {
#if defined(JS_NUNBOX32)
    return toTag() == JSVAL_TAG_NULL;
#elif defined(JS_PUNBOX64)
    return asBits_ == JSVAL_SHIFTED_TAG_NULL;
#endif
  }

  bool isNullOrUndefined() const { return isNull() || isUndefined(); }

  bool isInt32() const { return toTag() == JSVAL_TAG_INT32; }

  bool isInt32(int32_t i32) const {
    return asBits_ == bitsFromTagAndPayload(JSVAL_TAG_INT32, uint32_t(i32));
  }

  bool isDouble() const {
#if defined(JS_NUNBOX32)
    return uint32_t(toTag()) <= uint32_t(JSVAL_TAG_CLEAR);
#elif defined(JS_PUNBOX64)
    return (asBits_ | mozilla::FloatingPoint<double>::kSignBit) <=
           JSVAL_SHIFTED_TAG_MAX_DOUBLE;
#endif
  }

  bool isNumber() const {
#if defined(JS_NUNBOX32)
    MOZ_ASSERT(toTag() != JSVAL_TAG_CLEAR);
    return uint32_t(toTag()) <= uint32_t(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET);
#elif defined(JS_PUNBOX64)
    return asBits_ < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_NUMBER_SET;
#endif
  }

  bool isString() const { return toTag() == JSVAL_TAG_STRING; }

  bool isSymbol() const { return toTag() == JSVAL_TAG_SYMBOL; }

#ifdef ENABLE_BIGINT
  bool isBigInt() const { return toTag() == JSVAL_TAG_BIGINT; }
#endif

  bool isObject() const {
#if defined(JS_NUNBOX32)
    return toTag() == JSVAL_TAG_OBJECT;
#elif defined(JS_PUNBOX64)
    MOZ_ASSERT((asBits_ >> JSVAL_TAG_SHIFT) <= JSVAL_TAG_OBJECT);
    return asBits_ >= JSVAL_SHIFTED_TAG_OBJECT;
#endif
  }

  bool isPrimitive() const {
#if defined(JS_NUNBOX32)
    return uint32_t(toTag()) < uint32_t(JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET);
#elif defined(JS_PUNBOX64)
    return asBits_ < JSVAL_UPPER_EXCL_SHIFTED_TAG_OF_PRIMITIVE_SET;
#endif
  }

  bool isObjectOrNull() const { return isObject() || isNull(); }

  bool isGCThing() const {
#if defined(JS_NUNBOX32)
    /* gcc sometimes generates signed < without explicit casts. */
    return uint32_t(toTag()) >= uint32_t(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET);
#elif defined(JS_PUNBOX64)
    return asBits_ >= JSVAL_LOWER_INCL_SHIFTED_TAG_OF_GCTHING_SET;
#endif
  }

  bool isBoolean() const { return toTag() == JSVAL_TAG_BOOLEAN; }

  bool isTrue() const {
    return asBits_ == bitsFromTagAndPayload(JSVAL_TAG_BOOLEAN, uint32_t(true));
  }

  bool isFalse() const {
    return asBits_ == bitsFromTagAndPayload(JSVAL_TAG_BOOLEAN, uint32_t(false));
  }

  bool isMagic() const { return toTag() == JSVAL_TAG_MAGIC; }

  bool isMagic(JSWhyMagic why) const {
    MOZ_ASSERT_IF(isMagic(), s_.payload_.why_ == why);
    return isMagic();
  }

  JS::TraceKind traceKind() const {
    MOZ_ASSERT(isGCThing());
    static_assert((JSVAL_TAG_STRING & 0x03) == size_t(JS::TraceKind::String),
                  "Value type tags must correspond with JS::TraceKinds.");
    static_assert((JSVAL_TAG_SYMBOL & 0x03) == size_t(JS::TraceKind::Symbol),
                  "Value type tags must correspond with JS::TraceKinds.");
    static_assert((JSVAL_TAG_OBJECT & 0x03) == size_t(JS::TraceKind::Object),
                  "Value type tags must correspond with JS::TraceKinds.");
    if (MOZ_UNLIKELY(isPrivateGCThing())) {
      return JS::GCThingTraceKind(toGCThing());
    }
#ifdef ENABLE_BIGINT
    if (MOZ_UNLIKELY(isBigInt())) {
      return JS::TraceKind::BigInt;
    }
#endif
    return JS::TraceKind(toTag() & 0x03);
  }

  JSWhyMagic whyMagic() const {
    MOZ_ASSERT(isMagic());
    return s_.payload_.why_;
  }

  uint32_t magicUint32() const {
    MOZ_ASSERT(isMagic());
    return s_.payload_.u32_;
  }

  /*** Comparison ***/

  bool operator==(const Value& rhs) const { return asBits_ == rhs.asBits_; }

  bool operator!=(const Value& rhs) const { return asBits_ != rhs.asBits_; }

  friend inline bool SameType(const Value& lhs, const Value& rhs);

  /*** Extract the value's typed payload ***/

  int32_t toInt32() const {
    MOZ_ASSERT(isInt32());
#if defined(JS_NUNBOX32)
    return s_.payload_.i32_;
#elif defined(JS_PUNBOX64)
    return int32_t(asBits_);
#endif
  }

  double toDouble() const {
    MOZ_ASSERT(isDouble());
    return asDouble_;
  }

  double toNumber() const {
    MOZ_ASSERT(isNumber());
    return isDouble() ? toDouble() : double(toInt32());
  }

  JSString* toString() const {
    MOZ_ASSERT(isString());
#if defined(JS_NUNBOX32)
    return s_.payload_.str_;
#elif defined(JS_PUNBOX64)
    return reinterpret_cast<JSString*>(asBits_ ^ JSVAL_SHIFTED_TAG_STRING);
#endif
  }

  JS::Symbol* toSymbol() const {
    MOZ_ASSERT(isSymbol());
#if defined(JS_NUNBOX32)
    return s_.payload_.sym_;
#elif defined(JS_PUNBOX64)
    return reinterpret_cast<JS::Symbol*>(asBits_ ^ JSVAL_SHIFTED_TAG_SYMBOL);
#endif
  }

#ifdef ENABLE_BIGINT
  JS::BigInt* toBigInt() const {
    MOZ_ASSERT(isBigInt());
#if defined(JS_NUNBOX32)
    return s_.payload_.bi_;
#elif defined(JS_PUNBOX64)
    return reinterpret_cast<JS::BigInt*>(asBits_ ^ JSVAL_SHIFTED_TAG_BIGINT);
#endif
  }
#endif

  JSObject& toObject() const {
    MOZ_ASSERT(isObject());
#if defined(JS_NUNBOX32)
    return *s_.payload_.obj_;
#elif defined(JS_PUNBOX64)
    uint64_t ptrBits = asBits_ ^ JSVAL_SHIFTED_TAG_OBJECT;
    MOZ_ASSERT(ptrBits);
    MOZ_ASSERT((ptrBits & 0x7) == 0);
    return *reinterpret_cast<JSObject*>(ptrBits);
#endif
  }

  JSObject* toObjectOrNull() const {
    MOZ_ASSERT(isObjectOrNull());
#if defined(JS_NUNBOX32)
    return s_.payload_.obj_;
#elif defined(JS_PUNBOX64)
    // Note: the 'Spectre mitigations' comment at the top of this class
    // explains why we use XOR here and in other to* methods.
    uint64_t ptrBits =
        (asBits_ ^ JSVAL_SHIFTED_TAG_OBJECT) & ~JSVAL_OBJECT_OR_NULL_BIT;
    MOZ_ASSERT((ptrBits & 0x7) == 0);
    return reinterpret_cast<JSObject*>(ptrBits);
#endif
  }

  js::gc::Cell* toGCThing() const {
    MOZ_ASSERT(isGCThing());
#if defined(JS_NUNBOX32)
    return s_.payload_.cell_;
#elif defined(JS_PUNBOX64)
    uint64_t ptrBits = asBits_ & JSVAL_PAYLOAD_MASK_GCTHING;
    MOZ_ASSERT((ptrBits & 0x7) == 0);
    return reinterpret_cast<js::gc::Cell*>(ptrBits);
#endif
  }

  GCCellPtr toGCCellPtr() const { return GCCellPtr(toGCThing(), traceKind()); }

  bool toBoolean() const {
    MOZ_ASSERT(isBoolean());
#if defined(JS_NUNBOX32)
    return bool(s_.payload_.boo_);
#elif defined(JS_PUNBOX64)
    return bool(int32_t(asBits_));
#endif
  }

  uint32_t payloadAsRawUint32() const {
    MOZ_ASSERT(!isDouble());
    return s_.payload_.u32_;
  }

  uint64_t asRawBits() const { return asBits_; }

  JSValueType extractNonDoubleType() const {
    uint32_t type = toTag() & 0xF;
    MOZ_ASSERT(type > JSVAL_TYPE_DOUBLE);
    return JSValueType(type);
  }

  /*
   * Private API
   *
   * Private setters/getters allow the caller to read/write arbitrary types
   * that fit in the 64-bit payload. It is the caller's responsibility, after
   * storing to a value with setPrivateX to read only using getPrivateX.
   * Privates values are given a type which ensures they are not marked.
   */

  void setPrivate(void* ptr) {
    MOZ_ASSERT((uintptr_t(ptr) & 1) == 0);
#if defined(JS_NUNBOX32)
    s_.tag_ = JSValueTag(0);
    s_.payload_.ptr_ = ptr;
#elif defined(JS_PUNBOX64)
    asBits_ = uintptr_t(ptr) >> 1;
#endif
    MOZ_ASSERT(isDouble());
  }

  void* toPrivate() const {
    MOZ_ASSERT(isDouble());
#if defined(JS_NUNBOX32)
    return s_.payload_.ptr_;
#elif defined(JS_PUNBOX64)
    MOZ_ASSERT((asBits_ & 0x8000000000000000ULL) == 0);
    return reinterpret_cast<void*>(asBits_ << 1);
#endif
  }

  void setPrivateUint32(uint32_t ui) {
    MOZ_ASSERT(uint32_t(int32_t(ui)) == ui);
    setInt32(int32_t(ui));
  }

  uint32_t toPrivateUint32() const { return uint32_t(toInt32()); }

  /*
   * Private GC Thing API
   *
   * Non-JSObject, JSString, and JS::Symbol cells may be put into the 64-bit
   * payload as private GC things. Such Values are considered isGCThing(), and
   * as such, automatically marked. Their traceKind() is gotten via their
   * cells.
   */

  void setPrivateGCThing(js::gc::Cell* cell) {
    MOZ_ASSERT(JS::GCThingTraceKind(cell) != JS::TraceKind::String,
               "Private GC thing Values must not be strings. Make a "
               "StringValue instead.");
    MOZ_ASSERT(JS::GCThingTraceKind(cell) != JS::TraceKind::Symbol,
               "Private GC thing Values must not be symbols. Make a "
               "SymbolValue instead.");
#ifdef ENABLE_BIGINT
    MOZ_ASSERT(JS::GCThingTraceKind(cell) != JS::TraceKind::BigInt,
               "Private GC thing Values must not be BigInts. Make a "
               "BigIntValue instead.");
#endif
    MOZ_ASSERT(JS::GCThingTraceKind(cell) != JS::TraceKind::Object,
               "Private GC thing Values must not be objects. Make an "
               "ObjectValue instead.");

    MOZ_ASSERT(js::gc::IsCellPointerValid(cell));
#if defined(JS_PUNBOX64)
    // VisualStudio cannot contain parenthesized C++ style cast and shift
    // inside decltype in template parameter:
    //   AssertionConditionType<decltype((uintptr_t(x) >> 1))>
    // It throws syntax error.
    MOZ_ASSERT((((uintptr_t)cell) >> JSVAL_TAG_SHIFT) == 0);
#endif
    asBits_ =
        bitsFromTagAndPayload(JSVAL_TAG_PRIVATE_GCTHING, PayloadType(cell));
  }

  bool isPrivateGCThing() const { return toTag() == JSVAL_TAG_PRIVATE_GCTHING; }
} JS_HAZ_GC_POINTER MOZ_NON_PARAM;

static_assert(sizeof(Value) == 8,
              "Value size must leave three tag bits, be a binary power, and "
              "is ubiquitously depended upon everywhere");

inline bool IsOptimizedPlaceholderMagicValue(const Value& v) {
  if (v.isMagic()) {
    MOZ_ASSERT(v.whyMagic() == JS_OPTIMIZED_ARGUMENTS ||
               v.whyMagic() == JS_OPTIMIZED_OUT);
    return true;
  }
  return false;
}

static MOZ_ALWAYS_INLINE void ExposeValueToActiveJS(const Value& v) {
#ifdef DEBUG
  Value tmp = v;
  MOZ_ASSERT(!js::gc::EdgeNeedsSweepUnbarrieredSlow(&tmp));
#endif
  if (v.isGCThing()) {
    js::gc::ExposeGCThingToActiveJS(GCCellPtr(v));
  }
}

/************************************************************************/

static inline MOZ_MAY_CALL_AFTER_MUST_RETURN Value NullValue() {
  Value v;
  v.setNull();
  return v;
}

static inline constexpr Value UndefinedValue() { return Value(); }

static inline constexpr Value Int32Value(int32_t i32) {
  return Value::fromInt32(i32);
}

static inline Value DoubleValue(double dbl) {
  Value v;
  v.setDouble(dbl);
  return v;
}

static inline Value CanonicalizedDoubleValue(double d) {
  return MOZ_UNLIKELY(mozilla::IsNaN(d))
             ? Value::fromRawBits(detail::CanonicalizedNaNBits)
             : Value::fromDouble(d);
}

static inline bool IsCanonicalized(double d) {
  if (mozilla::IsInfinite(d) || mozilla::IsFinite(d)) {
    return true;
  }

  uint64_t bits;
  mozilla::BitwiseCast<uint64_t>(d, &bits);
  return (bits & ~mozilla::FloatingPoint<double>::kSignBit) ==
         detail::CanonicalizedNaNBits;
}

static inline Value DoubleNaNValue() {
  Value v;
  v.setNaN();
  return v;
}

static inline Value Float32Value(float f) {
  Value v;
  v.setDouble(f);
  return v;
}

static inline Value StringValue(JSString* str) {
  Value v;
  v.setString(str);
  return v;
}

static inline Value SymbolValue(JS::Symbol* sym) {
  Value v;
  v.setSymbol(sym);
  return v;
}

#ifdef ENABLE_BIGINT
static inline Value BigIntValue(JS::BigInt* bi) {
  Value v;
  v.setBigInt(bi);
  return v;
}
#endif

static inline Value BooleanValue(bool boo) {
  Value v;
  v.setBoolean(boo);
  return v;
}

static inline Value TrueValue() {
  Value v;
  v.setBoolean(true);
  return v;
}

static inline Value FalseValue() {
  Value v;
  v.setBoolean(false);
  return v;
}

static inline Value ObjectValue(JSObject& obj) {
  Value v;
  v.setObject(obj);
  return v;
}

static inline Value MagicValue(JSWhyMagic why) {
  Value v;
  v.setMagic(why);
  return v;
}

static inline Value MagicValueUint32(uint32_t payload) {
  Value v;
  v.setMagicUint32(payload);
  return v;
}

static inline Value NumberValue(float f) {
  Value v;
  v.setNumber(f);
  return v;
}

static inline Value NumberValue(double dbl) {
  Value v;
  v.setNumber(dbl);
  return v;
}

static inline Value NumberValue(int8_t i) { return Int32Value(i); }

static inline Value NumberValue(uint8_t i) { return Int32Value(i); }

static inline Value NumberValue(int16_t i) { return Int32Value(i); }

static inline Value NumberValue(uint16_t i) { return Int32Value(i); }

static inline Value NumberValue(int32_t i) { return Int32Value(i); }

static inline constexpr Value NumberValue(uint32_t i) {
  return i <= JSVAL_INT_MAX ? Int32Value(int32_t(i))
                            : Value::fromDouble(double(i));
}

namespace detail {

template <bool Signed>
class MakeNumberValue {
 public:
  template <typename T>
  static inline Value create(const T t) {
    Value v;
    if (JSVAL_INT_MIN <= t && t <= JSVAL_INT_MAX) {
      v.setInt32(int32_t(t));
    } else {
      v.setDouble(double(t));
    }
    return v;
  }
};

template <>
class MakeNumberValue<false> {
 public:
  template <typename T>
  static inline Value create(const T t) {
    Value v;
    if (t <= JSVAL_INT_MAX) {
      v.setInt32(int32_t(t));
    } else {
      v.setDouble(double(t));
    }
    return v;
  }
};

}  // namespace detail

template <typename T>
static inline Value NumberValue(const T t) {
  MOZ_ASSERT(Value::isNumberRepresentable(t), "value creation would be lossy");
  return detail::MakeNumberValue<std::numeric_limits<T>::is_signed>::create(t);
}

static inline Value ObjectOrNullValue(JSObject* obj) {
  Value v;
  v.setObjectOrNull(obj);
  return v;
}

static inline Value PrivateValue(void* ptr) {
  Value v;
  v.setPrivate(ptr);
  return v;
}

static inline Value PrivateUint32Value(uint32_t ui) {
  Value v;
  v.setPrivateUint32(ui);
  return v;
}

static inline Value PrivateGCThingValue(js::gc::Cell* cell) {
  Value v;
  v.setPrivateGCThing(cell);
  return v;
}

inline bool SameType(const Value& lhs, const Value& rhs) {
#if defined(JS_NUNBOX32)
  JSValueTag ltag = lhs.toTag(), rtag = rhs.toTag();
  return ltag == rtag || (ltag < JSVAL_TAG_CLEAR && rtag < JSVAL_TAG_CLEAR);
#elif defined(JS_PUNBOX64)
  return (lhs.isDouble() && rhs.isDouble()) ||
         (((lhs.asBits_ ^ rhs.asBits_) & 0xFFFF800000000000ULL) == 0);
#endif
}

}  // namespace JS

/************************************************************************/

namespace JS {
JS_PUBLIC_API void HeapValuePostBarrier(Value* valuep, const Value& prev,
                                        const Value& next);

template <>
struct GCPolicy<JS::Value> {
  static void trace(JSTracer* trc, Value* v, const char* name) {
    js::UnsafeTraceManuallyBarrieredEdge(trc, v, name);
  }
  static bool isTenured(const Value& thing) {
    return !thing.isGCThing() || !IsInsideNursery(thing.toGCThing());
  }
  static bool isValid(const Value& value) {
    return !value.isGCThing() || js::gc::IsCellPointerValid(value.toGCThing());
  }
};

}  // namespace JS

namespace js {

template <>
struct BarrierMethods<JS::Value> {
  static gc::Cell* asGCThingOrNull(const JS::Value& v) {
    return v.isGCThing() ? v.toGCThing() : nullptr;
  }
  static void postBarrier(JS::Value* v, const JS::Value& prev,
                          const JS::Value& next) {
    JS::HeapValuePostBarrier(v, prev, next);
  }
  static void exposeToJS(const JS::Value& v) { JS::ExposeValueToActiveJS(v); }
};

template <class Wrapper>
class MutableValueOperations;

/**
 * A class designed for CRTP use in implementing the non-mutating parts of the
 * Value interface in Value-like classes.  Wrapper must be a class inheriting
 * ValueOperations<Wrapper> with a visible get() method returning a const
 * reference to the Value abstracted by Wrapper.
 */
template <class Wrapper>
class WrappedPtrOperations<JS::Value, Wrapper> {
  const JS::Value& value() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isUndefined() const { return value().isUndefined(); }
  bool isNull() const { return value().isNull(); }
  bool isBoolean() const { return value().isBoolean(); }
  bool isTrue() const { return value().isTrue(); }
  bool isFalse() const { return value().isFalse(); }
  bool isNumber() const { return value().isNumber(); }
  bool isInt32() const { return value().isInt32(); }
  bool isInt32(int32_t i32) const { return value().isInt32(i32); }
  bool isDouble() const { return value().isDouble(); }
  bool isString() const { return value().isString(); }
  bool isSymbol() const { return value().isSymbol(); }
#ifdef ENABLE_BIGINT
  bool isBigInt() const { return value().isBigInt(); }
#endif
  bool isObject() const { return value().isObject(); }
  bool isMagic() const { return value().isMagic(); }
  bool isMagic(JSWhyMagic why) const { return value().isMagic(why); }
  bool isGCThing() const { return value().isGCThing(); }
  bool isPrimitive() const { return value().isPrimitive(); }

  bool isNullOrUndefined() const { return value().isNullOrUndefined(); }
  bool isObjectOrNull() const { return value().isObjectOrNull(); }

  bool toBoolean() const { return value().toBoolean(); }
  double toNumber() const { return value().toNumber(); }
  int32_t toInt32() const { return value().toInt32(); }
  double toDouble() const { return value().toDouble(); }
  JSString* toString() const { return value().toString(); }
  JS::Symbol* toSymbol() const { return value().toSymbol(); }
#ifdef ENABLE_BIGINT
  JS::BigInt* toBigInt() const { return value().toBigInt(); }
#endif
  JSObject& toObject() const { return value().toObject(); }
  JSObject* toObjectOrNull() const { return value().toObjectOrNull(); }
  gc::Cell* toGCThing() const { return value().toGCThing(); }
  JS::TraceKind traceKind() const { return value().traceKind(); }
  void* toPrivate() const { return value().toPrivate(); }
  uint32_t toPrivateUint32() const { return value().toPrivateUint32(); }

  uint64_t asRawBits() const { return value().asRawBits(); }
  JSValueType extractNonDoubleType() const {
    return value().extractNonDoubleType();
  }

  JSWhyMagic whyMagic() const { return value().whyMagic(); }
  uint32_t magicUint32() const { return value().magicUint32(); }
};

/**
 * A class designed for CRTP use in implementing all the mutating parts of the
 * Value interface in Value-like classes.  Wrapper must be a class inheriting
 * MutableWrappedPtrOperations<Wrapper> with visible get() methods returning
 * const and non-const references to the Value abstracted by Wrapper.
 */
template <class Wrapper>
class MutableWrappedPtrOperations<JS::Value, Wrapper>
    : public WrappedPtrOperations<JS::Value, Wrapper> {
  JS::Value& value() { return static_cast<Wrapper*>(this)->get(); }

 public:
  void setNull() { value().setNull(); }
  void setUndefined() { value().setUndefined(); }
  void setInt32(int32_t i) { value().setInt32(i); }
  void setDouble(double d) { value().setDouble(d); }
  void setNaN() { setDouble(JS::GenericNaN()); }
  void setBoolean(bool b) { value().setBoolean(b); }
  void setMagic(JSWhyMagic why) { value().setMagic(why); }
  bool setNumber(uint32_t ui) { return value().setNumber(ui); }
  bool setNumber(double d) { return value().setNumber(d); }
  void setString(JSString* str) { this->value().setString(str); }
  void setSymbol(JS::Symbol* sym) { this->value().setSymbol(sym); }
#ifdef ENABLE_BIGINT
  void setBigInt(JS::BigInt* bi) { this->value().setBigInt(bi); }
#endif
  void setObject(JSObject& obj) { this->value().setObject(obj); }
  void setObjectOrNull(JSObject* arg) { this->value().setObjectOrNull(arg); }
  void setPrivate(void* ptr) { this->value().setPrivate(ptr); }
  void setPrivateUint32(uint32_t ui) { this->value().setPrivateUint32(ui); }
  void setPrivateGCThing(js::gc::Cell* cell) {
    this->value().setPrivateGCThing(cell);
  }
};

/*
 * Augment the generic Heap<T> interface when T = Value with
 * type-querying, value-extracting, and mutating operations.
 */
template <typename Wrapper>
class HeapBase<JS::Value, Wrapper>
    : public WrappedPtrOperations<JS::Value, Wrapper> {
  void setBarriered(const JS::Value& v) {
    *static_cast<JS::Heap<JS::Value>*>(this) = v;
  }

 public:
  void setNull() { setBarriered(JS::NullValue()); }
  void setUndefined() { setBarriered(JS::UndefinedValue()); }
  void setInt32(int32_t i) { setBarriered(JS::Int32Value(i)); }
  void setDouble(double d) { setBarriered(JS::DoubleValue(d)); }
  void setNaN() { setDouble(JS::GenericNaN()); }
  void setBoolean(bool b) { setBarriered(JS::BooleanValue(b)); }
  void setMagic(JSWhyMagic why) { setBarriered(JS::MagicValue(why)); }
  void setString(JSString* str) { setBarriered(JS::StringValue(str)); }
  void setSymbol(JS::Symbol* sym) { setBarriered(JS::SymbolValue(sym)); }
#ifdef ENABLE_BIGINT
  void setBigInt(JS::BigInt* bi) { setBarriered(JS::BigIntValue(bi)); }
#endif
  void setObject(JSObject& obj) { setBarriered(JS::ObjectValue(obj)); }
  void setPrivateGCThing(js::gc::Cell* cell) {
    setBarriered(JS::PrivateGCThingValue(cell));
  }

  bool setNumber(uint32_t ui) {
    if (ui > JSVAL_INT_MAX) {
      setDouble((double)ui);
      return false;
    } else {
      setInt32((int32_t)ui);
      return true;
    }
  }

  bool setNumber(double d) {
    int32_t i;
    if (mozilla::NumberIsInt32(d, &i)) {
      setInt32(i);
      return true;
    }

    setDouble(d);
    return false;
  }

  void setObjectOrNull(JSObject* arg) {
    if (arg) {
      setObject(*arg);
    } else {
      setNull();
    }
  }
};

/*
 * If the Value is a GC pointer type, convert to that type and call |f| with
 * the pointer. If the Value is not a GC type, calls F::defaultValue.
 */
template <typename F, typename... Args>
auto DispatchTyped(F f, const JS::Value& val, Args&&... args)
    -> decltype(f(static_cast<JSObject*>(nullptr),
                  std::forward<Args>(args)...)) {
  if (val.isString()) {
    JSString* str = val.toString();
    MOZ_ASSERT(gc::IsCellPointerValid(str));
    return f(str, std::forward<Args>(args)...);
  }
  if (val.isObject()) {
    JSObject* obj = &val.toObject();
    MOZ_ASSERT(gc::IsCellPointerValid(obj));
    return f(obj, std::forward<Args>(args)...);
  }
  if (val.isSymbol()) {
    JS::Symbol* sym = val.toSymbol();
    MOZ_ASSERT(gc::IsCellPointerValid(sym));
    return f(sym, std::forward<Args>(args)...);
  }
#ifdef ENABLE_BIGINT
  if (val.isBigInt()) {
    JS::BigInt* bi = val.toBigInt();
    MOZ_ASSERT(gc::IsCellPointerValid(bi));
    return f(bi, std::forward<Args>(args)...);
  }
#endif
  if (MOZ_UNLIKELY(val.isPrivateGCThing())) {
    MOZ_ASSERT(gc::IsCellPointerValid(val.toGCThing()));
    return DispatchTyped(f, val.toGCCellPtr(), std::forward<Args>(args)...);
  }
  MOZ_ASSERT(!val.isGCThing());
  return F::defaultValue(val);
}

template <class S>
struct VoidDefaultAdaptor {
  static void defaultValue(const S&) {}
};
template <class S>
struct IdentityDefaultAdaptor {
  static S defaultValue(const S& v) { return v; }
};
template <class S, bool v>
struct BoolDefaultAdaptor {
  static bool defaultValue(const S&) { return v; }
};

static inline JS::Value PoisonedObjectValue(uintptr_t poison) {
  JS::Value v;
  v.setObjectNoCheck(reinterpret_cast<JSObject*>(poison));
  return v;
}

}  // namespace js

#ifdef DEBUG
namespace JS {

MOZ_ALWAYS_INLINE bool ValueIsNotGray(const Value& value) {
  if (!value.isGCThing()) {
    return true;
  }

  return CellIsNotGray(value.toGCThing());
}

MOZ_ALWAYS_INLINE bool ValueIsNotGray(const Heap<Value>& value) {
  return ValueIsNotGray(value.unbarrieredGet());
}

}  // namespace JS
#endif

/************************************************************************/

namespace JS {

extern JS_PUBLIC_DATA const HandleValue NullHandleValue;
extern JS_PUBLIC_DATA const HandleValue UndefinedHandleValue;
extern JS_PUBLIC_DATA const HandleValue TrueHandleValue;
extern JS_PUBLIC_DATA const HandleValue FalseHandleValue;

}  // namespace JS

#endif /* js_Value_h */
