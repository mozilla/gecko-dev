/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmTextToBinary.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"

#include "jsnum.h"

#include "builtin/String.h"
#include "ds/LifoAlloc.h"
#include "js/CharacterEncoding.h"
#include "js/HashTable.h"
#include "js/Printf.h"
#include "util/DoubleToString.h"
#include "wasm/WasmAST.h"
#include "wasm/WasmTypes.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::wasm;

using mozilla::BitwiseCast;
using mozilla::CeilingLog2;
using mozilla::CheckedInt;
using mozilla::CountLeadingZeroes32;
using mozilla::FloatingPoint;
using mozilla::IsPowerOfTwo;
using mozilla::Maybe;
using mozilla::PositiveInfinity;

/*****************************************************************************/
// wasm text token stream

namespace {

class WasmToken {
 public:
  enum FloatLiteralKind { HexNumber, DecNumber, Infinity, NaN };

  enum Kind {
    Align,
    AnyFunc,
    AtomicCmpXchg,
    AtomicLoad,
    AtomicRMW,
    AtomicStore,
    BinaryOpcode,
    Block,
    Br,
    BrIf,
    BrTable,
    Call,
    CallIndirect,
    CloseParen,
    ComparisonOpcode,
    Const,
    ConversionOpcode,
    CurrentMemory,
    Data,
    Drop,
    Elem,
    Else,
    End,
    EndOfFile,
    Equal,
    Error,
    Export,
    ExtraConversionOpcode,
    Field,
    Float,
    Func,
#ifdef ENABLE_WASM_GC
    GcFeatureOptIn,
#endif
    GetGlobal,
    GetLocal,
    Global,
    GrowMemory,
    If,
    Import,
    Index,
    Memory,
    NegativeZero,
    Load,
    Local,
    Loop,
#ifdef ENABLE_WASM_BULKMEM_OPS
    MemCopy,
    MemDrop,
    MemFill,
    MemInit,
#endif
    Module,
    Mutable,
    Name,
#ifdef ENABLE_WASM_GC
    StructNew,
    StructGet,
    StructSet,
    StructNarrow,
#endif
    Nop,
    Offset,
    OpenParen,
    Param,
#ifdef ENABLE_WASM_BULKMEM_OPS
    Passive,
#endif
    Ref,
    RefNull,
    Result,
    Return,
    SetGlobal,
    SetLocal,
    Shared,
    SignedInteger,
    Start,
    Struct,
    Store,
    Table,
#ifdef ENABLE_WASM_BULKMEM_OPS
    TableCopy,
    TableDrop,
    TableInit,
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    TableGet,
    TableGrow,
    TableSet,
    TableSize,
#endif
    TeeLocal,
    TernaryOpcode,
    Text,
    Then,
    Type,
    UnaryOpcode,
    Unreachable,
    UnsignedInteger,
    ValueType,
    Wait,
    Wake,
    Invalid
  };
 private:
  Kind kind_;
  const char16_t* begin_;
  const char16_t* end_;
  union U {
    uint32_t index_;
    uint64_t uint_;
    int64_t sint_;
    FloatLiteralKind floatLiteralKind_;
    ValType valueType_;
    Op op_;
    MiscOp miscOp_;
    ThreadOp threadOp_;
    U() : index_(0) {}
  } u;

 public:
  WasmToken() : kind_(Kind::Invalid), begin_(nullptr), end_(nullptr), u() {}
  WasmToken(Kind kind, const char16_t* begin, const char16_t* end)
      : kind_(kind), begin_(begin), end_(end), u{} {
    MOZ_ASSERT(kind_ != Error);
    MOZ_ASSERT(kind_ != Invalid);
    MOZ_ASSERT((kind == EndOfFile) == (begin == end));
  }
  explicit WasmToken(uint32_t index, const char16_t* begin, const char16_t* end)
      : kind_(Index), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    u.index_ = index;
  }
  explicit WasmToken(uint64_t uint, const char16_t* begin, const char16_t* end)
      : kind_(UnsignedInteger), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    u.uint_ = uint;
  }
  explicit WasmToken(int64_t sint, const char16_t* begin, const char16_t* end)
      : kind_(SignedInteger), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    u.sint_ = sint;
  }
  explicit WasmToken(FloatLiteralKind floatLiteralKind, const char16_t* begin,
                     const char16_t* end)
      : kind_(Float), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    u.floatLiteralKind_ = floatLiteralKind;
  }
  explicit WasmToken(Kind kind, ValType valueType, const char16_t* begin,
                     const char16_t* end)
      : kind_(kind), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
    u.valueType_ = valueType;
  }
  explicit WasmToken(Kind kind, Op op, const char16_t* begin,
                     const char16_t* end)
      : kind_(kind), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode ||
               kind_ == TernaryOpcode || kind_ == ComparisonOpcode ||
               kind_ == ConversionOpcode || kind_ == Load || kind_ == Store);
    u.op_ = op;
  }
  explicit WasmToken(Kind kind, MiscOp op, const char16_t* begin,
                     const char16_t* end)
      : kind_(kind), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    MOZ_ASSERT(kind_ == ExtraConversionOpcode);
    u.miscOp_ = op;
  }
  explicit WasmToken(Kind kind, ThreadOp op, const char16_t* begin,
                     const char16_t* end)
      : kind_(kind), begin_(begin), end_(end) {
    MOZ_ASSERT(begin != end);
    MOZ_ASSERT(kind_ == AtomicCmpXchg || kind_ == AtomicLoad ||
               kind_ == AtomicRMW || kind_ == AtomicStore || kind_ == Wait ||
               kind_ == Wake);
    u.threadOp_ = op;
  }
  explicit WasmToken(const char16_t* begin)
      : kind_(Error), begin_(begin), end_(begin), u{} {}
  Kind kind() const {
    MOZ_ASSERT(kind_ != Kind::Invalid);
    return kind_;
  }
  const char16_t* begin() const { return begin_; }
  const char16_t* end() const { return end_; }
  AstName text() const {
    MOZ_ASSERT(kind_ == Text);
    MOZ_ASSERT(begin_[0] == '"');
    MOZ_ASSERT(end_[-1] == '"');
    MOZ_ASSERT(end_ - begin_ >= 2);
    return AstName(begin_ + 1, end_ - begin_ - 2);
  }
  AstName name() const { return AstName(begin_, end_ - begin_); }
  uint32_t index() const {
    MOZ_ASSERT(kind_ == Index);
    return u.index_;
  }
  uint64_t uint() const {
    MOZ_ASSERT(kind_ == UnsignedInteger);
    return u.uint_;
  }
  int64_t sint() const {
    MOZ_ASSERT(kind_ == SignedInteger);
    return u.sint_;
  }
  FloatLiteralKind floatLiteralKind() const {
    MOZ_ASSERT(kind_ == Float);
    return u.floatLiteralKind_;
  }
  ValType valueType() const {
    MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
    return u.valueType_;
  }
  Op op() const {
    MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode ||
               kind_ == TernaryOpcode || kind_ == ComparisonOpcode ||
               kind_ == ConversionOpcode || kind_ == Load || kind_ == Store);
    return u.op_;
  }
  MiscOp miscOp() const {
    MOZ_ASSERT(kind_ == ExtraConversionOpcode);
    return u.miscOp_;
  }
  ThreadOp threadOp() const {
    MOZ_ASSERT(kind_ == AtomicCmpXchg || kind_ == AtomicLoad ||
               kind_ == AtomicRMW || kind_ == AtomicStore || kind_ == Wait ||
               kind_ == Wake);
    return u.threadOp_;
  }
  bool isOpcode() const {
    switch (kind_) {
      case AtomicCmpXchg:
      case AtomicLoad:
      case AtomicRMW:
      case AtomicStore:
      case BinaryOpcode:
      case Block:
      case Br:
      case BrIf:
      case BrTable:
      case Call:
      case CallIndirect:
      case ComparisonOpcode:
      case Const:
      case ConversionOpcode:
      case ExtraConversionOpcode:
      case CurrentMemory:
      case Drop:
      case GetGlobal:
      case GetLocal:
      case GrowMemory:
      case If:
      case Load:
      case Loop:
#ifdef ENABLE_WASM_BULKMEM_OPS
      case MemCopy:
      case MemDrop:
      case MemFill:
      case MemInit:
#endif
#ifdef ENABLE_WASM_GC
      case StructNew:
      case StructGet:
      case StructSet:
      case StructNarrow:
#endif
      case Nop:
      case RefNull:
      case Return:
      case SetGlobal:
      case SetLocal:
      case Store:
#ifdef ENABLE_WASM_BULKMEM_OPS
      case TableCopy:
      case TableDrop:
      case TableInit:
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
      case TableGet:
      case TableGrow:
      case TableSet:
      case TableSize:
#endif
      case TeeLocal:
      case TernaryOpcode:
      case UnaryOpcode:
      case Unreachable:
      case Wait:
      case Wake:
        return true;
      case Align:
      case AnyFunc:
      case CloseParen:
      case Data:
      case Elem:
      case Else:
      case EndOfFile:
      case Equal:
      case End:
      case Error:
      case Export:
      case Field:
      case Float:
      case Func:
#ifdef ENABLE_WASM_GC
      case GcFeatureOptIn:
#endif
      case Global:
      case Mutable:
      case Import:
      case Index:
      case Memory:
      case NegativeZero:
      case Local:
      case Module:
      case Name:
      case Offset:
      case OpenParen:
      case Param:
#ifdef ENABLE_WASM_BULKMEM_OPS
      case Passive:
#endif
      case Ref:
      case Result:
      case Shared:
      case SignedInteger:
      case Start:
      case Struct:
      case Table:
      case Text:
      case Then:
      case Type:
      case UnsignedInteger:
      case ValueType:
        return false;
      case Invalid:
        break;
    }
    MOZ_CRASH("unexpected token kind");
  }
};

struct InlineImport {
  WasmToken module;
  WasmToken field;
};

}  // end anonymous namespace

static bool IsWasmNewLine(char16_t c) { return c == '\n'; }

static bool IsWasmSpace(char16_t c) {
  switch (c) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case '\f':
      return true;
    default:
      return false;
  }
}

static bool IsWasmDigit(char16_t c) { return c >= '0' && c <= '9'; }

static bool IsWasmLetter(char16_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool IsNameAfterDollar(char16_t c) {
  return IsWasmLetter(c) || IsWasmDigit(c) || c == '_' || c == '$' ||
         c == '-' || c == '.' || c == '>';
}

static bool IsHexDigit(char c, uint8_t* value) {
  if (c >= '0' && c <= '9') {
    *value = c - '0';
    return true;
  }

  if (c >= 'a' && c <= 'f') {
    *value = 10 + (c - 'a');
    return true;
  }

  if (c >= 'A' && c <= 'F') {
    *value = 10 + (c - 'A');
    return true;
  }

  return false;
}

static WasmToken LexHexFloatLiteral(const char16_t* begin, const char16_t* end,
                                    const char16_t** curp) {
  const char16_t* cur = begin;

  if (cur != end && (*cur == '-' || *cur == '+')) {
    cur++;
  }

  MOZ_ASSERT(cur != end && *cur == '0');
  cur++;
  MOZ_ASSERT(cur != end && *cur == 'x');
  cur++;

  uint8_t digit;
  while (cur != end && IsHexDigit(*cur, &digit)) {
    cur++;
  }

  if (cur != end && *cur == '.') {
    cur++;
  }

  while (cur != end && IsHexDigit(*cur, &digit)) {
    cur++;
  }

  if (cur != end && *cur == 'p') {
    cur++;

    if (cur != end && (*cur == '-' || *cur == '+')) {
      cur++;
    }

    while (cur != end && IsWasmDigit(*cur)) {
      cur++;
    }
  }

  *curp = cur;
  return WasmToken(WasmToken::HexNumber, begin, cur);
}

static WasmToken LexDecFloatLiteral(const char16_t* begin, const char16_t* end,
                                    const char16_t** curp) {
  const char16_t* cur = begin;

  if (cur != end && (*cur == '-' || *cur == '+')) {
    cur++;
  }

  while (cur != end && IsWasmDigit(*cur)) {
    cur++;
  }

  if (cur != end && *cur == '.') {
    cur++;
  }

  while (cur != end && IsWasmDigit(*cur)) {
    cur++;
  }

  if (cur != end && *cur == 'e') {
    cur++;

    if (cur != end && (*cur == '-' || *cur == '+')) {
      cur++;
    }

    while (cur != end && IsWasmDigit(*cur)) {
      cur++;
    }
  }

  *curp = cur;
  return WasmToken(WasmToken::DecNumber, begin, cur);
}

static bool ConsumeTextByte(const char16_t** curp, const char16_t* end,
                            uint8_t* byte = nullptr) {
  const char16_t*& cur = *curp;
  MOZ_ASSERT(cur != end);

  if (*cur != '\\') {
    if (byte) {
      *byte = *cur;
    }
    cur++;
    return true;
  }

  if (++cur == end) {
    return false;
  }

  uint8_t u8;
  switch (*cur) {
    case 'n':
      u8 = '\n';
      break;
    case 't':
      u8 = '\t';
      break;
    case '\\':
      u8 = '\\';
      break;
    case '\"':
      u8 = '\"';
      break;
    case '\'':
      u8 = '\'';
      break;
    default: {
      uint8_t highNibble;
      if (!IsHexDigit(*cur, &highNibble)) {
        return false;
      }

      if (++cur == end) {
        return false;
      }

      uint8_t lowNibble;
      if (!IsHexDigit(*cur, &lowNibble)) {
        return false;
      }

      u8 = lowNibble | (highNibble << 4);
      break;
    }
  }

  if (byte) {
    *byte = u8;
  }
  cur++;
  return true;
}

namespace {

class WasmTokenStream {
  static const uint32_t LookaheadSize = 2;

  const char16_t* cur_;
  const char16_t* const end_;
  const char16_t* lineStart_;
  unsigned line_;
  uint32_t lookaheadIndex_;
  uint32_t lookaheadDepth_;
  WasmToken lookahead_[LookaheadSize];

  bool consume(const char16_t* match) {
    const char16_t* p = cur_;
    for (; *match; p++, match++) {
      if (p == end_ || *p != *match) {
        return false;
      }
    }
    cur_ = p;
    return true;
  }
  WasmToken fail(const char16_t* begin) const { return WasmToken(begin); }

  WasmToken nan(const char16_t* begin);
  WasmToken literal(const char16_t* begin);
  WasmToken next();
  void skipSpaces();

 public:
  explicit WasmTokenStream(const char16_t* text)
      : cur_(text),
        end_(text + js_strlen(text)),
        lineStart_(text),
        line_(1),
        lookaheadIndex_(0),
        lookaheadDepth_(0) {}
  void generateError(WasmToken token, UniqueChars* error) {
    unsigned column = token.begin() - lineStart_ + 1;
    *error = JS_smprintf("parsing wasm text at %u:%u", line_, column);
  }
  void generateError(WasmToken token, const char* msg, UniqueChars* error) {
    unsigned column = token.begin() - lineStart_ + 1;
    *error = JS_smprintf("parsing wasm text at %u:%u: %s", line_, column, msg);
  }
  WasmToken peek() {
    if (!lookaheadDepth_) {
      lookahead_[lookaheadIndex_] = next();
      lookaheadDepth_ = 1;
    }
    return lookahead_[lookaheadIndex_];
  }
  WasmToken get() {
    static_assert(LookaheadSize == 2, "can just flip");
    if (lookaheadDepth_) {
      lookaheadDepth_--;
      WasmToken ret = lookahead_[lookaheadIndex_];
      lookaheadIndex_ ^= 1;
      return ret;
    }
    return next();
  }
  void unget(WasmToken token) {
    static_assert(LookaheadSize == 2, "can just flip");
    lookaheadDepth_++;
    lookaheadIndex_ ^= 1;
    lookahead_[lookaheadIndex_] = token;
  }

  // Helpers:
  bool getIf(WasmToken::Kind kind, WasmToken* token) {
    if (peek().kind() == kind) {
      *token = get();
      return true;
    }
    return false;
  }
  bool getIf(WasmToken::Kind kind) {
    WasmToken token;
    if (getIf(kind, &token)) {
      return true;
    }
    return false;
  }
  AstName getIfName() {
    WasmToken token;
    if (getIf(WasmToken::Name, &token)) {
      return token.name();
    }
    return AstName();
  }
  bool getIfRef(AstRef* ref) {
    WasmToken token = peek();
    if (token.kind() == WasmToken::Name || token.kind() == WasmToken::Index) {
      return matchRef(ref, nullptr);
    }
    return false;
  }
  bool getIfOpcode(WasmToken* token) {
    *token = peek();
    if (token->isOpcode()) {
      (void)get();
      return true;
    }
    return false;
  }
  bool match(WasmToken::Kind expect, WasmToken* token, UniqueChars* error) {
    *token = get();
    if (token->kind() == expect) {
      return true;
    }
    generateError(*token, error);
    return false;
  }
  bool match(WasmToken::Kind expect, UniqueChars* error) {
    WasmToken token;
    return match(expect, &token, error);
  }
  bool matchRef(AstRef* ref, UniqueChars* error) {
    WasmToken token = get();
    switch (token.kind()) {
      case WasmToken::Name:
        *ref = AstRef(token.name());
        break;
      case WasmToken::Index:
        if (token.index() != AstNoIndex) {
          *ref = AstRef(token.index());
          break;
        }
        MOZ_FALLTHROUGH;
      default:
        generateError(token, error);
        return false;
    }
    return true;
  }
};

}  // end anonymous namespace

WasmToken WasmTokenStream::nan(const char16_t* begin) {
  if (consume(u":")) {
    if (!consume(u"0x")) {
      return fail(begin);
    }

    uint8_t digit;
    while (cur_ != end_ && IsHexDigit(*cur_, &digit)) {
      cur_++;
    }
  }

  return WasmToken(WasmToken::NaN, begin, cur_);
}

WasmToken WasmTokenStream::literal(const char16_t* begin) {
  CheckedInt<uint64_t> u = 0;
  if (consume(u"0x")) {
    if (cur_ == end_) {
      return fail(begin);
    }

    do {
      if (*cur_ == '.' || *cur_ == 'p') {
        return LexHexFloatLiteral(begin, end_, &cur_);
      }

      uint8_t digit;
      if (!IsHexDigit(*cur_, &digit)) {
        break;
      }

      u *= 16;
      u += digit;
      if (!u.isValid()) {
        return LexHexFloatLiteral(begin, end_, &cur_);
      }

      cur_++;
    } while (cur_ != end_);

    if (*begin == '-') {
      uint64_t value = u.value();
      if (value == 0) {
        return WasmToken(WasmToken::NegativeZero, begin, cur_);
      }
      if (value > uint64_t(INT64_MIN)) {
        return LexHexFloatLiteral(begin, end_, &cur_);
      }

      value = -value;
      return WasmToken(int64_t(value), begin, cur_);
    }
  } else {
    while (cur_ != end_) {
      if (*cur_ == '.' || *cur_ == 'e') {
        return LexDecFloatLiteral(begin, end_, &cur_);
      }

      if (!IsWasmDigit(*cur_)) {
        break;
      }

      u *= 10;
      u += *cur_ - '0';
      if (!u.isValid()) {
        return LexDecFloatLiteral(begin, end_, &cur_);
      }

      cur_++;
    }

    if (*begin == '-') {
      uint64_t value = u.value();
      if (value == 0) {
        return WasmToken(WasmToken::NegativeZero, begin, cur_);
      }
      if (value > uint64_t(INT64_MIN)) {
        return LexDecFloatLiteral(begin, end_, &cur_);
      }

      value = -value;
      return WasmToken(int64_t(value), begin, cur_);
    }
  }

  CheckedInt<uint32_t> index = u.value();
  if (index.isValid()) {
    return WasmToken(index.value(), begin, cur_);
  }

  return WasmToken(u.value(), begin, cur_);
}

void WasmTokenStream::skipSpaces() {
  while (cur_ != end_) {
    char16_t ch = *cur_;
    if (ch == ';' && consume(u";;")) {
      // Skipping single line comment.
      while (cur_ != end_ && !IsWasmNewLine(*cur_)) {
        cur_++;
      }
    } else if (ch == '(' && consume(u"(;")) {
      // Skipping multi-line and possibly nested comments.
      size_t level = 1;
      while (cur_ != end_) {
        char16_t ch = *cur_;
        if (ch == '(' && consume(u"(;")) {
          level++;
        } else if (ch == ';' && consume(u";)")) {
          if (--level == 0) {
            break;
          }
        } else {
          cur_++;
          if (IsWasmNewLine(ch)) {
            lineStart_ = cur_;
            line_++;
          }
        }
      }
    } else if (IsWasmSpace(ch)) {
      cur_++;
      if (IsWasmNewLine(ch)) {
        lineStart_ = cur_;
        line_++;
      }
    } else
      break;  // non-whitespace found
  }
}

WasmToken WasmTokenStream::next() {
  skipSpaces();

  if (cur_ == end_) {
    return WasmToken(WasmToken::EndOfFile, cur_, cur_);
  }

  const char16_t* begin = cur_;
  switch (*begin) {
    case '"':
      cur_++;
      while (true) {
        if (cur_ == end_) {
          return fail(begin);
        }
        if (*cur_ == '"') {
          break;
        }
        if (!ConsumeTextByte(&cur_, end_)) {
          return fail(begin);
        }
      }
      cur_++;
      return WasmToken(WasmToken::Text, begin, cur_);

    case '$':
      cur_++;
      while (cur_ != end_ && IsNameAfterDollar(*cur_)) {
        cur_++;
      }
      return WasmToken(WasmToken::Name, begin, cur_);

    case '(':
      cur_++;
      return WasmToken(WasmToken::OpenParen, begin, cur_);

    case ')':
      cur_++;
      return WasmToken(WasmToken::CloseParen, begin, cur_);

    case '=':
      cur_++;
      return WasmToken(WasmToken::Equal, begin, cur_);

    case '+':
    case '-':
      cur_++;
      if (consume(u"infinity")) {
        return WasmToken(WasmToken::Infinity, begin, cur_);
      }
      if (consume(u"nan")) {
        return nan(begin);
      }
      if (!IsWasmDigit(*cur_)) {
        break;
      }
      MOZ_FALLTHROUGH;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return literal(begin);

    case 'a':
      if (consume(u"align")) {
        return WasmToken(WasmToken::Align, begin, cur_);
      }
      if (consume(u"anyfunc")) {
        return WasmToken(WasmToken::AnyFunc, begin, cur_);
      }
      if (consume(u"anyref")) {
        return WasmToken(WasmToken::ValueType, ValType::AnyRef, begin, cur_);
      }
      if (consume(u"atomic.")) {
        if (consume(u"wake") || consume(u"notify")) {
          return WasmToken(WasmToken::Wake, ThreadOp::Wake, begin, cur_);
        }
        break;
      }
      break;

    case 'b':
      if (consume(u"block")) {
        return WasmToken(WasmToken::Block, begin, cur_);
      }
      if (consume(u"br")) {
        if (consume(u"_table")) {
          return WasmToken(WasmToken::BrTable, begin, cur_);
        }
        if (consume(u"_if")) {
          return WasmToken(WasmToken::BrIf, begin, cur_);
        }
        return WasmToken(WasmToken::Br, begin, cur_);
      }
      break;

    case 'c':
      if (consume(u"call")) {
        if (consume(u"_indirect")) {
          return WasmToken(WasmToken::CallIndirect, begin, cur_);
        }
        return WasmToken(WasmToken::Call, begin, cur_);
      }
      if (consume(u"current_memory")) {
        return WasmToken(WasmToken::CurrentMemory, begin, cur_);
      }
      break;

    case 'd':
      if (consume(u"data")) {
        return WasmToken(WasmToken::Data, begin, cur_);
      }
      if (consume(u"drop")) {
        return WasmToken(WasmToken::Drop, begin, cur_);
      }
      break;

    case 'e':
      if (consume(u"elem")) {
        return WasmToken(WasmToken::Elem, begin, cur_);
      }
      if (consume(u"else")) {
        return WasmToken(WasmToken::Else, begin, cur_);
      }
      if (consume(u"end")) {
        return WasmToken(WasmToken::End, begin, cur_);
      }
      if (consume(u"export")) {
        return WasmToken(WasmToken::Export, begin, cur_);
      }
      break;

    case 'f':
      if (consume(u"field")) {
        return WasmToken(WasmToken::Field, begin, cur_);
      }

      if (consume(u"func")) {
        return WasmToken(WasmToken::Func, begin, cur_);
      }

      if (consume(u"f32")) {
        if (!consume(u".")) {
          return WasmToken(WasmToken::ValueType, ValType::F32, begin, cur_);
        }

        switch (*cur_) {
          case 'a':
            if (consume(u"abs")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Abs, begin, cur_);
            }
            if (consume(u"add")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Add, begin,
                               cur_);
            }
            break;
          case 'c':
            if (consume(u"ceil")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Ceil, begin,
                               cur_);
            }
            if (consume(u"const")) {
              return WasmToken(WasmToken::Const, ValType::F32, begin, cur_);
            }
            if (consume(u"convert_s/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F32ConvertSI32,
                               begin, cur_);
            }
            if (consume(u"convert_u/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F32ConvertUI32,
                               begin, cur_);
            }
            if (consume(u"convert_s/i64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F32ConvertSI64,
                               begin, cur_);
            }
            if (consume(u"convert_u/i64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F32ConvertUI64,
                               begin, cur_);
            }
            if (consume(u"copysign")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32CopySign, begin,
                               cur_);
            }
            break;
          case 'd':
            if (consume(u"demote/f64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F32DemoteF64,
                               begin, cur_);
            }
            if (consume(u"div")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Div, begin,
                               cur_);
            }
            break;
          case 'e':
            if (consume(u"eq")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Eq, begin,
                               cur_);
            }
            break;
          case 'f':
            if (consume(u"floor")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Floor, begin,
                               cur_);
            }
            break;
          case 'g':
            if (consume(u"ge")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Ge, begin,
                               cur_);
            }
            if (consume(u"gt")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Gt, begin,
                               cur_);
            }
            break;
          case 'l':
            if (consume(u"le")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Le, begin,
                               cur_);
            }
            if (consume(u"lt")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Lt, begin,
                               cur_);
            }
            if (consume(u"load")) {
              return WasmToken(WasmToken::Load, Op::F32Load, begin, cur_);
            }
            break;
          case 'm':
            if (consume(u"max")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Max, begin,
                               cur_);
            }
            if (consume(u"min")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Min, begin,
                               cur_);
            }
            if (consume(u"mul")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Mul, begin,
                               cur_);
            }
            break;
          case 'n':
            if (consume(u"nearest")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Nearest, begin,
                               cur_);
            }
            if (consume(u"neg")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Neg, begin, cur_);
            }
            if (consume(u"ne")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F32Ne, begin,
                               cur_);
            }
            break;
          case 'r':
            if (consume(u"reinterpret/i32")) {
              return WasmToken(WasmToken::ConversionOpcode,
                               Op::F32ReinterpretI32, begin, cur_);
            }
            break;
          case 's':
            if (consume(u"sqrt")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Sqrt, begin,
                               cur_);
            }
            if (consume(u"sub")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F32Sub, begin,
                               cur_);
            }
            if (consume(u"store")) {
              return WasmToken(WasmToken::Store, Op::F32Store, begin, cur_);
            }
            break;
          case 't':
            if (consume(u"trunc")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F32Trunc, begin,
                               cur_);
            }
            break;
        }
        break;
      }
      if (consume(u"f64")) {
        if (!consume(u".")) {
          return WasmToken(WasmToken::ValueType, ValType::F64, begin, cur_);
        }

        switch (*cur_) {
          case 'a':
            if (consume(u"abs")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Abs, begin, cur_);
            }
            if (consume(u"add")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Add, begin,
                               cur_);
            }
            break;
          case 'c':
            if (consume(u"ceil")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Ceil, begin,
                               cur_);
            }
            if (consume(u"const")) {
              return WasmToken(WasmToken::Const, ValType::F64, begin, cur_);
            }
            if (consume(u"convert_s/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F64ConvertSI32,
                               begin, cur_);
            }
            if (consume(u"convert_u/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F64ConvertUI32,
                               begin, cur_);
            }
            if (consume(u"convert_s/i64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F64ConvertSI64,
                               begin, cur_);
            }
            if (consume(u"convert_u/i64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F64ConvertUI64,
                               begin, cur_);
            }
            if (consume(u"copysign")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64CopySign, begin,
                               cur_);
            }
            break;
          case 'd':
            if (consume(u"div")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Div, begin,
                               cur_);
            }
            break;
          case 'e':
            if (consume(u"eq")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Eq, begin,
                               cur_);
            }
            break;
          case 'f':
            if (consume(u"floor")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Floor, begin,
                               cur_);
            }
            break;
          case 'g':
            if (consume(u"ge")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Ge, begin,
                               cur_);
            }
            if (consume(u"gt")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Gt, begin,
                               cur_);
            }
            break;
          case 'l':
            if (consume(u"le")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Le, begin,
                               cur_);
            }
            if (consume(u"lt")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Lt, begin,
                               cur_);
            }
            if (consume(u"load")) {
              return WasmToken(WasmToken::Load, Op::F64Load, begin, cur_);
            }
            break;
          case 'm':
            if (consume(u"max")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Max, begin,
                               cur_);
            }
            if (consume(u"min")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Min, begin,
                               cur_);
            }
            if (consume(u"mul")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Mul, begin,
                               cur_);
            }
            break;
          case 'n':
            if (consume(u"nearest")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Nearest, begin,
                               cur_);
            }
            if (consume(u"neg")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Neg, begin, cur_);
            }
            if (consume(u"ne")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::F64Ne, begin,
                               cur_);
            }
            break;
          case 'p':
            if (consume(u"promote/f32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::F64PromoteF32,
                               begin, cur_);
            }
            break;
          case 'r':
            if (consume(u"reinterpret/i64")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64ReinterpretI64,
                               begin, cur_);
            }
            break;
          case 's':
            if (consume(u"sqrt")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Sqrt, begin,
                               cur_);
            }
            if (consume(u"sub")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::F64Sub, begin,
                               cur_);
            }
            if (consume(u"store")) {
              return WasmToken(WasmToken::Store, Op::F64Store, begin, cur_);
            }
            break;
          case 't':
            if (consume(u"trunc")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::F64Trunc, begin,
                               cur_);
            }
            break;
        }
        break;
      }
      break;

    case 'g':
#ifdef ENABLE_WASM_GC
      if (consume(u"gc_feature_opt_in")) {
        return WasmToken(WasmToken::GcFeatureOptIn, begin, cur_);
      }
#endif
      if (consume(u"get_global")) {
        return WasmToken(WasmToken::GetGlobal, begin, cur_);
      }
      if (consume(u"get_local")) {
        return WasmToken(WasmToken::GetLocal, begin, cur_);
      }
      if (consume(u"global")) {
        return WasmToken(WasmToken::Global, begin, cur_);
      }
      if (consume(u"grow_memory")) {
        return WasmToken(WasmToken::GrowMemory, begin, cur_);
      }
      break;

    case 'i':
      if (consume(u"i32")) {
        if (!consume(u".")) {
          return WasmToken(WasmToken::ValueType, ValType::I32, begin, cur_);
        }

        switch (*cur_) {
          case 'a':
            if (consume(u"add")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Add, begin,
                               cur_);
            }
            if (consume(u"and")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32And, begin,
                               cur_);
            }
            if (consume(u"atomic.")) {
              if (consume(u"rmw8_u.add")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicAdd8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.add")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicAdd16U, begin, cur_);
              }
              if (consume(u"rmw.add")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicAdd,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.and")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicAnd8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.and")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicAnd16U, begin, cur_);
              }
              if (consume(u"rmw.and")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicAnd,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I32AtomicCmpXchg8U, begin, cur_);
              }
              if (consume(u"rmw16_u.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I32AtomicCmpXchg16U, begin, cur_);
              }
              if (consume(u"rmw.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I32AtomicCmpXchg, begin, cur_);
              }
              if (consume(u"load8_u")) {
                return WasmToken(WasmToken::AtomicLoad,
                                 ThreadOp::I32AtomicLoad8U, begin, cur_);
              }
              if (consume(u"load16_u")) {
                return WasmToken(WasmToken::AtomicLoad,
                                 ThreadOp::I32AtomicLoad16U, begin, cur_);
              }
              if (consume(u"load")) {
                return WasmToken(WasmToken::AtomicLoad, ThreadOp::I32AtomicLoad,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicOr8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicOr16U,
                                 begin, cur_);
              }
              if (consume(u"rmw.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicOr,
                                 begin, cur_);
              }
              if (consume(u"store8_u")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I32AtomicStore8U, begin, cur_);
              }
              if (consume(u"store16_u")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I32AtomicStore16U, begin, cur_);
              }
              if (consume(u"store")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I32AtomicStore, begin, cur_);
              }
              if (consume(u"rmw8_u.sub")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicSub8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.sub")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicSub16U, begin, cur_);
              }
              if (consume(u"rmw.sub")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicSub,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.xor")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicXor8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.xor")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicXor16U, begin, cur_);
              }
              if (consume(u"rmw.xor")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicXor,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.xchg")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicXchg8U, begin, cur_);
              }
              if (consume(u"rmw16_u.xchg")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I32AtomicXchg16U, begin, cur_);
              }
              if (consume(u"rmw.xchg")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I32AtomicXchg,
                                 begin, cur_);
              }
              if (consume(u"wait")) {
                return WasmToken(WasmToken::Wait, ThreadOp::I32Wait, begin,
                                 cur_);
              }
            }
            break;
          case 'c':
            if (consume(u"const")) {
              return WasmToken(WasmToken::Const, ValType::I32, begin, cur_);
            }
            if (consume(u"clz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I32Clz, begin, cur_);
            }
            if (consume(u"ctz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I32Ctz, begin, cur_);
            }
            break;
          case 'd':
            if (consume(u"div_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32DivS, begin,
                               cur_);
            }
            if (consume(u"div_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32DivU, begin,
                               cur_);
            }
            break;
          case 'e':
            if (consume(u"eqz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I32Eqz, begin, cur_);
            }
            if (consume(u"eq")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32Eq, begin,
                               cur_);
            }
            if (consume(u"extend8_s")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32Extend8S,
                               begin, cur_);
            }
            if (consume(u"extend16_s")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32Extend16S,
                               begin, cur_);
            }
            break;
          case 'g':
            if (consume(u"ge_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32GeS, begin,
                               cur_);
            }
            if (consume(u"ge_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32GeU, begin,
                               cur_);
            }
            if (consume(u"gt_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32GtS, begin,
                               cur_);
            }
            if (consume(u"gt_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32GtU, begin,
                               cur_);
            }
            break;
          case 'l':
            if (consume(u"le_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32LeS, begin,
                               cur_);
            }
            if (consume(u"le_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32LeU, begin,
                               cur_);
            }
            if (consume(u"lt_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32LtS, begin,
                               cur_);
            }
            if (consume(u"lt_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32LtU, begin,
                               cur_);
            }
            if (consume(u"load")) {
              if (IsWasmSpace(*cur_)) {
                return WasmToken(WasmToken::Load, Op::I32Load, begin, cur_);
              }
              if (consume(u"8_s")) {
                return WasmToken(WasmToken::Load, Op::I32Load8S, begin, cur_);
              }
              if (consume(u"8_u")) {
                return WasmToken(WasmToken::Load, Op::I32Load8U, begin, cur_);
              }
              if (consume(u"16_s")) {
                return WasmToken(WasmToken::Load, Op::I32Load16S, begin, cur_);
              }
              if (consume(u"16_u")) {
                return WasmToken(WasmToken::Load, Op::I32Load16U, begin, cur_);
              }
              break;
            }
            break;
          case 'm':
            if (consume(u"mul")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Mul, begin,
                               cur_);
            }
            break;
          case 'n':
            if (consume(u"ne")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I32Ne, begin,
                               cur_);
            }
            break;
          case 'o':
            if (consume(u"or")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Or, begin, cur_);
            }
            break;
          case 'p':
            if (consume(u"popcnt")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I32Popcnt, begin,
                               cur_);
            }
            break;
          case 'r':
            if (consume(u"reinterpret/f32")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I32ReinterpretF32,
                               begin, cur_);
            }
            if (consume(u"rem_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32RemS, begin,
                               cur_);
            }
            if (consume(u"rem_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32RemU, begin,
                               cur_);
            }
            if (consume(u"rotr")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Rotr, begin,
                               cur_);
            }
            if (consume(u"rotl")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Rotl, begin,
                               cur_);
            }
            break;
          case 's':
            if (consume(u"sub")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Sub, begin,
                               cur_);
            }
            if (consume(u"shl")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Shl, begin,
                               cur_);
            }
            if (consume(u"shr_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32ShrS, begin,
                               cur_);
            }
            if (consume(u"shr_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32ShrU, begin,
                               cur_);
            }
            if (consume(u"store")) {
              if (IsWasmSpace(*cur_)) {
                return WasmToken(WasmToken::Store, Op::I32Store, begin, cur_);
              }
              if (consume(u"8")) {
                return WasmToken(WasmToken::Store, Op::I32Store8, begin, cur_);
              }
              if (consume(u"16")) {
                return WasmToken(WasmToken::Store, Op::I32Store16, begin, cur_);
              }
              break;
            }
            break;
          case 't':
            if (consume(u"trunc_s/f32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32TruncSF32,
                               begin, cur_);
            }
            if (consume(u"trunc_s/f64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32TruncSF64,
                               begin, cur_);
            }
            if (consume(u"trunc_u/f32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32TruncUF32,
                               begin, cur_);
            }
            if (consume(u"trunc_u/f64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32TruncUF64,
                               begin, cur_);
            }
            if (consume(u"trunc_s:sat/f32")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I32TruncSSatF32, begin, cur_);
            }
            if (consume(u"trunc_s:sat/f64")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I32TruncSSatF64, begin, cur_);
            }
            if (consume(u"trunc_u:sat/f32")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I32TruncUSatF32, begin, cur_);
            }
            if (consume(u"trunc_u:sat/f64")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I32TruncUSatF64, begin, cur_);
            }
            break;
          case 'w':
            if (consume(u"wrap/i64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I32WrapI64,
                               begin, cur_);
            }
            break;
          case 'x':
            if (consume(u"xor")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I32Xor, begin,
                               cur_);
            }
            break;
        }
        break;
      }
      if (consume(u"i64")) {
        if (!consume(u".")) {
          return WasmToken(WasmToken::ValueType, ValType::I64, begin, cur_);
        }

        switch (*cur_) {
          case 'a':
            if (consume(u"add")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Add, begin,
                               cur_);
            }
            if (consume(u"and")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64And, begin,
                               cur_);
            }
            if (consume(u"atomic.")) {
              if (consume(u"rmw8_u.add")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicAdd8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.add")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicAdd16U, begin, cur_);
              }
              if (consume(u"rmw32_u.add")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicAdd32U, begin, cur_);
              }
              if (consume(u"rmw.add")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicAdd,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.and")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicAnd8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.and")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicAnd16U, begin, cur_);
              }
              if (consume(u"rmw32_u.and")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicAnd32U, begin, cur_);
              }
              if (consume(u"rmw.and")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicAnd,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I64AtomicCmpXchg8U, begin, cur_);
              }
              if (consume(u"rmw16_u.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I64AtomicCmpXchg16U, begin, cur_);
              }
              if (consume(u"rmw32_u.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I64AtomicCmpXchg32U, begin, cur_);
              }
              if (consume(u"rmw.cmpxchg")) {
                return WasmToken(WasmToken::AtomicCmpXchg,
                                 ThreadOp::I64AtomicCmpXchg, begin, cur_);
              }
              if (consume(u"load8_u")) {
                return WasmToken(WasmToken::AtomicLoad,
                                 ThreadOp::I64AtomicLoad8U, begin, cur_);
              }
              if (consume(u"load16_u")) {
                return WasmToken(WasmToken::AtomicLoad,
                                 ThreadOp::I64AtomicLoad16U, begin, cur_);
              }
              if (consume(u"load32_u")) {
                return WasmToken(WasmToken::AtomicLoad,
                                 ThreadOp::I64AtomicLoad32U, begin, cur_);
              }
              if (consume(u"load")) {
                return WasmToken(WasmToken::AtomicLoad, ThreadOp::I64AtomicLoad,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicOr8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicOr16U,
                                 begin, cur_);
              }
              if (consume(u"rmw32_u.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicOr32U,
                                 begin, cur_);
              }
              if (consume(u"rmw.or")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicOr,
                                 begin, cur_);
              }
              if (consume(u"store8_u")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I64AtomicStore8U, begin, cur_);
              }
              if (consume(u"store16_u")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I64AtomicStore16U, begin, cur_);
              }
              if (consume(u"store32_u")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I64AtomicStore32U, begin, cur_);
              }
              if (consume(u"store")) {
                return WasmToken(WasmToken::AtomicStore,
                                 ThreadOp::I64AtomicStore, begin, cur_);
              }
              if (consume(u"rmw8_u.sub")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicSub8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.sub")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicSub16U, begin, cur_);
              }
              if (consume(u"rmw32_u.sub")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicSub32U, begin, cur_);
              }
              if (consume(u"rmw.sub")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicSub,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.xor")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicXor8U,
                                 begin, cur_);
              }
              if (consume(u"rmw16_u.xor")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicXor16U, begin, cur_);
              }
              if (consume(u"rmw32_u.xor")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicXor32U, begin, cur_);
              }
              if (consume(u"rmw.xor")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicXor,
                                 begin, cur_);
              }
              if (consume(u"rmw8_u.xchg")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicXchg8U, begin, cur_);
              }
              if (consume(u"rmw16_u.xchg")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicXchg16U, begin, cur_);
              }
              if (consume(u"rmw32_u.xchg")) {
                return WasmToken(WasmToken::AtomicRMW,
                                 ThreadOp::I64AtomicXchg32U, begin, cur_);
              }
              if (consume(u"rmw.xchg")) {
                return WasmToken(WasmToken::AtomicRMW, ThreadOp::I64AtomicXchg,
                                 begin, cur_);
              }
              if (consume(u"wait")) {
                return WasmToken(WasmToken::Wait, ThreadOp::I64Wait, begin,
                                 cur_);
              }
            }
            break;
          case 'c':
            if (consume(u"const")) {
              return WasmToken(WasmToken::Const, ValType::I64, begin, cur_);
            }
            if (consume(u"clz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I64Clz, begin, cur_);
            }
            if (consume(u"ctz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I64Ctz, begin, cur_);
            }
            break;
          case 'd':
            if (consume(u"div_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64DivS, begin,
                               cur_);
            }
            if (consume(u"div_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64DivU, begin,
                               cur_);
            }
            break;
          case 'e':
            if (consume(u"eqz")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I64Eqz, begin, cur_);
            }
            if (consume(u"eq")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64Eq, begin,
                               cur_);
            }
            if (consume(u"extend_s/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64ExtendSI32,
                               begin, cur_);
            }
            if (consume(u"extend_u/i32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64ExtendUI32,
                               begin, cur_);
            }
            if (consume(u"extend8_s")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64Extend8S,
                               begin, cur_);
            }
            if (consume(u"extend16_s")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64Extend16S,
                               begin, cur_);
            }
            if (consume(u"extend32_s")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64Extend32S,
                               begin, cur_);
            }
            break;
          case 'g':
            if (consume(u"ge_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64GeS, begin,
                               cur_);
            }
            if (consume(u"ge_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64GeU, begin,
                               cur_);
            }
            if (consume(u"gt_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64GtS, begin,
                               cur_);
            }
            if (consume(u"gt_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64GtU, begin,
                               cur_);
            }
            break;
          case 'l':
            if (consume(u"le_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64LeS, begin,
                               cur_);
            }
            if (consume(u"le_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64LeU, begin,
                               cur_);
            }
            if (consume(u"lt_s")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64LtS, begin,
                               cur_);
            }
            if (consume(u"lt_u")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64LtU, begin,
                               cur_);
            }
            if (consume(u"load")) {
              if (IsWasmSpace(*cur_)) {
                return WasmToken(WasmToken::Load, Op::I64Load, begin, cur_);
              }
              if (consume(u"8_s")) {
                return WasmToken(WasmToken::Load, Op::I64Load8S, begin, cur_);
              }
              if (consume(u"8_u")) {
                return WasmToken(WasmToken::Load, Op::I64Load8U, begin, cur_);
              }
              if (consume(u"16_s")) {
                return WasmToken(WasmToken::Load, Op::I64Load16S, begin, cur_);
              }
              if (consume(u"16_u")) {
                return WasmToken(WasmToken::Load, Op::I64Load16U, begin, cur_);
              }
              if (consume(u"32_s")) {
                return WasmToken(WasmToken::Load, Op::I64Load32S, begin, cur_);
              }
              if (consume(u"32_u")) {
                return WasmToken(WasmToken::Load, Op::I64Load32U, begin, cur_);
              }
              break;
            }
            break;
          case 'm':
            if (consume(u"mul")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Mul, begin,
                               cur_);
            }
            break;
          case 'n':
            if (consume(u"ne")) {
              return WasmToken(WasmToken::ComparisonOpcode, Op::I64Ne, begin,
                               cur_);
            }
            break;
          case 'o':
            if (consume(u"or")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Or, begin, cur_);
            }
            break;
          case 'p':
            if (consume(u"popcnt")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I64Popcnt, begin,
                               cur_);
            }
            break;
          case 'r':
            if (consume(u"reinterpret/f64")) {
              return WasmToken(WasmToken::UnaryOpcode, Op::I64ReinterpretF64,
                               begin, cur_);
            }
            if (consume(u"rem_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64RemS, begin,
                               cur_);
            }
            if (consume(u"rem_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64RemU, begin,
                               cur_);
            }
            if (consume(u"rotr")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Rotr, begin,
                               cur_);
            }
            if (consume(u"rotl")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Rotl, begin,
                               cur_);
            }
            break;
          case 's':
            if (consume(u"sub")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Sub, begin,
                               cur_);
            }
            if (consume(u"shl")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Shl, begin,
                               cur_);
            }
            if (consume(u"shr_s")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64ShrS, begin,
                               cur_);
            }
            if (consume(u"shr_u")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64ShrU, begin,
                               cur_);
            }
            if (consume(u"store")) {
              if (IsWasmSpace(*cur_)) {
                return WasmToken(WasmToken::Store, Op::I64Store, begin, cur_);
              }
              if (consume(u"8")) {
                return WasmToken(WasmToken::Store, Op::I64Store8, begin, cur_);
              }
              if (consume(u"16")) {
                return WasmToken(WasmToken::Store, Op::I64Store16, begin, cur_);
              }
              if (consume(u"32")) {
                return WasmToken(WasmToken::Store, Op::I64Store32, begin, cur_);
              }
              break;
            }
            break;
          case 't':
            if (consume(u"trunc_s/f32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64TruncSF32,
                               begin, cur_);
            }
            if (consume(u"trunc_s/f64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64TruncSF64,
                               begin, cur_);
            }
            if (consume(u"trunc_u/f32")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64TruncUF32,
                               begin, cur_);
            }
            if (consume(u"trunc_u/f64")) {
              return WasmToken(WasmToken::ConversionOpcode, Op::I64TruncUF64,
                               begin, cur_);
            }
            if (consume(u"trunc_s:sat/f32")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I64TruncSSatF32, begin, cur_);
            }
            if (consume(u"trunc_s:sat/f64")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I64TruncSSatF64, begin, cur_);
            }
            if (consume(u"trunc_u:sat/f32")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I64TruncUSatF32, begin, cur_);
            }
            if (consume(u"trunc_u:sat/f64")) {
              return WasmToken(WasmToken::ExtraConversionOpcode,
                               MiscOp::I64TruncUSatF64, begin, cur_);
            }
            break;
          case 'w':
            break;
          case 'x':
            if (consume(u"xor")) {
              return WasmToken(WasmToken::BinaryOpcode, Op::I64Xor, begin,
                               cur_);
            }
            break;
        }
        break;
      }
      if (consume(u"import")) {
        return WasmToken(WasmToken::Import, begin, cur_);
      }
      if (consume(u"infinity")) {
        return WasmToken(WasmToken::Infinity, begin, cur_);
      }
      if (consume(u"if")) {
        return WasmToken(WasmToken::If, begin, cur_);
      }
      break;

    case 'l':
      if (consume(u"local")) {
        return WasmToken(WasmToken::Local, begin, cur_);
      }
      if (consume(u"loop")) {
        return WasmToken(WasmToken::Loop, begin, cur_);
      }
      break;

    case 'm':
      if (consume(u"memory.")) {
#ifdef ENABLE_WASM_BULKMEM_OPS
        if (consume(u"copy")) {
          return WasmToken(WasmToken::MemCopy, begin, cur_);
        }
        if (consume(u"drop")) {
          return WasmToken(WasmToken::MemDrop, begin, cur_);
        }
        if (consume(u"fill")) {
          return WasmToken(WasmToken::MemFill, begin, cur_);
        }
        if (consume(u"init")) {
          return WasmToken(WasmToken::MemInit, begin, cur_);
        }
#endif
        if (consume(u"grow")) {
          return WasmToken(WasmToken::GrowMemory, begin, cur_);
        }
        if (consume(u"size")) {
          return WasmToken(WasmToken::CurrentMemory, begin, cur_);
        }
        break;
      }
      if (consume(u"module")) {
        return WasmToken(WasmToken::Module, begin, cur_);
      }
      if (consume(u"memory")) {
        return WasmToken(WasmToken::Memory, begin, cur_);
      }
      if (consume(u"mut")) {
        return WasmToken(WasmToken::Mutable, begin, cur_);
      }
      break;

    case 'n':
      if (consume(u"nan")) {
        return nan(begin);
      }
      if (consume(u"nop")) {
        return WasmToken(WasmToken::Nop, begin, cur_);
      }
      break;

    case 'o':
      if (consume(u"offset")) {
        return WasmToken(WasmToken::Offset, begin, cur_);
      }
      break;

    case 'p':
      if (consume(u"param")) {
        return WasmToken(WasmToken::Param, begin, cur_);
      }
#ifdef ENABLE_WASM_BULKMEM_OPS
      if (consume(u"passive")) {
        return WasmToken(WasmToken::Passive, begin, cur_);
      }
#endif
      break;

    case 'r':
      if (consume(u"result")) {
        return WasmToken(WasmToken::Result, begin, cur_);
      }
      if (consume(u"return")) {
        return WasmToken(WasmToken::Return, begin, cur_);
      }
      if (consume(u"ref")) {
        if (consume(u".eq")) {
          return WasmToken(WasmToken::ComparisonOpcode, Op::RefEq, begin, cur_);
        }
        if (consume(u".null")) {
          return WasmToken(WasmToken::RefNull, begin, cur_);
        }
        if (consume(u".is_null")) {
          return WasmToken(WasmToken::UnaryOpcode, Op::RefIsNull, begin, cur_);
        }
        return WasmToken(WasmToken::Ref, begin, cur_);
      }
      break;

    case 's':
      if (consume(u"select")) {
        return WasmToken(WasmToken::TernaryOpcode, Op::Select, begin, cur_);
      }
      if (consume(u"set_global")) {
        return WasmToken(WasmToken::SetGlobal, begin, cur_);
      }
      if (consume(u"set_local")) {
        return WasmToken(WasmToken::SetLocal, begin, cur_);
      }
      if (consume(u"shared")) {
        return WasmToken(WasmToken::Shared, begin, cur_);
      }
      if (consume(u"start")) {
        return WasmToken(WasmToken::Start, begin, cur_);
      }
      if (consume(u"struct")) {
#ifdef ENABLE_WASM_GC
        if (consume(u".new")) {
          return WasmToken(WasmToken::StructNew, begin, cur_);
        }
        if (consume(u".get")) {
          return WasmToken(WasmToken::StructGet, begin, cur_);
        }
        if (consume(u".set")) {
          return WasmToken(WasmToken::StructSet, begin, cur_);
        }
        if (consume(u".narrow")) {
          return WasmToken(WasmToken::StructNarrow, begin, cur_);
        }
#endif
        return WasmToken(WasmToken::Struct, begin, cur_);
      }
      break;

    case 't':
      if (consume(u"table.")) {
#ifdef ENABLE_WASM_BULKMEM_OPS
        if (consume(u"copy")) {
          return WasmToken(WasmToken::TableCopy, begin, cur_);
        }
        if (consume(u"drop")) {
          return WasmToken(WasmToken::TableDrop, begin, cur_);
        }
        if (consume(u"init")) {
          return WasmToken(WasmToken::TableInit, begin, cur_);
        }
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
        if (consume(u"get")) {
          return WasmToken(WasmToken::TableGet, begin, cur_);
        }
        if (consume(u"grow")) {
          return WasmToken(WasmToken::TableGrow, begin, cur_);
        }
        if (consume(u"set")) {
          return WasmToken(WasmToken::TableSet, begin, cur_);
        }
        if (consume(u"size")) {
          return WasmToken(WasmToken::TableSize, begin, cur_);
        }
#endif
        break;
      }
      if (consume(u"table")) {
        return WasmToken(WasmToken::Table, begin, cur_);
      }
      if (consume(u"tee_local")) {
        return WasmToken(WasmToken::TeeLocal, begin, cur_);
      }
      if (consume(u"then")) {
        return WasmToken(WasmToken::Then, begin, cur_);
      }
      if (consume(u"type")) {
        return WasmToken(WasmToken::Type, begin, cur_);
      }
      break;

    case 'u':
      if (consume(u"unreachable")) {
        return WasmToken(WasmToken::Unreachable, begin, cur_);
      }
      break;

    default:
      break;
  }

  return fail(begin);
}

/*****************************************************************************/
// wasm text format parser

namespace {

struct WasmParseContext {
  WasmTokenStream ts;
  LifoAlloc& lifo;
  UniqueChars* error;
  DtoaState* dtoaState;
  uintptr_t stackLimit;
  uint32_t nextSym;

  WasmParseContext(const char16_t* text, uintptr_t stackLimit, LifoAlloc& lifo,
                   UniqueChars* error)
      : ts(text),
        lifo(lifo),
        error(error),
        dtoaState(NewDtoaState()),
        stackLimit(stackLimit),
        nextSym(0) {}

  ~WasmParseContext() { DestroyDtoaState(dtoaState); }

  AstName gensym(const char* tag) {
    char buf[128];
    MOZ_ASSERT(strlen(tag) < sizeof(buf) - 20);
    SprintfLiteral(buf, ".%s.%u", tag, nextSym);
    nextSym++;
    size_t k = strlen(buf) + 1;
    char16_t* mem = (char16_t*)lifo.alloc(k * sizeof(char16_t));
    if (!mem) {
      return AstName();
    }
    for (size_t i = 0; i < k; i++) {
      mem[i] = buf[i];
    }
    return AstName(mem, k - 1);
  }
};

}  // end anonymous namespace

static AstExpr* ParseExprInsideParens(WasmParseContext& c);

static AstExpr* ParseExprBody(WasmParseContext& c, WasmToken token,
                              bool inParens);

static AstExpr* ParseExpr(WasmParseContext& c, bool inParens) {
  WasmToken openParen;
  if (!inParens || !c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    return new (c.lifo) AstPop();
  }

  // Special case: If we have an open paren, but it's a "(then ...", then
  // we don't have an expresion following us, so we pop here too. This
  // handles "(if (then ...))" which pops the condition.
  if (c.ts.peek().kind() == WasmToken::Then) {
    c.ts.unget(openParen);
    return new (c.lifo) AstPop();
  }

  AstExpr* expr = ParseExprInsideParens(c);
  if (!expr) {
    return nullptr;
  }

  if (!c.ts.match(WasmToken::CloseParen, c.error)) {
    return nullptr;
  }

  return expr;
}

static bool ParseExprList(WasmParseContext& c, AstExprVector* exprs) {
  for (;;) {
    if (c.ts.getIf(WasmToken::OpenParen)) {
      AstExpr* expr = ParseExprInsideParens(c);
      if (!expr || !exprs->append(expr)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
      continue;
    }

    WasmToken token;
    if (c.ts.getIfOpcode(&token)) {
      AstExpr* expr = ParseExprBody(c, token, false);
      if (!expr || !exprs->append(expr)) {
        return false;
      }
      continue;
    }

    break;
  }

  return true;
}

static bool MaybeParseValType(WasmParseContext& c, AstValType* type) {
  WasmToken token;

  if (c.ts.getIf(WasmToken::ValueType, &token)) {
    *type = AstValType(token.valueType());
  } else if (c.ts.getIf(WasmToken::OpenParen, &token)) {
    if (c.ts.getIf(WasmToken::Ref)) {
      AstRef target;
      if (!c.ts.matchRef(&target, c.error) ||
          !c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
      *type = AstValType(target);
    } else {
      c.ts.unget(token);
    }
  }
  return true;
}

static bool ParseValType(WasmParseContext& c, AstValType* type) {
  if (!MaybeParseValType(c, type)) {
    return false;
  }

  if (!type->isValid()) {
    c.ts.generateError(c.ts.peek(), "expected value type", c.error);
    return false;
  }

  return true;
}

static bool ParseBlockSignature(WasmParseContext& c, AstExprType* type) {
  WasmToken token;
  AstValType vt;

  if (!MaybeParseValType(c, &vt)) {
    return false;
  }

  if (vt.isValid()) {
    *type = AstExprType(vt);
  } else {
    *type = AstExprType(ExprType::Void);
  }

  return true;
}

static bool MaybeMatchName(WasmParseContext& c, const AstName& name) {
  WasmToken tok;
  if (c.ts.getIf(WasmToken::Name, &tok)) {
    AstName otherName = tok.name();
    if (otherName.empty()) {
      return true;
    }

    if (name.empty()) {
      c.ts.generateError(tok, "end name without a start name", c.error);
      return false;
    }

    if (otherName != name) {
      c.ts.generateError(tok, "start/end names don't match", c.error);
      return false;
    }
  }
  return true;
}

static AstBlock* ParseBlock(WasmParseContext& c, Op op, bool inParens) {
  AstExprVector exprs(c.lifo);

  AstName name = c.ts.getIfName();

  // Compatibility syntax sugar: If a second label is present, we'll wrap
  // this loop in a block.
  AstName otherName;
  if (op == Op::Loop) {
    AstName maybeName = c.ts.getIfName();
    if (!maybeName.empty()) {
      otherName = name;
      name = maybeName;
    }
  }

  AstExprType type(ExprType::Limit);
  if (!ParseBlockSignature(c, &type)) {
    return nullptr;
  }

  if (!ParseExprList(c, &exprs)) {
    return nullptr;
  }

  if (!inParens) {
    if (!c.ts.match(WasmToken::End, c.error)) {
      return nullptr;
    }
    if (!MaybeMatchName(c, name)) {
      return nullptr;
    }
  }

  AstBlock* result = new (c.lifo) AstBlock(op, type, name, std::move(exprs));
  if (!result) {
    return nullptr;
  }

  if (op == Op::Loop && !otherName.empty()) {
    if (!exprs.append(result)) {
      return nullptr;
    }
    result =
        new (c.lifo) AstBlock(Op::Block, type, otherName, std::move(exprs));
  }

  return result;
}

static AstBranch* ParseBranch(WasmParseContext& c, Op op, bool inParens) {
  MOZ_ASSERT(op == Op::Br || op == Op::BrIf);

  AstRef target;
  if (!c.ts.matchRef(&target, c.error)) {
    return nullptr;
  }

  AstExpr* value = nullptr;
  if (inParens) {
    if (c.ts.getIf(WasmToken::OpenParen)) {
      value = ParseExprInsideParens(c);
      if (!value) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
    }
  }

  AstExpr* cond = nullptr;
  if (op == Op::BrIf) {
    if (inParens && c.ts.getIf(WasmToken::OpenParen)) {
      cond = ParseExprInsideParens(c);
      if (!cond) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
    } else {
      cond = new (c.lifo) AstPop();
      if (!cond) {
        return nullptr;
      }
    }
  }

  return new (c.lifo) AstBranch(op, ExprType::Void, cond, target, value);
}

static bool ParseArgs(WasmParseContext& c, AstExprVector* args) {
  while (c.ts.getIf(WasmToken::OpenParen)) {
    AstExpr* arg = ParseExprInsideParens(c);
    if (!arg || !args->append(arg)) {
      return false;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }
  }

  return true;
}

static AstCall* ParseCall(WasmParseContext& c, bool inParens) {
  AstRef func;
  if (!c.ts.matchRef(&func, c.error)) {
    return nullptr;
  }

  AstExprVector args(c.lifo);
  if (inParens) {
    if (!ParseArgs(c, &args)) {
      return nullptr;
    }
  }

  return new (c.lifo) AstCall(Op::Call, ExprType::Void, func, std::move(args));
}

static AstCallIndirect* ParseCallIndirect(WasmParseContext& c, bool inParens) {
  AstRef firstRef;
  AstRef secondRef;
  AstRef funcType;
  AstRef targetTable = AstRef(0);

  // (call_indirect table signature arg ... index)
  // (call_indirect signature arg ... index)

  if (!c.ts.matchRef(&firstRef, c.error)) {
    return nullptr;
  }
  if (c.ts.getIfRef(&secondRef)) {
    targetTable = firstRef;
    funcType = secondRef;
  } else {
    funcType = firstRef;
  }

  AstExprVector args(c.lifo);
  AstExpr* index;
  if (inParens) {
    if (!ParseArgs(c, &args)) {
      return nullptr;
    }

    if (args.empty()) {
      index = new (c.lifo) AstPop();
    } else {
      index = args.popCopy();
    }
  } else {
    index = new (c.lifo) AstPop();
  }

  if (!index) {
    return nullptr;
  }

  return new (c.lifo) AstCallIndirect(targetTable, funcType, ExprType::Void,
                                      std::move(args), index);
}

static uint_fast8_t CountLeadingZeroes4(uint8_t x) {
  MOZ_ASSERT((x & -0x10) == 0);
  return CountLeadingZeroes32(x) - 28;
}

template <typename T>
static T ushl(T lhs, unsigned rhs) {
  return rhs < sizeof(T) * CHAR_BIT ? (lhs << rhs) : 0;
}

template <typename T>
static T ushr(T lhs, unsigned rhs) {
  return rhs < sizeof(T) * CHAR_BIT ? (lhs >> rhs) : 0;
}

template <typename Float>
static AstConst* ParseNaNLiteral(WasmParseContext& c, WasmToken token,
                                 const char16_t* cur, bool isNegated) {
  const char16_t* end = token.end();

  MOZ_ALWAYS_TRUE(*cur++ == 'n' && *cur++ == 'a' && *cur++ == 'n');

  typedef FloatingPoint<Float> Traits;
  typedef typename Traits::Bits Bits;

  Bits value;
  if (cur != end) {
    MOZ_ALWAYS_TRUE(*cur++ == ':' && *cur++ == '0' && *cur++ == 'x');
    if (cur == end) {
      goto error;
    }
    CheckedInt<Bits> u = 0;
    do {
      uint8_t digit = 0;
      MOZ_ALWAYS_TRUE(IsHexDigit(*cur, &digit));
      u *= 16;
      u += digit;
      cur++;
    } while (cur != end);
    if (!u.isValid()) {
      goto error;
    }
    value = u.value();
    if ((value & ~Traits::kSignificandBits) != 0) {
      goto error;
    }
    // NaN payloads must contain at least one set bit.
    if (value == 0) {
      goto error;
    }
  } else {
    // Produce the spec's default NaN.
    value = (Traits::kSignificandBits + 1) >> 1;
  }

  value = (isNegated ? Traits::kSignBit : 0) | Traits::kExponentBits | value;

  Float flt;
  BitwiseCast(value, &flt);
  return new (c.lifo) AstConst(LitVal(flt));

error:
  c.ts.generateError(token, c.error);
  return nullptr;
}

template <typename Float>
static bool ParseHexFloatLiteral(const char16_t* cur, const char16_t* end,
                                 Float* result) {
  MOZ_ALWAYS_TRUE(*cur++ == '0' && *cur++ == 'x');
  typedef FloatingPoint<Float> Traits;
  typedef typename Traits::Bits Bits;
  static const unsigned numBits = sizeof(Float) * CHAR_BIT;
  static const Bits allOnes = ~Bits(0);
  static const Bits mostSignificantBit = ~(allOnes >> 1);

  // Significand part.
  Bits significand = 0;
  CheckedInt<int32_t> exponent = 0;
  bool sawFirstNonZero = false;
  bool discardedExtraNonZero = false;
  const char16_t* dot = nullptr;
  int significandPos;
  for (; cur != end; cur++) {
    if (*cur == '.') {
      MOZ_ASSERT(!dot);
      dot = cur;
      continue;
    }

    uint8_t digit;
    if (!IsHexDigit(*cur, &digit)) {
      break;
    }
    if (!sawFirstNonZero) {
      if (digit == 0) {
        continue;
      }
      // We've located the first non-zero digit; we can now determine the
      // initial exponent. If we're after the dot, count the number of
      // zeros from the dot to here, and adjust for the number of leading
      // zero bits in the digit. Set up significandPos to put the first
      // nonzero at the most significant bit.
      int_fast8_t lz = CountLeadingZeroes4(digit);
      ptrdiff_t zeroAdjustValue = !dot ? 1 : dot + 1 - cur;
      CheckedInt<ptrdiff_t> zeroAdjust = zeroAdjustValue;
      zeroAdjust *= 4;
      zeroAdjust -= lz + 1;
      if (!zeroAdjust.isValid()) {
        return false;
      }
      exponent = zeroAdjust.value();
      significandPos = numBits - (4 - lz);
      sawFirstNonZero = true;
    } else {
      // We've already seen a non-zero; just take 4 more bits.
      if (!dot) {
        exponent += 4;
      }
      if (significandPos > -4) {
        significandPos -= 4;
      }
    }

    // Or the newly parsed digit into significand at signicandPos.
    if (significandPos >= 0) {
      significand |= ushl(Bits(digit), significandPos);
    } else if (significandPos > -4) {
      significand |= ushr(digit, 4 - significandPos);
      discardedExtraNonZero = (digit & ~ushl(allOnes, 4 - significandPos)) != 0;
    } else if (digit != 0) {
      discardedExtraNonZero = true;
    }
  }

  // Exponent part.
  if (cur != end) {
    MOZ_ALWAYS_TRUE(*cur++ == 'p');
    bool isNegated = false;
    if (cur != end && (*cur == '-' || *cur == '+')) {
      isNegated = *cur++ == '-';
    }
    CheckedInt<int32_t> parsedExponent = 0;
    while (cur != end && IsWasmDigit(*cur)) {
      parsedExponent = parsedExponent * 10 + (*cur++ - '0');
    }
    if (isNegated) {
      parsedExponent = -parsedExponent;
    }
    exponent += parsedExponent;
  }

  MOZ_ASSERT(cur == end);
  if (!exponent.isValid()) {
    return false;
  }

  // Create preliminary exponent and significand encodings of the results.
  Bits encodedExponent, encodedSignificand, discardedSignificandBits;
  if (significand == 0) {
    // Zero. The exponent is encoded non-biased.
    encodedExponent = 0;
    encodedSignificand = 0;
    discardedSignificandBits = 0;
  } else if (MOZ_UNLIKELY(exponent.value() <=
                          int32_t(-Traits::kExponentBias))) {
    // Underflow to subnormal or zero.
    encodedExponent = 0;
    encodedSignificand =
        ushr(significand, numBits - Traits::kExponentShift - exponent.value() -
                              Traits::kExponentBias);
    discardedSignificandBits =
        ushl(significand,
             Traits::kExponentShift + exponent.value() + Traits::kExponentBias);
  } else if (MOZ_LIKELY(exponent.value() <= int32_t(Traits::kExponentBias))) {
    // Normal (non-zero). The significand's leading 1 is encoded implicitly.
    encodedExponent = (Bits(exponent.value()) + Traits::kExponentBias)
                      << Traits::kExponentShift;
    MOZ_ASSERT(significand & mostSignificantBit);
    encodedSignificand =
        ushr(significand, numBits - Traits::kExponentShift - 1) &
        Traits::kSignificandBits;
    discardedSignificandBits = ushl(significand, Traits::kExponentShift + 1);
  } else {
    // Overflow to infinity.
    encodedExponent = Traits::kExponentBits;
    encodedSignificand = 0;
    discardedSignificandBits = 0;
  }
  MOZ_ASSERT((encodedExponent & ~Traits::kExponentBits) == 0);
  MOZ_ASSERT((encodedSignificand & ~Traits::kSignificandBits) == 0);
  MOZ_ASSERT(encodedExponent != Traits::kExponentBits ||
             encodedSignificand == 0);
  Bits bits = encodedExponent | encodedSignificand;

  // Apply rounding. If this overflows the significand, it carries into the
  // exponent bit according to the magic of the IEEE 754 encoding.
  bits += (discardedSignificandBits & mostSignificantBit) &&
          ((discardedSignificandBits & ~mostSignificantBit) ||
           discardedExtraNonZero ||
           // ties to even
           (encodedSignificand & 1));

  *result = BitwiseCast<Float>(bits);
  return true;
}

template <typename Float>
static AstConst* ParseFloatLiteral(WasmParseContext& c, WasmToken token) {
  Float result;
  switch (token.kind()) {
    case WasmToken::Index:
      result = token.index();
      break;
    case WasmToken::UnsignedInteger:
      result = token.uint();
      break;
    case WasmToken::SignedInteger:
      result = token.sint();
      break;
    case WasmToken::NegativeZero:
      result = -0.;
      break;
    case WasmToken::Float:
      break;
    default:
      c.ts.generateError(token, c.error);
      return nullptr;
  }

  if (token.kind() != WasmToken::Float) {
    return new (c.lifo) AstConst(LitVal(Float(result)));
  }

  const char16_t* begin = token.begin();
  const char16_t* end = token.end();
  const char16_t* cur = begin;

  bool isNegated = false;
  if (*cur == '-' || *cur == '+') {
    isNegated = *cur++ == '-';
  }

  switch (token.floatLiteralKind()) {
    case WasmToken::Infinity: {
      result = PositiveInfinity<Float>();
      break;
    }
    case WasmToken::NaN: {
      return ParseNaNLiteral<Float>(c, token, cur, isNegated);
    }
    case WasmToken::HexNumber: {
      if (!ParseHexFloatLiteral(cur, end, &result)) {
        c.ts.generateError(token, c.error);
        return nullptr;
      }
      break;
    }
    case WasmToken::DecNumber: {
      // Call into JS' strtod. Tokenization has already required that the
      // string is well-behaved.
      LifoAlloc::Mark mark = c.lifo.mark();
      char* buffer = c.lifo.newArray<char>(end - cur + 1);
      if (!buffer) {
        return nullptr;
      }
      for (ptrdiff_t i = 0; i < end - cur; ++i) {
        buffer[i] = char(cur[i]);
      }
      buffer[end - cur] = '\0';
      char* strtod_end;
      result = (Float)js_strtod_harder(c.dtoaState, buffer, &strtod_end);
      if (strtod_end == buffer) {
        c.lifo.release(mark);
        c.ts.generateError(token, c.error);
        return nullptr;
      }
      c.lifo.release(mark);
      break;
    }
  }

  if (isNegated) {
    result = -result;
  }

  return new (c.lifo) AstConst(LitVal(Float(result)));
}

static AstConst* ParseConst(WasmParseContext& c, WasmToken constToken) {
  WasmToken val = c.ts.get();
  switch (constToken.valueType().code()) {
    case ValType::I32: {
      switch (val.kind()) {
        case WasmToken::Index:
          return new (c.lifo) AstConst(LitVal(val.index()));
        case WasmToken::SignedInteger: {
          CheckedInt<int32_t> sint = val.sint();
          if (!sint.isValid()) {
            break;
          }
          return new (c.lifo) AstConst(LitVal(uint32_t(sint.value())));
        }
        case WasmToken::NegativeZero:
          return new (c.lifo) AstConst(LitVal(uint32_t(0)));
        default:
          break;
      }
      break;
    }
    case ValType::I64: {
      switch (val.kind()) {
        case WasmToken::Index:
          return new (c.lifo) AstConst(LitVal(uint64_t(val.index())));
        case WasmToken::UnsignedInteger:
          return new (c.lifo) AstConst(LitVal(val.uint()));
        case WasmToken::SignedInteger:
          return new (c.lifo) AstConst(LitVal(uint64_t(val.sint())));
        case WasmToken::NegativeZero:
          return new (c.lifo) AstConst(LitVal(uint64_t(0)));
        default:
          break;
      }
      break;
    }
    case ValType::F32: {
      return ParseFloatLiteral<float>(c, val);
    }
    case ValType::F64: {
      return ParseFloatLiteral<double>(c, val);
    }
    default:
      break;
  }
  c.ts.generateError(constToken, c.error);
  return nullptr;
}

static AstGetLocal* ParseGetLocal(WasmParseContext& c) {
  AstRef local;
  if (!c.ts.matchRef(&local, c.error)) {
    return nullptr;
  }

  return new (c.lifo) AstGetLocal(local);
}

static AstGetGlobal* ParseGetGlobal(WasmParseContext& c) {
  AstRef local;
  if (!c.ts.matchRef(&local, c.error)) {
    return nullptr;
  }
  return new (c.lifo) AstGetGlobal(local);
}

static AstSetGlobal* ParseSetGlobal(WasmParseContext& c, bool inParens) {
  AstRef global;
  if (!c.ts.matchRef(&global, c.error)) {
    return nullptr;
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  return new (c.lifo) AstSetGlobal(global, *value);
}

static AstSetLocal* ParseSetLocal(WasmParseContext& c, bool inParens) {
  AstRef local;
  if (!c.ts.matchRef(&local, c.error)) {
    return nullptr;
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  return new (c.lifo) AstSetLocal(local, *value);
}

static AstTeeLocal* ParseTeeLocal(WasmParseContext& c, bool inParens) {
  AstRef local;
  if (!c.ts.matchRef(&local, c.error)) {
    return nullptr;
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  return new (c.lifo) AstTeeLocal(local, *value);
}

static AstReturn* ParseReturn(WasmParseContext& c, bool inParens) {
  AstExpr* maybeExpr = nullptr;

  if (c.ts.peek().kind() != WasmToken::CloseParen) {
    maybeExpr = ParseExpr(c, inParens);
    if (!maybeExpr) {
      return nullptr;
    }
  }

  return new (c.lifo) AstReturn(maybeExpr);
}

static AstUnaryOperator* ParseUnaryOperator(WasmParseContext& c, Op op,
                                            bool inParens) {
  AstExpr* operand = ParseExpr(c, inParens);
  if (!operand) {
    return nullptr;
  }

  return new (c.lifo) AstUnaryOperator(op, operand);
}

static AstBinaryOperator* ParseBinaryOperator(WasmParseContext& c, Op op,
                                              bool inParens) {
  AstExpr* lhs = ParseExpr(c, inParens);
  if (!lhs) {
    return nullptr;
  }

  AstExpr* rhs = ParseExpr(c, inParens);
  if (!rhs) {
    return nullptr;
  }

  return new (c.lifo) AstBinaryOperator(op, lhs, rhs);
}

static AstComparisonOperator* ParseComparisonOperator(WasmParseContext& c,
                                                      Op op, bool inParens) {
  AstExpr* lhs = ParseExpr(c, inParens);
  if (!lhs) {
    return nullptr;
  }

  AstExpr* rhs = ParseExpr(c, inParens);
  if (!rhs) {
    return nullptr;
  }

  return new (c.lifo) AstComparisonOperator(op, lhs, rhs);
}

static AstTernaryOperator* ParseTernaryOperator(WasmParseContext& c, Op op,
                                                bool inParens) {
  AstExpr* op0 = ParseExpr(c, inParens);
  if (!op0) {
    return nullptr;
  }

  AstExpr* op1 = ParseExpr(c, inParens);
  if (!op1) {
    return nullptr;
  }

  AstExpr* op2 = ParseExpr(c, inParens);
  if (!op2) {
    return nullptr;
  }

  return new (c.lifo) AstTernaryOperator(op, op0, op1, op2);
}

static AstConversionOperator* ParseConversionOperator(WasmParseContext& c,
                                                      Op op, bool inParens) {
  AstExpr* operand = ParseExpr(c, inParens);
  if (!operand) {
    return nullptr;
  }

  return new (c.lifo) AstConversionOperator(op, operand);
}

static AstExtraConversionOperator* ParseExtraConversionOperator(
    WasmParseContext& c, MiscOp op, bool inParens) {
  AstExpr* operand = ParseExpr(c, inParens);
  if (!operand) {
    return nullptr;
  }

  return new (c.lifo) AstExtraConversionOperator(op, operand);
}

static AstDrop* ParseDrop(WasmParseContext& c, bool inParens) {
  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  return new (c.lifo) AstDrop(*value);
}

static AstIf* ParseIf(WasmParseContext& c, bool inParens) {
  AstName name = c.ts.getIfName();

  AstExprType type(ExprType::Limit);
  if (!ParseBlockSignature(c, &type)) {
    return nullptr;
  }

  AstExpr* cond = ParseExpr(c, inParens);
  if (!cond) {
    return nullptr;
  }

  if (inParens) {
    if (!c.ts.match(WasmToken::OpenParen, c.error)) {
      return nullptr;
    }
  }

  AstExprVector thenExprs(c.lifo);
  if (!inParens || c.ts.getIf(WasmToken::Then)) {
    if (!ParseExprList(c, &thenExprs)) {
      return nullptr;
    }
  } else {
    AstExpr* thenBranch = ParseExprInsideParens(c);
    if (!thenBranch || !thenExprs.append(thenBranch)) {
      return nullptr;
    }
  }
  if (inParens) {
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return nullptr;
    }
  }

  AstExprVector elseExprs(c.lifo);
  if (!inParens || c.ts.getIf(WasmToken::OpenParen)) {
    if (c.ts.getIf(WasmToken::Else)) {
      if (!MaybeMatchName(c, name)) {
        return nullptr;
      }
      if (!ParseExprList(c, &elseExprs)) {
        return nullptr;
      }
    } else if (inParens) {
      AstExpr* elseBranch = ParseExprInsideParens(c);
      if (!elseBranch || !elseExprs.append(elseBranch)) {
        return nullptr;
      }
    }
    if (inParens) {
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
    } else {
      if (!c.ts.match(WasmToken::End, c.error)) {
        return nullptr;
      }
      if (!MaybeMatchName(c, name)) {
        return nullptr;
      }
    }
  }

  return new (c.lifo)
      AstIf(type, cond, name, std::move(thenExprs), std::move(elseExprs));
}

static bool ParseLoadStoreAddress(WasmParseContext& c, int32_t* offset,
                                  uint32_t* alignLog2, AstExpr** base,
                                  bool inParens) {
  *offset = 0;
  if (c.ts.getIf(WasmToken::Offset)) {
    if (!c.ts.match(WasmToken::Equal, c.error)) {
      return false;
    }
    WasmToken val = c.ts.get();
    switch (val.kind()) {
      case WasmToken::Index:
        *offset = val.index();
        break;
      default:
        c.ts.generateError(val, c.error);
        return false;
    }
  }

  *alignLog2 = UINT32_MAX;
  if (c.ts.getIf(WasmToken::Align)) {
    if (!c.ts.match(WasmToken::Equal, c.error)) {
      return false;
    }
    WasmToken val = c.ts.get();
    switch (val.kind()) {
      case WasmToken::Index:
        if (!IsPowerOfTwo(val.index())) {
          c.ts.generateError(val, "non-power-of-two alignment", c.error);
          return false;
        }
        *alignLog2 = CeilingLog2(val.index());
        break;
      default:
        c.ts.generateError(val, c.error);
        return false;
    }
  }

  *base = ParseExpr(c, inParens);
  if (!*base) {
    return false;
  }

  return true;
}

static AstLoad* ParseLoad(WasmParseContext& c, Op op, bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case Op::I32Load8S:
      case Op::I32Load8U:
      case Op::I64Load8S:
      case Op::I64Load8U:
        alignLog2 = 0;
        break;
      case Op::I32Load16S:
      case Op::I32Load16U:
      case Op::I64Load16S:
      case Op::I64Load16U:
        alignLog2 = 1;
        break;
      case Op::I32Load:
      case Op::F32Load:
      case Op::I64Load32S:
      case Op::I64Load32U:
        alignLog2 = 2;
        break;
      case Op::I64Load:
      case Op::F64Load:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad load op");
    }
  }

  uint32_t flags = alignLog2;

  return new (c.lifo) AstLoad(op, AstLoadStoreAddress(base, flags, offset));
}

static AstStore* ParseStore(WasmParseContext& c, Op op, bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case Op::I32Store8:
      case Op::I64Store8:
        alignLog2 = 0;
        break;
      case Op::I32Store16:
      case Op::I64Store16:
        alignLog2 = 1;
        break;
      case Op::I32Store:
      case Op::F32Store:
      case Op::I64Store32:
        alignLog2 = 2;
        break;
      case Op::I64Store:
      case Op::F64Store:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad load op");
    }
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo)
      AstStore(op, AstLoadStoreAddress(base, flags, offset), value);
}

static AstAtomicCmpXchg* ParseAtomicCmpXchg(WasmParseContext& c, ThreadOp op,
                                            bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case ThreadOp::I32AtomicCmpXchg8U:
      case ThreadOp::I64AtomicCmpXchg8U:
        alignLog2 = 0;
        break;
      case ThreadOp::I32AtomicCmpXchg16U:
      case ThreadOp::I64AtomicCmpXchg16U:
        alignLog2 = 1;
        break;
      case ThreadOp::I32AtomicCmpXchg:
      case ThreadOp::I64AtomicCmpXchg32U:
        alignLog2 = 2;
        break;
      case ThreadOp::I64AtomicCmpXchg:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad cmpxchg op");
    }
  }

  AstExpr* expected = ParseExpr(c, inParens);
  if (!expected) {
    return nullptr;
  }

  AstExpr* replacement = ParseExpr(c, inParens);
  if (!replacement) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo) AstAtomicCmpXchg(
      op, AstLoadStoreAddress(base, flags, offset), expected, replacement);
}

static AstAtomicLoad* ParseAtomicLoad(WasmParseContext& c, ThreadOp op,
                                      bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case ThreadOp::I32AtomicLoad8U:
      case ThreadOp::I64AtomicLoad8U:
        alignLog2 = 0;
        break;
      case ThreadOp::I32AtomicLoad16U:
      case ThreadOp::I64AtomicLoad16U:
        alignLog2 = 1;
        break;
      case ThreadOp::I32AtomicLoad:
      case ThreadOp::I64AtomicLoad32U:
        alignLog2 = 2;
        break;
      case ThreadOp::I64AtomicLoad:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad load op");
    }
  }

  uint32_t flags = alignLog2;

  return new (c.lifo)
      AstAtomicLoad(op, AstLoadStoreAddress(base, flags, offset));
}

static AstAtomicRMW* ParseAtomicRMW(WasmParseContext& c, ThreadOp op,
                                    bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case ThreadOp::I32AtomicAdd8U:
      case ThreadOp::I64AtomicAdd8U:
      case ThreadOp::I32AtomicAnd8U:
      case ThreadOp::I64AtomicAnd8U:
      case ThreadOp::I32AtomicOr8U:
      case ThreadOp::I64AtomicOr8U:
      case ThreadOp::I32AtomicSub8U:
      case ThreadOp::I64AtomicSub8U:
      case ThreadOp::I32AtomicXor8U:
      case ThreadOp::I64AtomicXor8U:
      case ThreadOp::I32AtomicXchg8U:
      case ThreadOp::I64AtomicXchg8U:
        alignLog2 = 0;
        break;
      case ThreadOp::I32AtomicAdd16U:
      case ThreadOp::I64AtomicAdd16U:
      case ThreadOp::I32AtomicAnd16U:
      case ThreadOp::I64AtomicAnd16U:
      case ThreadOp::I32AtomicOr16U:
      case ThreadOp::I64AtomicOr16U:
      case ThreadOp::I32AtomicSub16U:
      case ThreadOp::I64AtomicSub16U:
      case ThreadOp::I32AtomicXor16U:
      case ThreadOp::I64AtomicXor16U:
      case ThreadOp::I32AtomicXchg16U:
      case ThreadOp::I64AtomicXchg16U:
        alignLog2 = 1;
        break;
      case ThreadOp::I32AtomicAdd:
      case ThreadOp::I64AtomicAdd32U:
      case ThreadOp::I32AtomicAnd:
      case ThreadOp::I64AtomicAnd32U:
      case ThreadOp::I32AtomicOr:
      case ThreadOp::I64AtomicOr32U:
      case ThreadOp::I32AtomicSub:
      case ThreadOp::I64AtomicSub32U:
      case ThreadOp::I32AtomicXor:
      case ThreadOp::I64AtomicXor32U:
      case ThreadOp::I32AtomicXchg:
      case ThreadOp::I64AtomicXchg32U:
        alignLog2 = 2;
        break;
      case ThreadOp::I64AtomicAdd:
      case ThreadOp::I64AtomicAnd:
      case ThreadOp::I64AtomicOr:
      case ThreadOp::I64AtomicSub:
      case ThreadOp::I64AtomicXor:
      case ThreadOp::I64AtomicXchg:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad RMW op");
    }
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo)
      AstAtomicRMW(op, AstLoadStoreAddress(base, flags, offset), value);
}

static AstAtomicStore* ParseAtomicStore(WasmParseContext& c, ThreadOp op,
                                        bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case ThreadOp::I32AtomicStore8U:
      case ThreadOp::I64AtomicStore8U:
        alignLog2 = 0;
        break;
      case ThreadOp::I32AtomicStore16U:
      case ThreadOp::I64AtomicStore16U:
        alignLog2 = 1;
        break;
      case ThreadOp::I32AtomicStore:
      case ThreadOp::I64AtomicStore32U:
        alignLog2 = 2;
        break;
      case ThreadOp::I64AtomicStore:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad store op");
    }
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo)
      AstAtomicStore(op, AstLoadStoreAddress(base, flags, offset), value);
}

static AstWait* ParseWait(WasmParseContext& c, ThreadOp op, bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  if (alignLog2 == UINT32_MAX) {
    switch (op) {
      case ThreadOp::I32Wait:
        alignLog2 = 2;
        break;
      case ThreadOp::I64Wait:
        alignLog2 = 3;
        break;
      default:
        MOZ_CRASH("Bad wait op");
    }
  }

  AstExpr* expected = ParseExpr(c, inParens);
  if (!expected) {
    return nullptr;
  }

  AstExpr* timeout = ParseExpr(c, inParens);
  if (!timeout) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo)
      AstWait(op, AstLoadStoreAddress(base, flags, offset), expected, timeout);
}

static AstWake* ParseWake(WasmParseContext& c, bool inParens) {
  int32_t offset;
  uint32_t alignLog2;
  AstExpr* base;
  if (!ParseLoadStoreAddress(c, &offset, &alignLog2, &base, inParens)) {
    return nullptr;
  }

  // Per spec, the required (and default) alignment is 4, because the smallest
  // access is int32.
  if (alignLog2 == UINT32_MAX) {
    alignLog2 = 2;
  }

  AstExpr* count = ParseExpr(c, inParens);
  if (!count) {
    return nullptr;
  }

  uint32_t flags = alignLog2;

  return new (c.lifo) AstWake(AstLoadStoreAddress(base, flags, offset), count);
}

static AstBranchTable* ParseBranchTable(WasmParseContext& c, bool inParens) {
  AstRefVector table(c.lifo);

  AstRef target;
  while (c.ts.getIfRef(&target)) {
    if (!table.append(target)) {
      return nullptr;
    }
  }

  if (table.empty()) {
    c.ts.generateError(c.ts.get(), c.error);
    return nullptr;
  }

  AstRef def = table.popCopy();

  AstExpr* index = ParseExpr(c, inParens);
  if (!index) {
    return nullptr;
  }

  AstExpr* value = nullptr;
  if (inParens) {
    if (c.ts.getIf(WasmToken::OpenParen)) {
      value = index;
      index = ParseExprInsideParens(c);
      if (!index) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
    }
  }

  return new (c.lifo) AstBranchTable(*index, def, std::move(table), value);
}

static AstGrowMemory* ParseGrowMemory(WasmParseContext& c, bool inParens) {
  AstExpr* operand = ParseExpr(c, inParens);
  if (!operand) {
    return nullptr;
  }

  return new (c.lifo) AstGrowMemory(operand);
}

#ifdef ENABLE_WASM_BULKMEM_OPS
static AstMemOrTableCopy* ParseMemOrTableCopy(WasmParseContext& c,
                                              bool inParens, bool isMem) {
  // (table.copy dest-table dest src-table src len)
  // (table.copy dest src len)
  // (memory.copy dest src len)

  AstRef targetMemOrTable = AstRef(0);
  bool requireSource = false;
  if (!isMem) {
    if (c.ts.getIfRef(&targetMemOrTable)) {
      requireSource = true;
    }
  }

  AstExpr* dest = ParseExpr(c, inParens);
  if (!dest) {
    return nullptr;
  }

  AstRef memOrTableSource = AstRef(0);
  if (requireSource) {
    if (!c.ts.getIfRef(&memOrTableSource)) {
      c.ts.generateError(c.ts.peek(),
                         "source is required if target is specified", c.error);
      return nullptr;
    }
  }

  AstExpr* src = ParseExpr(c, inParens);
  if (!src) {
    return nullptr;
  }

  AstExpr* len = ParseExpr(c, inParens);
  if (!len) {
    return nullptr;
  }

  return new (c.lifo) AstMemOrTableCopy(isMem, targetMemOrTable, dest,
                                        memOrTableSource, src, len);
}

static AstMemOrTableDrop* ParseMemOrTableDrop(WasmParseContext& c, bool isMem) {
  WasmToken segIndexTok;
  if (!c.ts.getIf(WasmToken::Index, &segIndexTok)) {
    return nullptr;
  }

  return new (c.lifo) AstMemOrTableDrop(isMem, segIndexTok.index());
}

static AstMemFill* ParseMemFill(WasmParseContext& c, bool inParens) {
  AstExpr* start = ParseExpr(c, inParens);
  if (!start) {
    return nullptr;
  }

  AstExpr* val = ParseExpr(c, inParens);
  if (!val) {
    return nullptr;
  }

  AstExpr* len = ParseExpr(c, inParens);
  if (!len) {
    return nullptr;
  }

  return new (c.lifo) AstMemFill(start, val, len);
}

static AstMemOrTableInit* ParseMemOrTableInit(WasmParseContext& c,
                                              bool inParens, bool isMem) {
  // (table.init table-index segment-index ...)
  // (table.init segment-index ...)
  // (memory.init segment-index ...)

  AstRef targetMemOrTable = AstRef(0);
  uint32_t segIndex = 0;

  WasmToken segIndexTok;
  if (isMem) {
    if (!c.ts.getIf(WasmToken::Index, &segIndexTok)) {
      return nullptr;
    }
    segIndex = segIndexTok.index();
  } else {
    // Slightly hairy to parse this for tables because the element index "0"
    // could just as well be the table index "0".
    c.ts.getIfRef(&targetMemOrTable);
    if (c.ts.getIf(WasmToken::Index, &segIndexTok)) {
      segIndex = segIndexTok.index();
    } else if (targetMemOrTable.isIndex()) {
      segIndex = targetMemOrTable.index();
      targetMemOrTable = AstRef(0);
    } else {
      c.ts.generateError(c.ts.peek(), "expected element segment reference",
                         c.error);
      return nullptr;
    }
  }

  AstExpr* dst = ParseExpr(c, inParens);
  if (!dst) {
    return nullptr;
  }

  AstExpr* src = ParseExpr(c, inParens);
  if (!src) {
    return nullptr;
  }

  AstExpr* len = ParseExpr(c, inParens);
  if (!len) {
    return nullptr;
  }

  return new (c.lifo)
      AstMemOrTableInit(isMem, segIndex, targetMemOrTable, dst, src, len);
}
#endif

#ifdef ENABLE_WASM_GENERALIZED_TABLES
static AstTableGet* ParseTableGet(WasmParseContext& c, bool inParens) {
  // (table.get table index)
  // (table.get index)

  AstRef targetTable = AstRef(0);
  c.ts.getIfRef(&targetTable);

  AstExpr* index = ParseExpr(c, inParens);
  if (!index) {
    return nullptr;
  }
  return new (c.lifo) AstTableGet(targetTable, index);
}

static AstTableGrow* ParseTableGrow(WasmParseContext& c, bool inParens) {
  // (table.grow table delta)
  // (table.grow delta)

  AstRef targetTable = AstRef(0);
  c.ts.getIfRef(&targetTable);

  AstExpr* delta = ParseExpr(c, inParens);
  if (!delta) {
    return nullptr;
  }

  AstExpr* initValue = ParseExpr(c, inParens);
  if (!initValue) {
    return nullptr;
  }

  return new (c.lifo) AstTableGrow(targetTable, delta, initValue);
}

static AstTableSet* ParseTableSet(WasmParseContext& c, bool inParens) {
  // (table.set table index value)
  // (table.set index value)

  AstRef targetTable = AstRef(0);
  c.ts.getIfRef(&targetTable);

  AstExpr* index = ParseExpr(c, inParens);
  if (!index) {
    return nullptr;
  }
  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }
  return new (c.lifo) AstTableSet(targetTable, index, value);
}

static AstTableSize* ParseTableSize(WasmParseContext& c, bool inParens) {
  // (table.size table)
  // (table.size)

  AstRef targetTable = AstRef(0);
  c.ts.getIfRef(&targetTable);

  return new (c.lifo) AstTableSize(targetTable);
}
#endif

#ifdef ENABLE_WASM_GC
static AstExpr* ParseStructNew(WasmParseContext& c, bool inParens) {
  AstRef typeDef;
  if (!c.ts.matchRef(&typeDef, c.error)) {
    return nullptr;
  }

  AstExprVector args(c.lifo);
  if (inParens) {
    if (!ParseArgs(c, &args)) {
      return nullptr;
    }
  }

  // An AstRef cast to AstValType turns into a Ref type, which is exactly what
  // we need here.

  return new (c.lifo)
      AstStructNew(typeDef, AstExprType(AstValType(typeDef)), std::move(args));
}

static AstExpr* ParseStructGet(WasmParseContext& c, bool inParens) {
  AstRef typeDef;
  if (!c.ts.matchRef(&typeDef, c.error)) {
    return nullptr;
  }

  AstRef fieldDef;
  if (!c.ts.matchRef(&fieldDef, c.error)) {
    return nullptr;
  }

  AstExpr* ptr = ParseExpr(c, inParens);
  if (!ptr) {
    return nullptr;
  }

  // The field type is not available here, we must first resolve the type.
  // Fortunately, we don't need to inspect the result type of this operation.

  return new (c.lifo) AstStructGet(typeDef, fieldDef, ExprType(), ptr);
}

static AstExpr* ParseStructSet(WasmParseContext& c, bool inParens) {
  AstRef typeDef;
  if (!c.ts.matchRef(&typeDef, c.error)) {
    return nullptr;
  }

  AstRef fieldDef;
  if (!c.ts.matchRef(&fieldDef, c.error)) {
    return nullptr;
  }

  AstExpr* ptr = ParseExpr(c, inParens);
  if (!ptr) {
    return nullptr;
  }

  AstExpr* value = ParseExpr(c, inParens);
  if (!value) {
    return nullptr;
  }

  return new (c.lifo) AstStructSet(typeDef, fieldDef, ptr, value);
}

static AstExpr* ParseStructNarrow(WasmParseContext& c, bool inParens) {
  AstValType inputType;
  if (!ParseValType(c, &inputType)) {
    return nullptr;
  }

  if (!inputType.isRefType()) {
    c.ts.generateError(c.ts.peek(), "struct.narrow requires ref type", c.error);
    return nullptr;
  }

  AstValType outputType;
  if (!ParseValType(c, &outputType)) {
    return nullptr;
  }

  if (!outputType.isRefType()) {
    c.ts.generateError(c.ts.peek(), "struct.narrow requires ref type", c.error);
    return nullptr;
  }

  AstExpr* ptr = ParseExpr(c, inParens);
  if (!ptr) {
    return nullptr;
  }

  return new (c.lifo) AstStructNarrow(inputType, outputType, ptr);
}
#endif

static AstExpr* ParseRefNull(WasmParseContext& c) {
  return new (c.lifo) AstRefNull();
}

static AstExpr* ParseExprBody(WasmParseContext& c, WasmToken token,
                              bool inParens) {
  if (!CheckRecursionLimitDontReport(c.stackLimit)) {
    return nullptr;
  }
  switch (token.kind()) {
    case WasmToken::Unreachable:
      return new (c.lifo) AstUnreachable;
    case WasmToken::AtomicCmpXchg:
      return ParseAtomicCmpXchg(c, token.threadOp(), inParens);
    case WasmToken::AtomicLoad:
      return ParseAtomicLoad(c, token.threadOp(), inParens);
    case WasmToken::AtomicRMW:
      return ParseAtomicRMW(c, token.threadOp(), inParens);
    case WasmToken::AtomicStore:
      return ParseAtomicStore(c, token.threadOp(), inParens);
    case WasmToken::Wait:
      return ParseWait(c, token.threadOp(), inParens);
    case WasmToken::Wake:
      return ParseWake(c, inParens);
    case WasmToken::BinaryOpcode:
      return ParseBinaryOperator(c, token.op(), inParens);
    case WasmToken::Block:
      return ParseBlock(c, Op::Block, inParens);
    case WasmToken::Br:
      return ParseBranch(c, Op::Br, inParens);
    case WasmToken::BrIf:
      return ParseBranch(c, Op::BrIf, inParens);
    case WasmToken::BrTable:
      return ParseBranchTable(c, inParens);
    case WasmToken::Call:
      return ParseCall(c, inParens);
    case WasmToken::CallIndirect:
      return ParseCallIndirect(c, inParens);
    case WasmToken::ComparisonOpcode:
      return ParseComparisonOperator(c, token.op(), inParens);
    case WasmToken::Const:
      return ParseConst(c, token);
    case WasmToken::ConversionOpcode:
      return ParseConversionOperator(c, token.op(), inParens);
    case WasmToken::ExtraConversionOpcode:
      return ParseExtraConversionOperator(c, token.miscOp(), inParens);
    case WasmToken::Drop:
      return ParseDrop(c, inParens);
    case WasmToken::If:
      return ParseIf(c, inParens);
    case WasmToken::GetGlobal:
      return ParseGetGlobal(c);
    case WasmToken::GetLocal:
      return ParseGetLocal(c);
    case WasmToken::Load:
      return ParseLoad(c, token.op(), inParens);
    case WasmToken::Loop:
      return ParseBlock(c, Op::Loop, inParens);
    case WasmToken::Return:
      return ParseReturn(c, inParens);
    case WasmToken::SetGlobal:
      return ParseSetGlobal(c, inParens);
    case WasmToken::SetLocal:
      return ParseSetLocal(c, inParens);
    case WasmToken::Store:
      return ParseStore(c, token.op(), inParens);
    case WasmToken::TeeLocal:
      return ParseTeeLocal(c, inParens);
    case WasmToken::TernaryOpcode:
      return ParseTernaryOperator(c, token.op(), inParens);
    case WasmToken::UnaryOpcode:
      return ParseUnaryOperator(c, token.op(), inParens);
    case WasmToken::Nop:
      return new (c.lifo) AstNop();
    case WasmToken::CurrentMemory:
      return new (c.lifo) AstCurrentMemory();
    case WasmToken::GrowMemory:
      return ParseGrowMemory(c, inParens);
#ifdef ENABLE_WASM_BULKMEM_OPS
    case WasmToken::MemCopy:
      return ParseMemOrTableCopy(c, inParens, /*isMem=*/true);
    case WasmToken::MemDrop:
      return ParseMemOrTableDrop(c, /*isMem=*/true);
    case WasmToken::MemFill:
      return ParseMemFill(c, inParens);
    case WasmToken::MemInit:
      return ParseMemOrTableInit(c, inParens, /*isMem=*/true);
    case WasmToken::TableCopy:
      return ParseMemOrTableCopy(c, inParens, /*isMem=*/false);
    case WasmToken::TableDrop:
      return ParseMemOrTableDrop(c, /*isMem=*/false);
    case WasmToken::TableInit:
      return ParseMemOrTableInit(c, inParens, /*isMem=*/false);
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    case WasmToken::TableGet:
      return ParseTableGet(c, inParens);
    case WasmToken::TableGrow:
      return ParseTableGrow(c, inParens);
    case WasmToken::TableSet:
      return ParseTableSet(c, inParens);
    case WasmToken::TableSize:
      return ParseTableSize(c, inParens);
#endif
#ifdef ENABLE_WASM_GC
    case WasmToken::StructNew:
      return ParseStructNew(c, inParens);
    case WasmToken::StructGet:
      return ParseStructGet(c, inParens);
    case WasmToken::StructSet:
      return ParseStructSet(c, inParens);
    case WasmToken::StructNarrow:
      return ParseStructNarrow(c, inParens);
#endif
    case WasmToken::RefNull:
      return ParseRefNull(c);
    default:
      c.ts.generateError(token, c.error);
      return nullptr;
  }
}

static AstExpr* ParseExprInsideParens(WasmParseContext& c) {
  WasmToken token = c.ts.get();

  return ParseExprBody(c, token, true);
}

static bool ParseValueTypeList(WasmParseContext& c, AstValTypeVector* vec) {
  for (;;) {
    AstValType vt;
    if (!MaybeParseValType(c, &vt)) {
      return false;
    }
    if (!vt.isValid()) {
      break;
    }
    if (!vec->append(vt)) {
      return false;
    }
  }
  return true;
}

static bool ParseResult(WasmParseContext& c, AstExprType* result) {
  if (!result->isVoid()) {
    c.ts.generateError(c.ts.peek(), c.error);
    return false;
  }

  AstValType type;
  if (!ParseValType(c, &type)) {
    return false;
  }

  *result = AstExprType(type);
  return true;
}

static bool ParseLocalOrParam(WasmParseContext& c, AstNameVector* locals,
                              AstValTypeVector* localTypes) {
  if (c.ts.peek().kind() != WasmToken::Name) {
    return locals->append(AstName()) && ParseValueTypeList(c, localTypes);
  }

  AstValType type;
  return locals->append(c.ts.get().name()) && ParseValType(c, &type) &&
         localTypes->append(type);
}

static bool ParseInlineImport(WasmParseContext& c, InlineImport* import) {
  return c.ts.match(WasmToken::Text, &import->module, c.error) &&
         c.ts.match(WasmToken::Text, &import->field, c.error);
}

static bool ParseInlineExport(WasmParseContext& c, DefinitionKind kind,
                              AstModule* module, AstRef ref) {
  WasmToken name;
  if (!c.ts.match(WasmToken::Text, &name, c.error)) {
    return false;
  }

  AstExport* exp = new (c.lifo) AstExport(name.text(), kind, ref);
  return exp && module->append(exp);
}

static bool MaybeParseTypeUse(WasmParseContext& c, AstRef* funcType) {
  WasmToken openParen;
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Type)) {
      if (!c.ts.matchRef(funcType, c.error)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
    } else {
      c.ts.unget(openParen);
    }
  }
  return true;
}

static bool ParseFuncSig(WasmParseContext& c, AstFuncType* funcType) {
  AstValTypeVector args(c.lifo);
  AstExprType result = AstExprType(ExprType::Void);

  while (c.ts.getIf(WasmToken::OpenParen)) {
    WasmToken token = c.ts.get();
    switch (token.kind()) {
      case WasmToken::Param:
        if (!ParseValueTypeList(c, &args)) {
          return false;
        }
        break;
      case WasmToken::Result:
        if (!ParseResult(c, &result)) {
          return false;
        }
        break;
      default:
        c.ts.generateError(token, c.error);
        return false;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }
  }

  *funcType = AstFuncType(std::move(args), result);
  return true;
}

static bool ParseFuncType(WasmParseContext& c, AstRef* ref, AstModule* module) {
  if (!MaybeParseTypeUse(c, ref)) {
    return false;
  }

  if (ref->isInvalid()) {
    AstFuncType funcType(c.lifo);
    if (!ParseFuncSig(c, &funcType)) {
      return false;
    }
    uint32_t funcTypeIndex;
    if (!module->declare(std::move(funcType), &funcTypeIndex)) {
      return false;
    }
    ref->setIndex(funcTypeIndex);
  }

  return true;
}

static bool ParseFunc(WasmParseContext& c, AstModule* module) {
  AstValTypeVector vars(c.lifo);
  AstValTypeVector args(c.lifo);
  AstNameVector locals(c.lifo);

  AstName funcName = c.ts.getIfName();

  // Inline imports and exports.
  WasmToken openParen;
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Import)) {
      if (module->funcs().length()) {
        c.ts.generateError(openParen, "import after function definition",
                           c.error);
        return false;
      }

      InlineImport names;
      if (!ParseInlineImport(c, &names)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }

      AstRef funcType;
      if (!ParseFuncType(c, &funcType, module)) {
        return false;
      }

      auto* imp = new (c.lifo) AstImport(funcName, names.module.text(),
                                         names.field.text(), funcType);
      return imp && module->append(imp);
    }

    if (c.ts.getIf(WasmToken::Export)) {
      AstRef ref =
          funcName.empty()
              ? AstRef(module->numFuncImports() + module->funcs().length())
              : AstRef(funcName);
      if (!ParseInlineExport(c, DefinitionKind::Function, module, ref)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
    } else {
      c.ts.unget(openParen);
    }
  }

  AstRef funcTypeRef;
  if (!MaybeParseTypeUse(c, &funcTypeRef)) {
    return false;
  }

  AstExprVector body(c.lifo);

  AstExprType result = AstExprType(ExprType::Void);
  while (c.ts.getIf(WasmToken::OpenParen)) {
    WasmToken token = c.ts.get();
    switch (token.kind()) {
      case WasmToken::Local:
        if (!ParseLocalOrParam(c, &locals, &vars)) {
          return false;
        }
        break;
      case WasmToken::Param:
        if (!vars.empty()) {
          c.ts.generateError(token, c.error);
          return false;
        }
        if (!ParseLocalOrParam(c, &locals, &args)) {
          return false;
        }
        break;
      case WasmToken::Result:
        if (!ParseResult(c, &result)) {
          return false;
        }
        break;
      default:
        c.ts.unget(token);
        AstExpr* expr = ParseExprInsideParens(c);
        if (!expr || !body.append(expr)) {
          return false;
        }
        break;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }
  }

  if (!ParseExprList(c, &body)) {
    return false;
  }

  if (funcTypeRef.isInvalid()) {
    uint32_t funcTypeIndex;
    if (!module->declare(AstFuncType(std::move(args), result),
                         &funcTypeIndex)) {
      return false;
    }
    funcTypeRef.setIndex(funcTypeIndex);
  }

  auto* func = new (c.lifo) AstFunc(funcName, funcTypeRef, std::move(vars),
                                    std::move(locals), std::move(body));
  return func && module->append(func);
}

static bool ParseGlobalType(WasmParseContext& c, AstValType* type,
                            bool* isMutable);

static bool ParseStructFields(WasmParseContext& c, AstStructType* st) {
  AstNameVector names(c.lifo);
  AstBoolVector mutability(c.lifo);
  AstValTypeVector types(c.lifo);

  while (true) {
    if (!c.ts.getIf(WasmToken::OpenParen)) {
      break;
    }

    if (!c.ts.match(WasmToken::Field, c.error)) {
      return false;
    }

    AstName name = c.ts.getIfName();

    AstValType type;
    bool isMutable;
    if (!ParseGlobalType(c, &type, &isMutable)) {
      return false;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }

    if (!names.append(name)) {
      return false;
    }
    if (!mutability.append(isMutable)) {
      return false;
    }
    if (!types.append(type)) {
      return false;
    }
  }

  *st =
      AstStructType(std::move(names), std::move(mutability), std::move(types));
  return true;
}

static AstTypeDef* ParseTypeDef(WasmParseContext& c) {
  AstName name = c.ts.getIfName();

  if (!c.ts.match(WasmToken::OpenParen, c.error)) {
    return nullptr;
  }

  AstTypeDef* type = nullptr;
  if (c.ts.getIf(WasmToken::Func)) {
    AstFuncType funcType(c.lifo);
    if (!ParseFuncSig(c, &funcType)) {
      return nullptr;
    }

    type = new (c.lifo) AstFuncType(name, std::move(funcType));
  } else if (c.ts.getIf(WasmToken::Struct)) {
    AstStructType st(c.lifo);
    if (!ParseStructFields(c, &st)) {
      return nullptr;
    }

    type = new (c.lifo) AstStructType(name, std::move(st));
  } else {
    c.ts.generateError(c.ts.peek(), "bad type definition", c.error);
    return nullptr;
  }

  if (!c.ts.match(WasmToken::CloseParen, c.error)) {
    return nullptr;
  }

  return type;
}

static bool MaybeParseOwnerIndex(WasmParseContext& c) {
  if (c.ts.peek().kind() == WasmToken::Index) {
    WasmToken elemIndex = c.ts.get();
    if (elemIndex.index()) {
      c.ts.generateError(elemIndex, "can't handle non-default memory/table yet",
                         c.error);
      return false;
    }
  }
  return true;
}

static AstExpr* ParseInitializerExpression(WasmParseContext& c) {
  if (!c.ts.match(WasmToken::OpenParen, c.error)) {
    return nullptr;
  }

  AstExpr* initExpr = ParseExprInsideParens(c);
  if (!initExpr) {
    return nullptr;
  }

  if (!c.ts.match(WasmToken::CloseParen, c.error)) {
    return nullptr;
  }

  return initExpr;
}

static bool ParseInitializerExpressionOrPassive(WasmParseContext& c,
                                                AstExpr** maybeInitExpr) {
#ifdef ENABLE_WASM_BULKMEM_OPS
  if (c.ts.getIf(WasmToken::Passive)) {
    *maybeInitExpr = nullptr;
    return true;
  }
#endif

  AstExpr* initExpr = ParseInitializerExpression(c);
  if (!initExpr) {
    return false;
  }

  *maybeInitExpr = initExpr;
  return true;
}

static AstDataSegment* ParseDataSegment(WasmParseContext& c) {
  if (!MaybeParseOwnerIndex(c)) {
    return nullptr;
  }

  AstExpr* offsetIfActive;
  if (!ParseInitializerExpressionOrPassive(c, &offsetIfActive)) {
    return nullptr;
  }

  AstNameVector fragments(c.lifo);

  WasmToken text;
  while (c.ts.getIf(WasmToken::Text, &text)) {
    if (!fragments.append(text.text())) {
      return nullptr;
    }
  }

  return new (c.lifo) AstDataSegment(offsetIfActive, std::move(fragments));
}

static bool ParseLimits(WasmParseContext& c, Limits* limits,
                        Shareable allowShared) {
  WasmToken initial;
  if (!c.ts.match(WasmToken::Index, &initial, c.error)) {
    return false;
  }

  Maybe<uint32_t> maximum;
  WasmToken token;
  if (c.ts.getIf(WasmToken::Index, &token)) {
    maximum.emplace(token.index());
  }

  Shareable shared = Shareable::False;
  if (c.ts.getIf(WasmToken::Shared, &token)) {
    // A missing maximum is caught later.
    if (allowShared == Shareable::True) {
      shared = Shareable::True;
    } else {
      c.ts.generateError(token, "'shared' not allowed", c.error);
      return false;
    }
  }

  *limits = Limits(initial.index(), maximum, shared);
  return true;
}

static bool ParseMemory(WasmParseContext& c, AstModule* module) {
  AstName name = c.ts.getIfName();

  WasmToken openParen;
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Import)) {
      InlineImport names;
      if (!ParseInlineImport(c, &names)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }

      Limits memory;
      if (!ParseLimits(c, &memory, Shareable::True)) {
        return false;
      }

      auto* imp =
          new (c.lifo) AstImport(name, names.module.text(), names.field.text(),
                                 DefinitionKind::Memory, memory);
      return imp && module->append(imp);
    }

    if (c.ts.getIf(WasmToken::Export)) {
      AstRef ref =
          name.empty() ? AstRef(module->memories().length()) : AstRef(name);
      if (!ParseInlineExport(c, DefinitionKind::Memory, module, ref)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
    } else {
      c.ts.unget(openParen);
    }
  }

  if (c.ts.getIf(WasmToken::OpenParen)) {
    if (!c.ts.match(WasmToken::Data, c.error)) {
      return false;
    }

    AstNameVector fragments(c.lifo);

    WasmToken data;
    size_t pages = 0;
    size_t totalLength = 0;
    while (c.ts.getIf(WasmToken::Text, &data)) {
      if (!fragments.append(data.text())) {
        return false;
      }
      totalLength += data.text().length();
    }

    if (fragments.length()) {
      AstExpr* offset = new (c.lifo) AstConst(LitVal(uint32_t(0)));
      if (!offset) {
        return false;
      }

      auto* segment = new (c.lifo) AstDataSegment(offset, std::move(fragments));
      if (!segment || !module->append(segment)) {
        return false;
      }

      pages = AlignBytes<size_t>(totalLength, PageSize) / PageSize;
      if (pages != uint32_t(pages)) {
        return false;
      }
    }

    if (!module->addMemory(name,
                           Limits(pages, Some(pages), Shareable::False))) {
      return false;
    }

    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }

    return true;
  }

  Limits memory;
  if (!ParseLimits(c, &memory, Shareable::True)) {
    return false;
  }

  return module->addMemory(name, memory);
}

#ifdef ENABLE_WASM_GC
// Custom section for experimental work.  The size of this section should always
// be 1 byte, and that byte is a nonzero varint7 carrying the version number
// being opted into.
static bool ParseGcFeatureOptIn(WasmParseContext& c, AstModule* module) {
  WasmToken token;
  if (!c.ts.getIf(WasmToken::Index, &token)) {
    c.ts.generateError(token, "GC feature version number required", c.error);
    return false;
  }

  if (token.index() == 0 || token.index() > 127) {
    c.ts.generateError(token, "invalid GC feature version number", c.error);
    return false;
  }

  return module->addGcFeatureOptIn(token.index());
}
#endif

static bool ParseStartFunc(WasmParseContext& c, WasmToken token,
                           AstModule* module) {
  AstRef func;
  if (!c.ts.matchRef(&func, c.error)) {
    return false;
  }

  if (!module->setStartFunc(AstStartFunc(func))) {
    c.ts.generateError(token, c.error);
    return false;
  }

  return true;
}

static bool ParseGlobalType(WasmParseContext& c, AstValType* type,
                            bool* isMutable) {
  WasmToken openParen;
  *isMutable = false;

  // Either (mut T) or T, where T can be (ref U).
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Mutable)) {
      *isMutable = true;
      if (!ParseValType(c, type)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
      return true;
    }
    c.ts.unget(openParen);
  }

  if (!ParseValType(c, type)) {
    return false;
  }

  return true;
}

static bool ParseElemType(WasmParseContext& c, TableKind* tableKind) {
  WasmToken token;
  if (c.ts.getIf(WasmToken::AnyFunc, &token)) {
    *tableKind = TableKind::AnyFunction;
    return true;
  }
#ifdef ENABLE_WASM_GENERALIZED_TABLES
  if (c.ts.getIf(WasmToken::ValueType, &token) &&
      token.valueType() == ValType::AnyRef) {
    *tableKind = TableKind::AnyRef;
    return true;
  }
  c.ts.generateError(token, "'anyfunc' or 'anyref' required", c.error);
#else
  c.ts.generateError(token, "'anyfunc' required", c.error);
#endif
  return false;
}

static bool ParseTableSig(WasmParseContext& c, Limits* table,
                          TableKind* tableKind) {
  return ParseLimits(c, table, Shareable::False) && ParseElemType(c, tableKind);
}

static AstImport* ParseImport(WasmParseContext& c, AstModule* module) {
  AstName name = c.ts.getIfName();

  WasmToken moduleName;
  if (!c.ts.match(WasmToken::Text, &moduleName, c.error)) {
    return nullptr;
  }

  WasmToken fieldName;
  if (!c.ts.match(WasmToken::Text, &fieldName, c.error)) {
    return nullptr;
  }

  AstRef funcTypeRef;
  WasmToken openParen;
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Memory)) {
      if (name.empty()) {
        name = c.ts.getIfName();
      }

      Limits memory;
      if (!ParseLimits(c, &memory, Shareable::True)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
      return new (c.lifo) AstImport(name, moduleName.text(), fieldName.text(),
                                    DefinitionKind::Memory, memory);
    }
    if (c.ts.getIf(WasmToken::Table)) {
      if (name.empty()) {
        name = c.ts.getIfName();
      }

      TableKind tableKind;
      Limits table;
      if (!ParseTableSig(c, &table, &tableKind)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }

      return new (c.lifo) AstImport(name, moduleName.text(), fieldName.text(),
                                    table, tableKind);
    }
    if (c.ts.getIf(WasmToken::Global)) {
      if (name.empty()) {
        name = c.ts.getIfName();
      }

      AstValType type;
      bool isMutable;
      if (!ParseGlobalType(c, &type, &isMutable)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }

      return new (c.lifo) AstImport(name, moduleName.text(), fieldName.text(),
                                    AstGlobal(AstName(), type, isMutable));
    }
    if (c.ts.getIf(WasmToken::Func)) {
      if (name.empty()) {
        name = c.ts.getIfName();
      }

      AstRef funcTypeRef;
      if (!ParseFuncType(c, &funcTypeRef, module)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }

      return new (c.lifo)
          AstImport(name, moduleName.text(), fieldName.text(), funcTypeRef);
    }

    if (c.ts.getIf(WasmToken::Type)) {
      if (!c.ts.matchRef(&funcTypeRef, c.error)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }
    } else {
      c.ts.unget(openParen);
    }
  }

  if (funcTypeRef.isInvalid()) {
    AstFuncType funcType(c.lifo);
    if (!ParseFuncSig(c, &funcType)) {
      return nullptr;
    }

    uint32_t funcTypeIndex;
    if (!module->declare(std::move(funcType), &funcTypeIndex)) {
      return nullptr;
    }
    funcTypeRef.setIndex(funcTypeIndex);
  }

  return new (c.lifo)
      AstImport(name, moduleName.text(), fieldName.text(), funcTypeRef);
}

static AstExport* ParseExport(WasmParseContext& c) {
  WasmToken name;
  if (!c.ts.match(WasmToken::Text, &name, c.error)) {
    return nullptr;
  }

  WasmToken exportee = c.ts.get();
  switch (exportee.kind()) {
    case WasmToken::Index:
      if (exportee.index() == AstNoIndex) {
        c.ts.generateError(exportee, c.error);
        return nullptr;
      }
      return new (c.lifo) AstExport(name.text(), DefinitionKind::Function,
                                    AstRef(exportee.index()));
    case WasmToken::Name:
      return new (c.lifo) AstExport(name.text(), DefinitionKind::Function,
                                    AstRef(exportee.name()));
    case WasmToken::Table: {
      AstRef ref;
      if (!c.ts.getIfRef(&ref)) {
        ref = AstRef(0);
      }
      return new (c.lifo) AstExport(name.text(), DefinitionKind::Table, ref);
    }
    case WasmToken::Memory: {
      AstRef ref;
      if (!c.ts.getIfRef(&ref)) {
        ref = AstRef(0);
      }
      return new (c.lifo) AstExport(name.text(), DefinitionKind::Memory, ref);
    }
    case WasmToken::Global: {
      AstRef ref;
      if (!c.ts.matchRef(&ref, c.error)) {
        return nullptr;
      }
      return new (c.lifo) AstExport(name.text(), DefinitionKind::Global, ref);
    }
    case WasmToken::OpenParen: {
      exportee = c.ts.get();

      DefinitionKind kind;
      switch (exportee.kind()) {
        case WasmToken::Func:
          kind = DefinitionKind::Function;
          break;
        case WasmToken::Table:
          kind = DefinitionKind::Table;
          break;
        case WasmToken::Memory:
          kind = DefinitionKind::Memory;
          break;
        case WasmToken::Global:
          kind = DefinitionKind::Global;
          break;
        default:
          c.ts.generateError(exportee, c.error);
          return nullptr;
      }

      AstRef ref;
      if (!c.ts.matchRef(&ref, c.error)) {
        return nullptr;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return nullptr;
      }

      return new (c.lifo) AstExport(name.text(), kind, ref);
    }
    default:
      break;
  }

  c.ts.generateError(exportee, c.error);
  return nullptr;
}

static bool ParseTable(WasmParseContext& c, WasmToken token,
                       AstModule* module) {
  AstName name = c.ts.getIfName();

  if (c.ts.getIf(WasmToken::OpenParen)) {
    // Either an import and we're done, or an export and continue.
    if (c.ts.getIf(WasmToken::Import)) {
      InlineImport names;
      if (!ParseInlineImport(c, &names)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }

      TableKind tableKind;
      Limits table;
      if (!ParseTableSig(c, &table, &tableKind)) {
        return false;
      }

      auto* import = new (c.lifo) AstImport(
          name, names.module.text(), names.field.text(), table, tableKind);

      return import && module->append(import);
    }

    if (!c.ts.match(WasmToken::Export, c.error)) {
      c.ts.generateError(token, c.error);
      return false;
    }

    AstRef ref =
        name.empty() ? AstRef(module->tables().length()) : AstRef(name);
    if (!ParseInlineExport(c, DefinitionKind::Table, module, ref)) {
      return false;
    }
    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return false;
    }
  }

  // Either: min max? anyfunc
  if (c.ts.peek().kind() == WasmToken::Index) {
    TableKind tableKind;
    Limits table;
    if (!ParseTableSig(c, &table, &tableKind)) {
      return false;
    }
    return module->addTable(name, table, tableKind);
  }

  // Or: anyfunc (elem 1 2 ...)
  TableKind tableKind;
  if (!ParseElemType(c, &tableKind)) {
    return false;
  }

  if (!c.ts.match(WasmToken::OpenParen, c.error)) {
    return false;
  }
  if (!c.ts.match(WasmToken::Elem, c.error)) {
    return false;
  }

  if (name.empty()) {
    // For inline elements we need a name, so synthesize one if there isn't
    // one already.
    name = c.gensym("elem");
    if (name.empty()) return false;
  }

  AstRefVector elems(c.lifo);

  AstRef elem;
  while (c.ts.getIfRef(&elem)) {
    if (!elems.append(elem)) {
      return false;
    }
  }

  if (!c.ts.match(WasmToken::CloseParen, c.error)) {
    return false;
  }

  uint32_t numElements = uint32_t(elems.length());
  if (numElements != elems.length()) {
    return false;
  }

  if (!module->addTable(
          name, Limits(numElements, Some(numElements), Shareable::False),
          tableKind)) {
    return false;
  }

  auto* zero = new (c.lifo) AstConst(LitVal(uint32_t(0)));
  if (!zero) {
    return false;
  }

  AstElemSegment* segment =
      new (c.lifo) AstElemSegment(AstRef(name), zero, std::move(elems));
  return segment && module->append(segment);
}

static AstElemSegment* ParseElemSegment(WasmParseContext& c) {
  // (elem table-name init-expr ref ...)
  // (elem init-expr ref ...)
  // (elem passive ref ...)

  AstRef targetTable = AstRef(0);
  bool hasTableName = c.ts.getIfRef(&targetTable);

  AstExpr* offsetIfActive;
  if (!ParseInitializerExpressionOrPassive(c, &offsetIfActive)) {
    return nullptr;
  }

  if (hasTableName && !offsetIfActive) {
    c.ts.generateError(c.ts.peek(), "passive segment must not have a table",
                       c.error);
    return nullptr;
  }

  AstRefVector elems(c.lifo);

  AstRef elem;
  while (c.ts.getIfRef(&elem)) {
    if (!elems.append(elem)) {
      return nullptr;
    }
  }

  return new (c.lifo)
      AstElemSegment(targetTable, offsetIfActive, std::move(elems));
}

static bool ParseGlobal(WasmParseContext& c, AstModule* module) {
  AstName name = c.ts.getIfName();

  AstValType type;
  bool isMutable;

  WasmToken openParen;
  if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
    if (c.ts.getIf(WasmToken::Import)) {
      if (module->globals().length()) {
        c.ts.generateError(openParen, "import after global definition",
                           c.error);
        return false;
      }

      InlineImport names;
      if (!ParseInlineImport(c, &names)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }

      if (!ParseGlobalType(c, &type, &isMutable)) {
        return false;
      }

      auto* imp =
          new (c.lifo) AstImport(name, names.module.text(), names.field.text(),
                                 AstGlobal(AstName(), type, isMutable));
      return imp && module->append(imp);
    }

    if (c.ts.getIf(WasmToken::Export)) {
      size_t refIndex = module->numGlobalImports() + module->globals().length();
      AstRef ref = name.empty() ? AstRef(refIndex) : AstRef(name);
      if (!ParseInlineExport(c, DefinitionKind::Global, module, ref)) {
        return false;
      }
      if (!c.ts.match(WasmToken::CloseParen, c.error)) {
        return false;
      }
    } else {
      c.ts.unget(openParen);
    }
  }

  if (!ParseGlobalType(c, &type, &isMutable)) {
    return false;
  }

  AstExpr* init = ParseInitializerExpression(c);
  if (!init) {
    return false;
  }

  auto* glob = new (c.lifo) AstGlobal(name, type, isMutable, Some(init));
  return glob && module->append(glob);
}

static AstModule* ParseBinaryModule(WasmParseContext& c, AstModule* module) {
  // By convention with EncodeBinaryModule, a binary module only contains a
  // data section containing the raw bytes contained in the module.
  AstNameVector fragments(c.lifo);

  WasmToken text;
  while (c.ts.getIf(WasmToken::Text, &text)) {
    if (!fragments.append(text.text())) {
      return nullptr;
    }
  }

  auto* data = new (c.lifo) AstDataSegment(nullptr, std::move(fragments));
  if (!data || !module->append(data)) {
    return nullptr;
  }

  return module;
}

static AstModule* ParseModule(const char16_t* text, uintptr_t stackLimit,
                              LifoAlloc& lifo, UniqueChars* error,
                              bool* binary) {
  WasmParseContext c(text, stackLimit, lifo, error);

  *binary = false;

  if (!c.ts.match(WasmToken::OpenParen, c.error)) {
    return nullptr;
  }
  if (!c.ts.match(WasmToken::Module, c.error)) {
    return nullptr;
  }

  auto* module = new (c.lifo) AstModule(c.lifo);
  if (!module) {
    return nullptr;
  }

  if (c.ts.peek().kind() == WasmToken::Text) {
    *binary = true;
    return ParseBinaryModule(c, module);
  }

  while (c.ts.getIf(WasmToken::OpenParen)) {
    WasmToken section = c.ts.get();

    switch (section.kind()) {
      case WasmToken::Type: {
        AstTypeDef* typeDef = ParseTypeDef(c);
        if (!typeDef) {
          return nullptr;
        }
        if (!module->append(typeDef)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Start: {
        if (!ParseStartFunc(c, section, module)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Memory: {
        if (!ParseMemory(c, module)) {
          return nullptr;
        }
        break;
      }
#ifdef ENABLE_WASM_GC
      case WasmToken::GcFeatureOptIn: {
        if (!ParseGcFeatureOptIn(c, module)) {
          return nullptr;
        }
        break;
      }
#endif
      case WasmToken::Global: {
        if (!ParseGlobal(c, module)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Data: {
        AstDataSegment* segment = ParseDataSegment(c);
        if (!segment || !module->append(segment)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Import: {
        AstImport* imp = ParseImport(c, module);
        if (!imp || !module->append(imp)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Export: {
        AstExport* exp = ParseExport(c);
        if (!exp || !module->append(exp)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Table: {
        if (!ParseTable(c, section, module)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Elem: {
        AstElemSegment* segment = ParseElemSegment(c);
        if (!segment || !module->append(segment)) {
          return nullptr;
        }
        break;
      }
      case WasmToken::Func: {
        if (!ParseFunc(c, module)) {
          return nullptr;
        }
        break;
      }
      default:
        c.ts.generateError(section, c.error);
        return nullptr;
    }

    if (!c.ts.match(WasmToken::CloseParen, c.error)) {
      return nullptr;
    }
  }

  if (!c.ts.match(WasmToken::CloseParen, c.error)) {
    return nullptr;
  }
  if (!c.ts.match(WasmToken::EndOfFile, c.error)) {
    return nullptr;
  }

  return module;
}

/*****************************************************************************/
// wasm name resolution

namespace {

class Resolver {
  UniqueChars* error_;
  AstNameMap varMap_;
  AstNameMap globalMap_;
  AstNameMap funcTypeMap_;
  AstNameMap funcMap_;
  AstNameMap importMap_;
  AstNameMap tableMap_;
  AstNameMap memoryMap_;
  AstNameMap typeMap_;
  AstNameMap fieldMap_;
  AstNameVector targetStack_;

  bool registerName(AstNameMap& map, AstName name, size_t index) {
    AstNameMap::AddPtr p = map.lookupForAdd(name);
    if (!p) {
      if (!map.add(p, name, index)) {
        return false;
      }
    } else {
      return false;
    }
    return true;
  }
  bool resolveRef(AstNameMap& map, AstRef& ref) {
    AstNameMap::Ptr p = map.lookup(ref.name());
    if (p) {
      ref.setIndex(p->value());
      return true;
    }
    return false;
  }
  bool failResolveLabel(const char* kind, AstName name) {
    TwoByteChars chars(name.begin(), name.length());
    UniqueChars utf8Chars(CharsToNewUTF8CharsZ(nullptr, chars).c_str());
    *error_ = JS_smprintf("%s label '%s' not found", kind, utf8Chars.get());
    return false;
  }

 public:
  explicit Resolver(LifoAlloc& lifo, UniqueChars* error)
      : error_(error),
        varMap_(lifo),
        globalMap_(lifo),
        funcTypeMap_(lifo),
        funcMap_(lifo),
        importMap_(lifo),
        tableMap_(lifo),
        memoryMap_(lifo),
        typeMap_(lifo),
        fieldMap_(lifo),
        targetStack_(lifo) {}
  void beginFunc() {
    varMap_.clear();
    MOZ_ASSERT(targetStack_.empty());
  }

#define REGISTER(what, map)                                \
  bool register##what##Name(AstName name, size_t index) {  \
    return name.empty() || registerName(map, name, index); \
  }

  REGISTER(FuncType, funcTypeMap_)
  REGISTER(Func, funcMap_)
  REGISTER(Var, varMap_)
  REGISTER(Global, globalMap_)
  REGISTER(Table, tableMap_)
  REGISTER(Memory, memoryMap_)
  REGISTER(Type, typeMap_)
  REGISTER(Field, fieldMap_)

#undef REGISTER

  bool pushTarget(AstName name) { return targetStack_.append(name); }
  void popTarget(AstName name) {
    MOZ_ASSERT(targetStack_.back() == name);
    targetStack_.popBack();
  }

#define RESOLVE(map, label)                           \
  bool resolve##label(AstRef& ref) {                  \
    MOZ_ASSERT(!ref.isInvalid());                     \
    if (!ref.name().empty() && !resolveRef(map, ref)) \
      return failResolveLabel(#label, ref.name());    \
    return true;                                      \
  }

  RESOLVE(funcTypeMap_, Signature)
  RESOLVE(funcMap_, Function)
  RESOLVE(varMap_, Local)
  RESOLVE(globalMap_, Global)
  RESOLVE(tableMap_, Table)
  RESOLVE(memoryMap_, Memory)
  RESOLVE(typeMap_, Type)
  RESOLVE(fieldMap_, Field)

#undef RESOLVE

  bool resolveBranchTarget(AstRef& ref) {
    if (ref.name().empty()) {
      return true;
    }
    for (size_t i = 0, e = targetStack_.length(); i < e; i++) {
      if (targetStack_[e - i - 1] == ref.name()) {
        ref.setIndex(i);
        return true;
      }
    }
    return failResolveLabel("branch target", ref.name());
  }

  bool fail(const char* message) {
    *error_ = JS_smprintf("%s", message);
    return false;
  }
};

}  // end anonymous namespace

static bool ResolveType(Resolver& r, AstValType& vt) {
  if (vt.isResolved()) {
    return true;
  }
  if (!r.resolveType(vt.asRef())) {
    return false;
  }
  vt.resolve();
  return true;
}

static bool ResolveType(Resolver& r, AstExprType& et) {
  if (et.isResolved()) {
    return true;
  }
  if (!ResolveType(r, et.asAstValType())) {
    return false;
  }
  et.resolve();
  return true;
}

static bool ResolveExpr(Resolver& r, AstExpr& expr);

static bool ResolveExprList(Resolver& r, const AstExprVector& v) {
  for (size_t i = 0; i < v.length(); i++) {
    if (!ResolveExpr(r, *v[i])) {
      return false;
    }
  }
  return true;
}

static bool ResolveBlock(Resolver& r, AstBlock& b) {
  if (!r.pushTarget(b.name())) {
    return false;
  }

  if (!ResolveType(r, b.type())) {
    return false;
  }

  if (!ResolveExprList(r, b.exprs())) {
    return false;
  }

  r.popTarget(b.name());
  return true;
}

static bool ResolveDropOperator(Resolver& r, AstDrop& drop) {
  return ResolveExpr(r, drop.value());
}

static bool ResolveBranch(Resolver& r, AstBranch& br) {
  if (!r.resolveBranchTarget(br.target())) {
    return false;
  }

  if (br.maybeValue() && !ResolveExpr(r, *br.maybeValue())) {
    return false;
  }

  if (br.op() == Op::BrIf) {
    if (!ResolveExpr(r, br.cond())) {
      return false;
    }
  }

  return true;
}

static bool ResolveArgs(Resolver& r, const AstExprVector& args) {
  for (AstExpr* arg : args) {
    if (!ResolveExpr(r, *arg)) {
      return false;
    }
  }

  return true;
}

static bool ResolveCall(Resolver& r, AstCall& c) {
  MOZ_ASSERT(c.op() == Op::Call);

  if (!ResolveArgs(r, c.args())) {
    return false;
  }

  if (!r.resolveFunction(c.func())) {
    return false;
  }

  return true;
}

static bool ResolveCallIndirect(Resolver& r, AstCallIndirect& c) {
  if (!ResolveArgs(r, c.args())) {
    return false;
  }

  if (!ResolveExpr(r, *c.index())) {
    return false;
  }

  if (!r.resolveSignature(c.funcType())) {
    return false;
  }

  if (!r.resolveTable(c.targetTable())) {
    return false;
  }

  return true;
}

static bool ResolveFirst(Resolver& r, AstFirst& f) {
  return ResolveExprList(r, f.exprs());
}

static bool ResolveGetLocal(Resolver& r, AstGetLocal& gl) {
  return r.resolveLocal(gl.local());
}

static bool ResolveSetLocal(Resolver& r, AstSetLocal& sl) {
  if (!ResolveExpr(r, sl.value())) {
    return false;
  }

  if (!r.resolveLocal(sl.local())) {
    return false;
  }

  return true;
}

static bool ResolveGetGlobal(Resolver& r, AstGetGlobal& gl) {
  return r.resolveGlobal(gl.global());
}

static bool ResolveSetGlobal(Resolver& r, AstSetGlobal& sl) {
  if (!ResolveExpr(r, sl.value())) {
    return false;
  }

  if (!r.resolveGlobal(sl.global())) {
    return false;
  }

  return true;
}

static bool ResolveTeeLocal(Resolver& r, AstTeeLocal& sl) {
  if (!ResolveExpr(r, sl.value())) {
    return false;
  }

  if (!r.resolveLocal(sl.local())) {
    return false;
  }

  return true;
}

static bool ResolveUnaryOperator(Resolver& r, AstUnaryOperator& b) {
  return ResolveExpr(r, *b.operand());
}

static bool ResolveGrowMemory(Resolver& r, AstGrowMemory& gm) {
  return ResolveExpr(r, *gm.operand());
}

static bool ResolveBinaryOperator(Resolver& r, AstBinaryOperator& b) {
  return ResolveExpr(r, *b.lhs()) && ResolveExpr(r, *b.rhs());
}

static bool ResolveTernaryOperator(Resolver& r, AstTernaryOperator& b) {
  return ResolveExpr(r, *b.op0()) && ResolveExpr(r, *b.op1()) &&
         ResolveExpr(r, *b.op2());
}

static bool ResolveComparisonOperator(Resolver& r, AstComparisonOperator& b) {
  return ResolveExpr(r, *b.lhs()) && ResolveExpr(r, *b.rhs());
}

static bool ResolveConversionOperator(Resolver& r, AstConversionOperator& b) {
  return ResolveExpr(r, *b.operand());
}

static bool ResolveExtraConversionOperator(Resolver& r,
                                           AstExtraConversionOperator& b) {
  return ResolveExpr(r, *b.operand());
}

static bool ResolveIfElse(Resolver& r, AstIf& i) {
  if (!ResolveType(r, i.type())) {
    return false;
  }
  if (!ResolveExpr(r, i.cond())) {
    return false;
  }
  if (!r.pushTarget(i.name())) {
    return false;
  }
  if (!ResolveExprList(r, i.thenExprs())) {
    return false;
  }
  if (i.hasElse()) {
    if (!ResolveExprList(r, i.elseExprs())) {
      return false;
    }
  }
  r.popTarget(i.name());
  return true;
}

static bool ResolveLoadStoreAddress(Resolver& r,
                                    const AstLoadStoreAddress& address) {
  return ResolveExpr(r, address.base());
}

static bool ResolveLoad(Resolver& r, AstLoad& l) {
  return ResolveLoadStoreAddress(r, l.address());
}

static bool ResolveStore(Resolver& r, AstStore& s) {
  return ResolveLoadStoreAddress(r, s.address()) && ResolveExpr(r, s.value());
}

static bool ResolveReturn(Resolver& r, AstReturn& ret) {
  return !ret.maybeExpr() || ResolveExpr(r, *ret.maybeExpr());
}

static bool ResolveBranchTable(Resolver& r, AstBranchTable& bt) {
  if (!r.resolveBranchTarget(bt.def())) {
    return false;
  }

  for (AstRef& elem : bt.table()) {
    if (!r.resolveBranchTarget(elem)) {
      return false;
    }
  }

  if (bt.maybeValue() && !ResolveExpr(r, *bt.maybeValue())) {
    return false;
  }

  return ResolveExpr(r, bt.index());
}

static bool ResolveAtomicCmpXchg(Resolver& r, AstAtomicCmpXchg& s) {
  return ResolveLoadStoreAddress(r, s.address()) &&
         ResolveExpr(r, s.expected()) && ResolveExpr(r, s.replacement());
}

static bool ResolveAtomicLoad(Resolver& r, AstAtomicLoad& l) {
  return ResolveLoadStoreAddress(r, l.address());
}

static bool ResolveAtomicRMW(Resolver& r, AstAtomicRMW& s) {
  return ResolveLoadStoreAddress(r, s.address()) && ResolveExpr(r, s.value());
}

static bool ResolveAtomicStore(Resolver& r, AstAtomicStore& s) {
  return ResolveLoadStoreAddress(r, s.address()) && ResolveExpr(r, s.value());
}

static bool ResolveWait(Resolver& r, AstWait& s) {
  return ResolveLoadStoreAddress(r, s.address()) &&
         ResolveExpr(r, s.expected()) && ResolveExpr(r, s.timeout());
}

static bool ResolveWake(Resolver& r, AstWake& s) {
  return ResolveLoadStoreAddress(r, s.address()) && ResolveExpr(r, s.count());
}

#ifdef ENABLE_WASM_BULKMEM_OPS
static bool ResolveMemOrTableCopy(Resolver& r, AstMemOrTableCopy& s) {
  return ResolveExpr(r, s.dest()) && ResolveExpr(r, s.src()) &&
         ResolveExpr(r, s.len()) && r.resolveTable(s.destTable()) &&
         r.resolveTable(s.srcTable());
}

static bool ResolveMemFill(Resolver& r, AstMemFill& s) {
  return ResolveExpr(r, s.start()) && ResolveExpr(r, s.val()) &&
         ResolveExpr(r, s.len());
}

static bool ResolveMemOrTableInit(Resolver& r, AstMemOrTableInit& s) {
  return ResolveExpr(r, s.dst()) && ResolveExpr(r, s.src()) &&
         ResolveExpr(r, s.len()) && r.resolveTable(s.targetTable());
}
#endif

#ifdef ENABLE_WASM_GENERALIZED_TABLES
static bool ResolveTableGet(Resolver& r, AstTableGet& s) {
  return ResolveExpr(r, s.index()) && r.resolveTable(s.targetTable());
}

static bool ResolveTableGrow(Resolver& r, AstTableGrow& s) {
  return ResolveExpr(r, s.delta()) && ResolveExpr(r, s.initValue()) &&
         r.resolveTable(s.targetTable());
}

static bool ResolveTableSet(Resolver& r, AstTableSet& s) {
  return ResolveExpr(r, s.index()) && ResolveExpr(r, s.value()) &&
         r.resolveTable(s.targetTable());
}

static bool ResolveTableSize(Resolver& r, AstTableSize& s) {
  return r.resolveTable(s.targetTable());
}
#endif

#ifdef ENABLE_WASM_GC
static bool ResolveStructNew(Resolver& r, AstStructNew& s) {
  if (!ResolveArgs(r, s.fieldValues())) {
    return false;
  }

  if (!r.resolveType(s.structType())) {
    return false;
  }

  return true;
}

static bool ResolveStructGet(Resolver& r, AstStructGet& s) {
  if (!r.resolveType(s.structType())) {
    return false;
  }

  if (!r.resolveField(s.fieldName())) {
    return false;
  }

  return ResolveExpr(r, s.ptr());
}

static bool ResolveStructSet(Resolver& r, AstStructSet& s) {
  if (!r.resolveType(s.structType())) {
    return false;
  }

  if (!r.resolveField(s.fieldName())) {
    return false;
  }

  return ResolveExpr(r, s.ptr()) && ResolveExpr(r, s.value());
}

static bool ResolveStructNarrow(Resolver& r, AstStructNarrow& s) {
  if (!ResolveType(r, s.inputStruct())) {
    return false;
  }

  if (!ResolveType(r, s.outputStruct())) {
    return false;
  }

  return ResolveExpr(r, s.ptr());
}
#endif

static bool ResolveRefNull(Resolver& r, AstRefNull& s) { return true; }

static bool ResolveExpr(Resolver& r, AstExpr& expr) {
  switch (expr.kind()) {
    case AstExprKind::Nop:
    case AstExprKind::Pop:
    case AstExprKind::Unreachable:
    case AstExprKind::CurrentMemory:
      return true;
    case AstExprKind::RefNull:
      return ResolveRefNull(r, expr.as<AstRefNull>());
    case AstExprKind::Drop:
      return ResolveDropOperator(r, expr.as<AstDrop>());
    case AstExprKind::BinaryOperator:
      return ResolveBinaryOperator(r, expr.as<AstBinaryOperator>());
    case AstExprKind::Block:
      return ResolveBlock(r, expr.as<AstBlock>());
    case AstExprKind::Branch:
      return ResolveBranch(r, expr.as<AstBranch>());
    case AstExprKind::Call:
      return ResolveCall(r, expr.as<AstCall>());
    case AstExprKind::CallIndirect:
      return ResolveCallIndirect(r, expr.as<AstCallIndirect>());
    case AstExprKind::ComparisonOperator:
      return ResolveComparisonOperator(r, expr.as<AstComparisonOperator>());
    case AstExprKind::Const:
      return true;
    case AstExprKind::ConversionOperator:
      return ResolveConversionOperator(r, expr.as<AstConversionOperator>());
    case AstExprKind::ExtraConversionOperator:
      return ResolveExtraConversionOperator(
          r, expr.as<AstExtraConversionOperator>());
    case AstExprKind::First:
      return ResolveFirst(r, expr.as<AstFirst>());
    case AstExprKind::GetGlobal:
      return ResolveGetGlobal(r, expr.as<AstGetGlobal>());
    case AstExprKind::GetLocal:
      return ResolveGetLocal(r, expr.as<AstGetLocal>());
    case AstExprKind::If:
      return ResolveIfElse(r, expr.as<AstIf>());
    case AstExprKind::Load:
      return ResolveLoad(r, expr.as<AstLoad>());
    case AstExprKind::Return:
      return ResolveReturn(r, expr.as<AstReturn>());
    case AstExprKind::SetGlobal:
      return ResolveSetGlobal(r, expr.as<AstSetGlobal>());
    case AstExprKind::SetLocal:
      return ResolveSetLocal(r, expr.as<AstSetLocal>());
    case AstExprKind::Store:
      return ResolveStore(r, expr.as<AstStore>());
    case AstExprKind::BranchTable:
      return ResolveBranchTable(r, expr.as<AstBranchTable>());
    case AstExprKind::TeeLocal:
      return ResolveTeeLocal(r, expr.as<AstTeeLocal>());
    case AstExprKind::TernaryOperator:
      return ResolveTernaryOperator(r, expr.as<AstTernaryOperator>());
    case AstExprKind::UnaryOperator:
      return ResolveUnaryOperator(r, expr.as<AstUnaryOperator>());
    case AstExprKind::GrowMemory:
      return ResolveGrowMemory(r, expr.as<AstGrowMemory>());
    case AstExprKind::AtomicCmpXchg:
      return ResolveAtomicCmpXchg(r, expr.as<AstAtomicCmpXchg>());
    case AstExprKind::AtomicLoad:
      return ResolveAtomicLoad(r, expr.as<AstAtomicLoad>());
    case AstExprKind::AtomicRMW:
      return ResolveAtomicRMW(r, expr.as<AstAtomicRMW>());
    case AstExprKind::AtomicStore:
      return ResolveAtomicStore(r, expr.as<AstAtomicStore>());
    case AstExprKind::Wait:
      return ResolveWait(r, expr.as<AstWait>());
    case AstExprKind::Wake:
      return ResolveWake(r, expr.as<AstWake>());
#ifdef ENABLE_WASM_BULKMEM_OPS
    case AstExprKind::MemOrTableCopy:
      return ResolveMemOrTableCopy(r, expr.as<AstMemOrTableCopy>());
    case AstExprKind::MemOrTableDrop:
      return true;
    case AstExprKind::MemFill:
      return ResolveMemFill(r, expr.as<AstMemFill>());
    case AstExprKind::MemOrTableInit:
      return ResolveMemOrTableInit(r, expr.as<AstMemOrTableInit>());
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    case AstExprKind::TableGet:
      return ResolveTableGet(r, expr.as<AstTableGet>());
    case AstExprKind::TableGrow:
      return ResolveTableGrow(r, expr.as<AstTableGrow>());
    case AstExprKind::TableSet:
      return ResolveTableSet(r, expr.as<AstTableSet>());
    case AstExprKind::TableSize:
      return ResolveTableSize(r, expr.as<AstTableSize>());
#endif
#ifdef ENABLE_WASM_GC
    case AstExprKind::StructNew:
      return ResolveStructNew(r, expr.as<AstStructNew>());
    case AstExprKind::StructGet:
      return ResolveStructGet(r, expr.as<AstStructGet>());
    case AstExprKind::StructSet:
      return ResolveStructSet(r, expr.as<AstStructSet>());
    case AstExprKind::StructNarrow:
      return ResolveStructNarrow(r, expr.as<AstStructNarrow>());
#endif
  }
  MOZ_CRASH("Bad expr kind");
}

static bool ResolveFunc(Resolver& r, AstFunc& func) {
  r.beginFunc();

  for (AstValType& vt : func.vars()) {
    if (!ResolveType(r, vt)) {
      return false;
    }
  }

  for (size_t i = 0; i < func.locals().length(); i++) {
    if (!r.registerVarName(func.locals()[i], i)) {
      return r.fail("duplicate var");
    }
  }

  for (AstExpr* expr : func.body()) {
    if (!ResolveExpr(r, *expr)) {
      return false;
    }
  }
  return true;
}

static bool ResolveSignature(Resolver& r, AstFuncType& ft) {
  for (AstValType& vt : ft.args()) {
    if (!ResolveType(r, vt)) {
      return false;
    }
  }
  return ResolveType(r, ft.ret());
}

static bool ResolveStruct(Resolver& r, AstStructType& s) {
  for (AstValType& vt : s.fieldTypes()) {
    if (!ResolveType(r, vt)) {
      return false;
    }
  }
  return true;
}

static bool ResolveElemSegment(Resolver& r, AstElemSegment& seg) {
  if (!r.resolveTable(seg.targetTableRef())) return false;
  return true;
}

static bool ResolveModule(LifoAlloc& lifo, AstModule* module,
                          UniqueChars* error) {
  Resolver r(lifo, error);

  size_t numTypes = module->types().length();
  for (size_t i = 0; i < numTypes; i++) {
    AstTypeDef* td = module->types()[i];
    if (td->isFuncType()) {
      AstFuncType* funcType = &td->asFuncType();
      if (!r.registerFuncTypeName(funcType->name(), i)) {
        return r.fail("duplicate signature");
      }
    } else if (td->isStructType()) {
      AstStructType* structType = &td->asStructType();
      if (!r.registerTypeName(structType->name(), i)) {
        return r.fail("duplicate type name");
      }

      size_t numFields = structType->fieldNames().length();
      for (size_t j = 0; j < numFields; j++) {
        const AstName& fieldName = structType->fieldNames()[j];
        if (!r.registerFieldName(fieldName, j)) {
          return r.fail("duplicate field name (must be unique in module)");
        }
      }
    } else {
      MOZ_CRASH("Bad type");
    }
  }

  for (size_t i = 0; i < numTypes; i++) {
    AstTypeDef* td = module->types()[i];
    if (td->isFuncType()) {
      AstFuncType* funcType = &td->asFuncType();
      if (!ResolveSignature(r, *funcType)) {
        return false;
      }
    } else if (td->isStructType()) {
      AstStructType* structType = &td->asStructType();
      if (!ResolveStruct(r, *structType)) {
        return false;
      }
    } else {
      MOZ_CRASH("Bad type");
    }
  }

  size_t lastFuncIndex = 0;
  size_t lastGlobalIndex = 0;
  size_t lastMemoryIndex = 0;
  size_t lastTableIndex = 0;
  for (AstImport* imp : module->imports()) {
    switch (imp->kind()) {
      case DefinitionKind::Function:
        if (!r.registerFuncName(imp->name(), lastFuncIndex++)) {
          return r.fail("duplicate import");
        }
        if (!r.resolveSignature(imp->funcType())) {
          return false;
        }
        break;
      case DefinitionKind::Global:
        if (!r.registerGlobalName(imp->name(), lastGlobalIndex++)) {
          return r.fail("duplicate import");
        }
        if (!ResolveType(r, imp->global().type())) {
          return false;
        }
        break;
      case DefinitionKind::Memory:
        if (!r.registerMemoryName(imp->name(), lastMemoryIndex++)) {
          return r.fail("duplicate import");
        }
        break;
      case DefinitionKind::Table:
        if (!r.registerTableName(imp->name(), lastTableIndex++)) {
          return r.fail("duplicate import");
        }
        break;
    }
  }

  for (AstFunc* func : module->funcs()) {
    if (!r.resolveSignature(func->funcType())) {
      return false;
    }
    if (!r.registerFuncName(func->name(), lastFuncIndex++)) {
      return r.fail("duplicate function");
    }
  }

  for (AstGlobal* global : module->globals()) {
    if (!r.registerGlobalName(global->name(), lastGlobalIndex++)) {
      return r.fail("duplicate import");
    }
    if (!ResolveType(r, global->type())) {
      return false;
    }
    if (global->hasInit() && !ResolveExpr(r, global->init())) {
      return false;
    }
  }

  for (const AstTable& table : module->tables()) {
    if (table.imported) {
      continue;
    }
    if (!r.registerTableName(table.name, lastTableIndex++)) {
      return r.fail("duplicate import");
    }
  }

  for (const AstMemory& memory : module->memories()) {
    if (memory.imported) {
      continue;
    }
    if (!r.registerMemoryName(memory.name, lastMemoryIndex++)) {
      return r.fail("duplicate import");
    }
  }

  for (AstExport* export_ : module->exports()) {
    switch (export_->kind()) {
      case DefinitionKind::Function:
        if (!r.resolveFunction(export_->ref())) {
          return false;
        }
        break;
      case DefinitionKind::Global:
        if (!r.resolveGlobal(export_->ref())) {
          return false;
        }
        break;
      case DefinitionKind::Table:
        if (!r.resolveTable(export_->ref())) {
          return false;
        }
        break;
      case DefinitionKind::Memory:
        if (!r.resolveMemory(export_->ref())) {
          return false;
        }
        break;
    }
  }

  for (AstElemSegment* seg : module->elemSegments()) {
    if (!ResolveElemSegment(r, *seg)) {
      return false;
    }
  }

  for (AstFunc* func : module->funcs()) {
    if (!ResolveFunc(r, *func)) {
      return false;
    }
  }

  if (module->hasStartFunc()) {
    if (!r.resolveFunction(module->startFunc().func())) {
      return false;
    }
  }

  for (AstDataSegment* segment : module->dataSegments()) {
    if (segment->offsetIfActive() &&
        !ResolveExpr(r, *segment->offsetIfActive())) {
      return false;
    }
  }

  for (AstElemSegment* segment : module->elemSegments()) {
    if (segment->offsetIfActive() &&
        !ResolveExpr(r, *segment->offsetIfActive())) {
      return false;
    }
    for (AstRef& ref : segment->elems()) {
      if (!r.resolveFunction(ref)) {
        return false;
      }
    }
  }

  return true;
}

/*****************************************************************************/
// wasm function body serialization

static bool EncodeExpr(Encoder& e, AstExpr& expr);

static bool EncodeExprList(Encoder& e, const AstExprVector& v) {
  for (size_t i = 0; i < v.length(); i++) {
    if (!EncodeExpr(e, *v[i])) {
      return false;
    }
  }
  return true;
}

static bool EncodeBlock(Encoder& e, AstBlock& b) {
  if (!e.writeOp(b.op())) {
    return false;
  }

  if (!e.writeBlockType(b.type().type())) {
    return false;
  }

  if (!EncodeExprList(e, b.exprs())) {
    return false;
  }

  if (!e.writeOp(Op::End)) {
    return false;
  }

  return true;
}

static bool EncodeBranch(Encoder& e, AstBranch& br) {
  MOZ_ASSERT(br.op() == Op::Br || br.op() == Op::BrIf);

  if (br.maybeValue()) {
    if (!EncodeExpr(e, *br.maybeValue())) {
      return false;
    }
  }

  if (br.op() == Op::BrIf) {
    if (!EncodeExpr(e, br.cond())) {
      return false;
    }
  }

  if (!e.writeOp(br.op())) {
    return false;
  }

  if (!e.writeVarU32(br.target().index())) {
    return false;
  }

  return true;
}

static bool EncodeFirst(Encoder& e, AstFirst& f) {
  return EncodeExprList(e, f.exprs());
}

static bool EncodeArgs(Encoder& e, const AstExprVector& args) {
  for (AstExpr* arg : args) {
    if (!EncodeExpr(e, *arg)) {
      return false;
    }
  }

  return true;
}

static bool EncodeCall(Encoder& e, AstCall& c) {
  if (!EncodeArgs(e, c.args())) {
    return false;
  }

  if (!e.writeOp(c.op())) {
    return false;
  }

  if (!e.writeVarU32(c.func().index())) {
    return false;
  }

  return true;
}

static bool EncodeOneTableIndex(Encoder& e, uint32_t index) {
  if (index) {
    return e.writeVarU32(uint32_t(MemoryTableFlags::HasTableIndex)) &&
           e.writeVarU32(index);
  }
  return e.writeVarU32(uint32_t(MemoryTableFlags::Default));
}

static bool EncodeCallIndirect(Encoder& e, AstCallIndirect& c) {
  if (!EncodeArgs(e, c.args())) {
    return false;
  }

  if (!EncodeExpr(e, *c.index())) {
    return false;
  }

  if (!e.writeOp(Op::CallIndirect)) {
    return false;
  }

  if (!e.writeVarU32(c.funcType().index())) {
    return false;
  }

  return EncodeOneTableIndex(e, c.targetTable().index());
}

static bool EncodeConst(Encoder& e, AstConst& c) {
  switch (c.val().type().code()) {
    case ValType::I32:
      return e.writeOp(Op::I32Const) && e.writeVarS32(c.val().i32());
    case ValType::I64:
      return e.writeOp(Op::I64Const) && e.writeVarS64(c.val().i64());
    case ValType::F32:
      return e.writeOp(Op::F32Const) && e.writeFixedF32(c.val().f32());
    case ValType::F64:
      return e.writeOp(Op::F64Const) && e.writeFixedF64(c.val().f64());
    default:
      break;
  }
  MOZ_CRASH("Bad value type");
}

static bool EncodeDrop(Encoder& e, AstDrop& drop) {
  return EncodeExpr(e, drop.value()) && e.writeOp(Op::Drop);
}

static bool EncodeGetLocal(Encoder& e, AstGetLocal& gl) {
  return e.writeOp(Op::GetLocal) && e.writeVarU32(gl.local().index());
}

static bool EncodeSetLocal(Encoder& e, AstSetLocal& sl) {
  return EncodeExpr(e, sl.value()) && e.writeOp(Op::SetLocal) &&
         e.writeVarU32(sl.local().index());
}

static bool EncodeTeeLocal(Encoder& e, AstTeeLocal& sl) {
  return EncodeExpr(e, sl.value()) && e.writeOp(Op::TeeLocal) &&
         e.writeVarU32(sl.local().index());
}

static bool EncodeGetGlobal(Encoder& e, AstGetGlobal& gg) {
  return e.writeOp(Op::GetGlobal) && e.writeVarU32(gg.global().index());
}

static bool EncodeSetGlobal(Encoder& e, AstSetGlobal& sg) {
  return EncodeExpr(e, sg.value()) && e.writeOp(Op::SetGlobal) &&
         e.writeVarU32(sg.global().index());
}

static bool EncodeUnaryOperator(Encoder& e, AstUnaryOperator& b) {
  return EncodeExpr(e, *b.operand()) && e.writeOp(b.op());
}

static bool EncodeBinaryOperator(Encoder& e, AstBinaryOperator& b) {
  return EncodeExpr(e, *b.lhs()) && EncodeExpr(e, *b.rhs()) &&
         e.writeOp(b.op());
}

static bool EncodeTernaryOperator(Encoder& e, AstTernaryOperator& b) {
  return EncodeExpr(e, *b.op0()) && EncodeExpr(e, *b.op1()) &&
         EncodeExpr(e, *b.op2()) && e.writeOp(b.op());
}

static bool EncodeComparisonOperator(Encoder& e, AstComparisonOperator& b) {
  return EncodeExpr(e, *b.lhs()) && EncodeExpr(e, *b.rhs()) &&
         e.writeOp(b.op());
}

static bool EncodeConversionOperator(Encoder& e, AstConversionOperator& b) {
  return EncodeExpr(e, *b.operand()) && e.writeOp(b.op());
}

static bool EncodeExtraConversionOperator(Encoder& e,
                                          AstExtraConversionOperator& b) {
  return EncodeExpr(e, *b.operand()) && e.writeOp(b.op());
}

static bool EncodeIf(Encoder& e, AstIf& i) {
  if (!EncodeExpr(e, i.cond()) || !e.writeOp(Op::If)) {
    return false;
  }

  if (!e.writeBlockType(i.type().type())) {
    return false;
  }

  if (!EncodeExprList(e, i.thenExprs())) {
    return false;
  }

  if (i.hasElse()) {
    if (!e.writeOp(Op::Else)) {
      return false;
    }
    if (!EncodeExprList(e, i.elseExprs())) {
      return false;
    }
  }

  return e.writeOp(Op::End);
}

static bool EncodeLoadStoreAddress(Encoder& e,
                                   const AstLoadStoreAddress& address) {
  return EncodeExpr(e, address.base());
}

static bool EncodeLoadStoreFlags(Encoder& e,
                                 const AstLoadStoreAddress& address) {
  return e.writeVarU32(address.flags()) && e.writeVarU32(address.offset());
}

static bool EncodeLoad(Encoder& e, AstLoad& l) {
  return EncodeLoadStoreAddress(e, l.address()) && e.writeOp(l.op()) &&
         EncodeLoadStoreFlags(e, l.address());
}

static bool EncodeStore(Encoder& e, AstStore& s) {
  return EncodeLoadStoreAddress(e, s.address()) && EncodeExpr(e, s.value()) &&
         e.writeOp(s.op()) && EncodeLoadStoreFlags(e, s.address());
}

static bool EncodeReturn(Encoder& e, AstReturn& r) {
  if (r.maybeExpr()) {
    if (!EncodeExpr(e, *r.maybeExpr())) {
      return false;
    }
  }

  if (!e.writeOp(Op::Return)) {
    return false;
  }

  return true;
}

static bool EncodeBranchTable(Encoder& e, AstBranchTable& bt) {
  if (bt.maybeValue()) {
    if (!EncodeExpr(e, *bt.maybeValue())) {
      return false;
    }
  }

  if (!EncodeExpr(e, bt.index())) {
    return false;
  }

  if (!e.writeOp(Op::BrTable)) {
    return false;
  }

  if (!e.writeVarU32(bt.table().length())) {
    return false;
  }

  for (const AstRef& elem : bt.table()) {
    if (!e.writeVarU32(elem.index())) {
      return false;
    }
  }

  if (!e.writeVarU32(bt.def().index())) {
    return false;
  }

  return true;
}

static bool EncodeCurrentMemory(Encoder& e, AstCurrentMemory& cm) {
  if (!e.writeOp(Op::CurrentMemory)) {
    return false;
  }

  if (!e.writeVarU32(uint32_t(MemoryTableFlags::Default))) {
    return false;
  }

  return true;
}

static bool EncodeGrowMemory(Encoder& e, AstGrowMemory& gm) {
  if (!EncodeExpr(e, *gm.operand())) {
    return false;
  }

  if (!e.writeOp(Op::GrowMemory)) {
    return false;
  }

  if (!e.writeVarU32(uint32_t(MemoryTableFlags::Default))) {
    return false;
  }

  return true;
}

static bool EncodeAtomicCmpXchg(Encoder& e, AstAtomicCmpXchg& s) {
  return EncodeLoadStoreAddress(e, s.address()) &&
         EncodeExpr(e, s.expected()) && EncodeExpr(e, s.replacement()) &&
         e.writeOp(s.op()) && EncodeLoadStoreFlags(e, s.address());
}

static bool EncodeAtomicLoad(Encoder& e, AstAtomicLoad& l) {
  return EncodeLoadStoreAddress(e, l.address()) && e.writeOp(l.op()) &&
         EncodeLoadStoreFlags(e, l.address());
}

static bool EncodeAtomicRMW(Encoder& e, AstAtomicRMW& s) {
  return EncodeLoadStoreAddress(e, s.address()) && EncodeExpr(e, s.value()) &&
         e.writeOp(s.op()) && EncodeLoadStoreFlags(e, s.address());
}

static bool EncodeAtomicStore(Encoder& e, AstAtomicStore& s) {
  return EncodeLoadStoreAddress(e, s.address()) && EncodeExpr(e, s.value()) &&
         e.writeOp(s.op()) && EncodeLoadStoreFlags(e, s.address());
}

static bool EncodeWait(Encoder& e, AstWait& s) {
  return EncodeLoadStoreAddress(e, s.address()) &&
         EncodeExpr(e, s.expected()) && EncodeExpr(e, s.timeout()) &&
         e.writeOp(s.op()) && EncodeLoadStoreFlags(e, s.address());
}

static bool EncodeWake(Encoder& e, AstWake& s) {
  return EncodeLoadStoreAddress(e, s.address()) && EncodeExpr(e, s.count()) &&
         e.writeOp(ThreadOp::Wake) && EncodeLoadStoreFlags(e, s.address());
}

#ifdef ENABLE_WASM_BULKMEM_OPS
static bool EncodeMemOrTableCopy(Encoder& e, AstMemOrTableCopy& s) {
  bool result = EncodeExpr(e, s.dest()) && EncodeExpr(e, s.src()) &&
                EncodeExpr(e, s.len()) &&
                e.writeOp(s.isMem() ? MiscOp::MemCopy : MiscOp::TableCopy);
  if (s.destTable().index() == 0 && s.srcTable().index() == 0) {
    result = result && e.writeVarU32(uint32_t(MemoryTableFlags::Default));
  } else {
    result = result &&
             e.writeVarU32(uint32_t(MemoryTableFlags::HasTableIndex)) &&
             e.writeVarU32(s.destTable().index()) &&
             e.writeVarU32(s.srcTable().index());
  }
  return result;
}

static bool EncodeMemOrTableDrop(Encoder& e, AstMemOrTableDrop& s) {
  return e.writeOp(s.isMem() ? MiscOp::MemDrop : MiscOp::TableDrop) &&
         e.writeVarU32(s.segIndex());
}

static bool EncodeMemFill(Encoder& e, AstMemFill& s) {
  return EncodeExpr(e, s.start()) && EncodeExpr(e, s.val()) &&
         EncodeExpr(e, s.len()) && e.writeOp(MiscOp::MemFill) &&
         e.writeVarU32(uint32_t(MemoryTableFlags::Default));
}

static bool EncodeMemOrTableInit(Encoder& e, AstMemOrTableInit& s) {
  return EncodeExpr(e, s.dst()) && EncodeExpr(e, s.src()) &&
         EncodeExpr(e, s.len()) &&
         e.writeOp(s.isMem() ? MiscOp::MemInit : MiscOp::TableInit) &&
         EncodeOneTableIndex(e, s.targetTable().index()) &&
         e.writeVarU32(s.segIndex());
}
#endif

#ifdef ENABLE_WASM_GENERALIZED_TABLES
static bool EncodeTableGet(Encoder& e, AstTableGet& s) {
  return EncodeExpr(e, s.index()) && e.writeOp(MiscOp::TableGet) &&
         EncodeOneTableIndex(e, s.targetTable().index());
}

static bool EncodeTableGrow(Encoder& e, AstTableGrow& s) {
  return EncodeExpr(e, s.delta()) && EncodeExpr(e, s.initValue()) &&
         e.writeOp(MiscOp::TableGrow) &&
         EncodeOneTableIndex(e, s.targetTable().index());
}

static bool EncodeTableSet(Encoder& e, AstTableSet& s) {
  return EncodeExpr(e, s.index()) && EncodeExpr(e, s.value()) &&
         e.writeOp(MiscOp::TableSet) &&
         EncodeOneTableIndex(e, s.targetTable().index());
}

static bool EncodeTableSize(Encoder& e, AstTableSize& s) {
  return e.writeOp(MiscOp::TableSize) &&
         EncodeOneTableIndex(e, s.targetTable().index());
}
#endif

#ifdef ENABLE_WASM_GC
static bool EncodeStructNew(Encoder& e, AstStructNew& s) {
  if (!EncodeArgs(e, s.fieldValues())) {
    return false;
  }

  if (!e.writeOp(MiscOp::StructNew)) {
    return false;
  }

  if (!e.writeVarU32(s.structType().index())) {
    return false;
  }

  return true;
}

static bool EncodeStructGet(Encoder& e, AstStructGet& s) {
  if (!EncodeExpr(e, s.ptr())) {
    return false;
  }
  if (!e.writeOp(MiscOp::StructGet)) {
    return false;
  }
  if (!e.writeVarU32(s.structType().index())) {
    return false;
  }
  if (!e.writeVarU32(s.fieldName().index())) {
    return false;
  }
  return true;
}

static bool EncodeStructSet(Encoder& e, AstStructSet& s) {
  if (!EncodeExpr(e, s.ptr())) {
    return false;
  }
  if (!EncodeExpr(e, s.value())) {
    return false;
  }
  if (!e.writeOp(MiscOp::StructSet)) {
    return false;
  }
  if (!e.writeVarU32(s.structType().index())) {
    return false;
  }
  if (!e.writeVarU32(s.fieldName().index())) {
    return false;
  }
  return true;
}

static bool EncodeStructNarrow(Encoder& e, AstStructNarrow& s) {
  if (!EncodeExpr(e, s.ptr())) {
    return false;
  }
  if (!e.writeOp(MiscOp::StructNarrow)) {
    return false;
  }
  if (!e.writeValType(s.inputStruct().type())) {
    return false;
  }
  if (!e.writeValType(s.outputStruct().type())) {
    return false;
  }
  return true;
}
#endif

static bool EncodeRefNull(Encoder& e, AstRefNull& s) {
  return e.writeOp(Op::RefNull);
}

static bool EncodeExpr(Encoder& e, AstExpr& expr) {
  switch (expr.kind()) {
    case AstExprKind::Pop:
      return true;
    case AstExprKind::Nop:
      return e.writeOp(Op::Nop);
    case AstExprKind::Unreachable:
      return e.writeOp(Op::Unreachable);
    case AstExprKind::RefNull:
      return EncodeRefNull(e, expr.as<AstRefNull>());
    case AstExprKind::BinaryOperator:
      return EncodeBinaryOperator(e, expr.as<AstBinaryOperator>());
    case AstExprKind::Block:
      return EncodeBlock(e, expr.as<AstBlock>());
    case AstExprKind::Branch:
      return EncodeBranch(e, expr.as<AstBranch>());
    case AstExprKind::Call:
      return EncodeCall(e, expr.as<AstCall>());
    case AstExprKind::CallIndirect:
      return EncodeCallIndirect(e, expr.as<AstCallIndirect>());
    case AstExprKind::ComparisonOperator:
      return EncodeComparisonOperator(e, expr.as<AstComparisonOperator>());
    case AstExprKind::Const:
      return EncodeConst(e, expr.as<AstConst>());
    case AstExprKind::ConversionOperator:
      return EncodeConversionOperator(e, expr.as<AstConversionOperator>());
    case AstExprKind::Drop:
      return EncodeDrop(e, expr.as<AstDrop>());
    case AstExprKind::ExtraConversionOperator:
      return EncodeExtraConversionOperator(
          e, expr.as<AstExtraConversionOperator>());
    case AstExprKind::First:
      return EncodeFirst(e, expr.as<AstFirst>());
    case AstExprKind::GetLocal:
      return EncodeGetLocal(e, expr.as<AstGetLocal>());
    case AstExprKind::GetGlobal:
      return EncodeGetGlobal(e, expr.as<AstGetGlobal>());
    case AstExprKind::If:
      return EncodeIf(e, expr.as<AstIf>());
    case AstExprKind::Load:
      return EncodeLoad(e, expr.as<AstLoad>());
    case AstExprKind::Return:
      return EncodeReturn(e, expr.as<AstReturn>());
    case AstExprKind::SetLocal:
      return EncodeSetLocal(e, expr.as<AstSetLocal>());
    case AstExprKind::TeeLocal:
      return EncodeTeeLocal(e, expr.as<AstTeeLocal>());
    case AstExprKind::SetGlobal:
      return EncodeSetGlobal(e, expr.as<AstSetGlobal>());
    case AstExprKind::Store:
      return EncodeStore(e, expr.as<AstStore>());
    case AstExprKind::BranchTable:
      return EncodeBranchTable(e, expr.as<AstBranchTable>());
    case AstExprKind::TernaryOperator:
      return EncodeTernaryOperator(e, expr.as<AstTernaryOperator>());
    case AstExprKind::UnaryOperator:
      return EncodeUnaryOperator(e, expr.as<AstUnaryOperator>());
    case AstExprKind::CurrentMemory:
      return EncodeCurrentMemory(e, expr.as<AstCurrentMemory>());
    case AstExprKind::GrowMemory:
      return EncodeGrowMemory(e, expr.as<AstGrowMemory>());
    case AstExprKind::AtomicCmpXchg:
      return EncodeAtomicCmpXchg(e, expr.as<AstAtomicCmpXchg>());
    case AstExprKind::AtomicLoad:
      return EncodeAtomicLoad(e, expr.as<AstAtomicLoad>());
    case AstExprKind::AtomicRMW:
      return EncodeAtomicRMW(e, expr.as<AstAtomicRMW>());
    case AstExprKind::AtomicStore:
      return EncodeAtomicStore(e, expr.as<AstAtomicStore>());
    case AstExprKind::Wait:
      return EncodeWait(e, expr.as<AstWait>());
    case AstExprKind::Wake:
      return EncodeWake(e, expr.as<AstWake>());
#ifdef ENABLE_WASM_BULKMEM_OPS
    case AstExprKind::MemOrTableCopy:
      return EncodeMemOrTableCopy(e, expr.as<AstMemOrTableCopy>());
    case AstExprKind::MemOrTableDrop:
      return EncodeMemOrTableDrop(e, expr.as<AstMemOrTableDrop>());
    case AstExprKind::MemFill:
      return EncodeMemFill(e, expr.as<AstMemFill>());
    case AstExprKind::MemOrTableInit:
      return EncodeMemOrTableInit(e, expr.as<AstMemOrTableInit>());
#endif
#ifdef ENABLE_WASM_GENERALIZED_TABLES
    case AstExprKind::TableGet:
      return EncodeTableGet(e, expr.as<AstTableGet>());
    case AstExprKind::TableGrow:
      return EncodeTableGrow(e, expr.as<AstTableGrow>());
    case AstExprKind::TableSet:
      return EncodeTableSet(e, expr.as<AstTableSet>());
    case AstExprKind::TableSize:
      return EncodeTableSize(e, expr.as<AstTableSize>());
#endif
#ifdef ENABLE_WASM_GC
    case AstExprKind::StructNew:
      return EncodeStructNew(e, expr.as<AstStructNew>());
    case AstExprKind::StructGet:
      return EncodeStructGet(e, expr.as<AstStructGet>());
    case AstExprKind::StructSet:
      return EncodeStructSet(e, expr.as<AstStructSet>());
    case AstExprKind::StructNarrow:
      return EncodeStructNarrow(e, expr.as<AstStructNarrow>());
#endif
  }
  MOZ_CRASH("Bad expr kind");
}

/*****************************************************************************/
// wasm AST binary serialization

#ifdef ENABLE_WASM_GC
static bool EncodeGcFeatureOptInSection(Encoder& e, AstModule& module) {
  uint32_t optInVersion = module.gcFeatureOptIn();
  if (!optInVersion) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::GcFeatureOptIn, &offset)) {
    return false;
  }

  if (!e.writeVarU32(optInVersion)) {
    return false;
  }

  e.finishSection(offset);
  return true;
}
#endif

static bool EncodeTypeSection(Encoder& e, AstModule& module) {
  if (module.types().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Type, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.types().length())) {
    return false;
  }

  for (AstTypeDef* td : module.types()) {
    if (td->isFuncType()) {
      AstFuncType* funcType = &td->asFuncType();
      if (!e.writeVarU32(uint32_t(TypeCode::Func))) {
        return false;
      }

      if (!e.writeVarU32(funcType->args().length())) {
        return false;
      }

      for (AstValType vt : funcType->args()) {
        if (!e.writeValType(vt.type())) {
          return false;
        }
      }

      if (!e.writeVarU32(!IsVoid(funcType->ret().type()))) {
        return false;
      }

      if (!IsVoid(funcType->ret().type())) {
        if (!e.writeValType(NonVoidToValType(funcType->ret().type()))) {
          return false;
        }
      }
    } else if (td->isStructType()) {
      AstStructType* st = &td->asStructType();
      if (!e.writeVarU32(uint32_t(TypeCode::Struct))) {
        return false;
      }

      if (!e.writeVarU32(st->fieldTypes().length())) {
        return false;
      }

      const AstValTypeVector& fieldTypes = st->fieldTypes();
      const AstBoolVector& fieldMutables = st->fieldMutability();
      for (uint32_t i = 0; i < fieldTypes.length(); i++) {
        if (!e.writeFixedU8(fieldMutables[i] ? uint8_t(FieldFlags::Mutable)
                                             : 0)) {
          return false;
        }
        if (!e.writeValType(fieldTypes[i].type())) {
          return false;
        }
      }
    } else {
      MOZ_CRASH("Bad type");
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeFunctionSection(Encoder& e, AstModule& module) {
  if (module.funcs().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Function, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.funcs().length())) {
    return false;
  }

  for (AstFunc* func : module.funcs()) {
    if (!e.writeVarU32(func->funcType().index())) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeBytes(Encoder& e, AstName wasmName) {
  TwoByteChars range(wasmName.begin(), wasmName.length());
  UniqueChars utf8(JS::CharsToNewUTF8CharsZ(nullptr, range).c_str());
  return utf8 && e.writeBytes(utf8.get(), strlen(utf8.get()));
}

static bool EncodeLimits(Encoder& e, const Limits& limits) {
  uint32_t flags = limits.maximum ? uint32_t(MemoryTableFlags::HasMaximum)
                                  : uint32_t(MemoryTableFlags::Default);
  if (limits.shared == Shareable::True) {
    flags |= uint32_t(MemoryTableFlags::IsShared);
  }

  if (!e.writeVarU32(flags)) {
    return false;
  }

  if (!e.writeVarU32(limits.initial)) {
    return false;
  }

  if (limits.maximum) {
    if (!e.writeVarU32(*limits.maximum)) {
      return false;
    }
  }

  return true;
}

static bool EncodeTableLimits(Encoder& e, const Limits& limits,
                              TableKind tableKind) {
  switch (tableKind) {
    case TableKind::AnyFunction:
      if (!e.writeVarU32(uint32_t(TypeCode::AnyFunc))) {
        return false;
      }
      break;
    case TableKind::AnyRef:
      if (!e.writeVarU32(uint32_t(TypeCode::AnyRef))) {
        return false;
      }
      break;
    default:
      MOZ_CRASH("Unexpected table kind");
  }

  return EncodeLimits(e, limits);
}

static bool EncodeGlobalType(Encoder& e, const AstGlobal* global) {
  return e.writeValType(global->type()) &&
         e.writeVarU32(global->isMutable()
                           ? uint32_t(GlobalTypeImmediate::IsMutable)
                           : 0);
}

static bool EncodeImport(Encoder& e, AstImport& imp) {
  if (!EncodeBytes(e, imp.module())) {
    return false;
  }

  if (!EncodeBytes(e, imp.field())) {
    return false;
  }

  if (!e.writeVarU32(uint32_t(imp.kind()))) {
    return false;
  }

  switch (imp.kind()) {
    case DefinitionKind::Function:
      if (!e.writeVarU32(imp.funcType().index())) {
        return false;
      }
      break;
    case DefinitionKind::Global:
      MOZ_ASSERT(!imp.global().hasInit());
      if (!EncodeGlobalType(e, &imp.global())) {
        return false;
      }
      break;
    case DefinitionKind::Table:
      if (!EncodeTableLimits(e, imp.limits(), imp.tableKind())) {
        return false;
      }
      break;
    case DefinitionKind::Memory:
      if (!EncodeLimits(e, imp.limits())) {
        return false;
      }
      break;
  }

  return true;
}

static bool EncodeImportSection(Encoder& e, AstModule& module) {
  if (module.imports().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Import, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.imports().length())) {
    return false;
  }

  for (AstImport* imp : module.imports()) {
    if (!EncodeImport(e, *imp)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeMemorySection(Encoder& e, AstModule& module) {
  size_t numOwnMemories = 0;
  for (const AstMemory& memory : module.memories()) {
    if (!memory.imported) {
      numOwnMemories++;
    }
  }

  if (!numOwnMemories) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Memory, &offset)) {
    return false;
  }

  if (!e.writeVarU32(numOwnMemories)) {
    return false;
  }

  for (const AstMemory& memory : module.memories()) {
    if (memory.imported) {
      continue;
    }
    if (!EncodeLimits(e, memory.limits)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeGlobalSection(Encoder& e, AstModule& module) {
  if (!module.globals().length()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Global, &offset)) {
    return false;
  }

  const AstGlobalVector& globals = module.globals();

  if (!e.writeVarU32(globals.length())) {
    return false;
  }

  for (const AstGlobal* global : globals) {
    MOZ_ASSERT(global->hasInit());
    if (!EncodeGlobalType(e, global)) {
      return false;
    }
    if (!EncodeExpr(e, global->init())) {
      return false;
    }
    if (!e.writeOp(Op::End)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeExport(Encoder& e, AstExport& exp) {
  if (!EncodeBytes(e, exp.name())) {
    return false;
  }

  if (!e.writeVarU32(uint32_t(exp.kind()))) {
    return false;
  }

  if (!e.writeVarU32(exp.ref().index())) {
    return false;
  }

  return true;
}

static bool EncodeExportSection(Encoder& e, AstModule& module) {
  uint32_t numExports = module.exports().length();
  if (!numExports) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Export, &offset)) {
    return false;
  }

  if (!e.writeVarU32(numExports)) {
    return false;
  }

  for (AstExport* exp : module.exports()) {
    if (!EncodeExport(e, *exp)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeTableSection(Encoder& e, AstModule& module) {
  size_t numOwnTables = 0;
  for (const AstTable& table : module.tables()) {
    if (!table.imported) {
      numOwnTables++;
    }
  }

  if (!numOwnTables) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Table, &offset)) {
    return false;
  }

  if (!e.writeVarU32(numOwnTables)) {
    return false;
  }

  for (const AstTable& table : module.tables()) {
    if (table.imported) {
      continue;
    }
    if (!EncodeTableLimits(e, table.limits, table.tableKind)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeFunctionBody(Encoder& e, Uint32Vector* offsets,
                               AstFunc& func) {
  size_t bodySizeAt;
  if (!e.writePatchableVarU32(&bodySizeAt)) {
    return false;
  }

  size_t beforeBody = e.currentOffset();

  ValTypeVector varTypes;
  for (const AstValType& vt : func.vars()) {
    if (!varTypes.append(vt.type())) {
      return false;
    }
  }
  if (!EncodeLocalEntries(e, varTypes)) {
    return false;
  }

  for (AstExpr* expr : func.body()) {
    if (!offsets->append(e.currentOffset())) {
      return false;
    }
    if (!EncodeExpr(e, *expr)) {
      return false;
    }
  }

  if (!offsets->append(e.currentOffset())) {
    return false;
  }
  if (!e.writeOp(Op::End)) {
    return false;
  }

  e.patchVarU32(bodySizeAt, e.currentOffset() - beforeBody);
  return true;
}

static bool EncodeStartSection(Encoder& e, AstModule& module) {
  if (!module.hasStartFunc()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Start, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.startFunc().func().index())) {
    return false;
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeCodeSection(Encoder& e, Uint32Vector* offsets,
                              AstModule& module) {
  if (module.funcs().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Code, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.funcs().length())) {
    return false;
  }

  for (AstFunc* func : module.funcs()) {
    if (!EncodeFunctionBody(e, offsets, *func)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeDestinationOffsetOrFlags(Encoder& e, uint32_t index,
                                           AstExpr* offsetIfActive) {
  if (offsetIfActive) {
    // In the MVP, the following VarU32 is the table or linear memory index
    // and it must be zero.  In the bulk-mem-ops proposal, it is repurposed
    // as a flag field, and if the index is not zero it must be present.
    if (index) {
      if (!e.writeVarU32(uint32_t(InitializerKind::ActiveWithIndex)) ||
          !e.writeVarU32(index)) {
        return false;
      }
    } else {
      if (!e.writeVarU32(uint32_t(InitializerKind::Active))) {
        return false;
      }
    }
    if (!EncodeExpr(e, *offsetIfActive)) {
      return false;
    }
    if (!e.writeOp(Op::End)) {
      return false;
    }
  } else {
    if (!e.writeVarU32(uint32_t(InitializerKind::Passive))) {
      return false;
    }
  }

  return true;
}

static bool EncodeDataSegment(Encoder& e, const AstDataSegment& segment) {
  if (!EncodeDestinationOffsetOrFlags(e, 0, segment.offsetIfActive())) {
    return false;
  }

  size_t totalLength = 0;
  for (const AstName& fragment : segment.fragments()) {
    totalLength += fragment.length();
  }

  Vector<uint8_t, 0, SystemAllocPolicy> bytes;
  if (!bytes.reserve(totalLength)) {
    return false;
  }

  for (const AstName& fragment : segment.fragments()) {
    const char16_t* cur = fragment.begin();
    const char16_t* end = fragment.end();
    while (cur != end) {
      uint8_t byte;
      MOZ_ALWAYS_TRUE(ConsumeTextByte(&cur, end, &byte));
      bytes.infallibleAppend(byte);
    }
  }

  return e.writeBytes(bytes.begin(), bytes.length());
}

static bool EncodeDataSection(Encoder& e, AstModule& module) {
  if (module.dataSegments().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Data, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.dataSegments().length())) {
    return false;
  }

  for (AstDataSegment* segment : module.dataSegments()) {
    if (!EncodeDataSegment(e, *segment)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeElemSegment(Encoder& e, AstElemSegment& segment) {
  if (!EncodeDestinationOffsetOrFlags(e, segment.targetTable().index(),
                                      segment.offsetIfActive())) {
    return false;
  }

  if (!e.writeVarU32(segment.elems().length())) {
    return false;
  }

  for (const AstRef& elem : segment.elems()) {
    if (!e.writeVarU32(elem.index())) {
      return false;
    }
  }

  return true;
}

static bool EncodeElemSection(Encoder& e, AstModule& module) {
  if (module.elemSegments().empty()) {
    return true;
  }

  size_t offset;
  if (!e.startSection(SectionId::Elem, &offset)) {
    return false;
  }

  if (!e.writeVarU32(module.elemSegments().length())) {
    return false;
  }

  for (AstElemSegment* segment : module.elemSegments()) {
    if (!EncodeElemSegment(e, *segment)) {
      return false;
    }
  }

  e.finishSection(offset);
  return true;
}

static bool EncodeModule(AstModule& module, Uint32Vector* offsets,
                         Bytes* bytes) {
  Encoder e(*bytes);

  if (!e.writeFixedU32(MagicNumber)) {
    return false;
  }

  if (!e.writeFixedU32(EncodingVersion)) {
    return false;
  }

#ifdef ENABLE_WASM_GC
  if (!EncodeGcFeatureOptInSection(e, module)) {
    return false;
  }
#endif

  if (!EncodeTypeSection(e, module)) {
    return false;
  }

  if (!EncodeImportSection(e, module)) {
    return false;
  }

  if (!EncodeFunctionSection(e, module)) {
    return false;
  }

  if (!EncodeTableSection(e, module)) {
    return false;
  }

  if (!EncodeMemorySection(e, module)) {
    return false;
  }

  if (!EncodeGlobalSection(e, module)) {
    return false;
  }

  if (!EncodeExportSection(e, module)) {
    return false;
  }

  if (!EncodeStartSection(e, module)) {
    return false;
  }

  if (!EncodeElemSection(e, module)) {
    return false;
  }

  if (!EncodeCodeSection(e, offsets, module)) {
    return false;
  }

  if (!EncodeDataSection(e, module)) {
    return false;
  }

  return true;
}

static bool EncodeBinaryModule(const AstModule& module, Bytes* bytes) {
  Encoder e(*bytes);

  const AstDataSegmentVector& dataSegments = module.dataSegments();
  MOZ_ASSERT(dataSegments.length() == 1);

  for (const AstName& fragment : dataSegments[0]->fragments()) {
    const char16_t* cur = fragment.begin();
    const char16_t* end = fragment.end();
    while (cur != end) {
      uint8_t byte;
      MOZ_ALWAYS_TRUE(ConsumeTextByte(&cur, end, &byte));
      if (!e.writeFixedU8(byte)) {
        return false;
      }
    }
  }

  return true;
}

/*****************************************************************************/

bool wasm::TextToBinary(const char16_t* text, uintptr_t stackLimit,
                        Bytes* bytes, Uint32Vector* offsets,
                        UniqueChars* error) {
  LifoAlloc lifo(AST_LIFO_DEFAULT_CHUNK_SIZE);

  bool binary = false;
  AstModule* module = ParseModule(text, stackLimit, lifo, error, &binary);
  if (!module) {
    return false;
  }

  if (binary) {
    return EncodeBinaryModule(*module, bytes);
  }

  if (!ResolveModule(lifo, module, error)) {
    return false;
  }

  return EncodeModule(*module, offsets, bytes);
}
