/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_ExecutionTracer_h
#define debugger_ExecutionTracer_h

#include "mozilla/Assertions.h"      // MOZ_DIAGNOSTIC_ASSERT, MOZ_ASSERT
#include "mozilla/MathAlgorithms.h"  // mozilla::IsPowerOfTwo
#include "mozilla/Maybe.h"  // mozilla::Maybe, mozilla::Some, mozilla::Nothing
#include "mozilla/Span.h"

#include <limits>    // std::numeric_limits
#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint16_t

#include "js/CharacterEncoding.h"  // JS::UTF8Chars
#include "js/RootingAPI.h"         // JS::Rooted
#include "js/Utility.h"            // js_malloc, js_free
#include "js/Value.h"              // JS::Value
#include "vm/JSContext.h"          // JSContext
#include "vm/Stack.h"              // js::AbstractFramePtr

namespace js {

enum class TracerStringEncoding {
  Latin1,
  TwoByte,
  UTF8,
};

// TODO: it should be noted that part of this design is informed by the fact
// that it evolved from a prototype which wrote this data from a content
// process and read it from the parent process, allowing the parent process to
// display the trace in real time as the program executes. Bug 1910182 tracks
// the next steps for making that prototype a reality.
template <size_t BUFFER_SIZE>
class TracingBuffer {
  static_assert(mozilla::IsPowerOfTwo(BUFFER_SIZE));

  // BUFFER_SIZE is the size of the underlying ring buffer, and BUFFER_MASK
  // masks off indices into it in order to wrap around
  static const size_t BUFFER_MASK = BUFFER_SIZE - 1;

  // The entry header is just a u16 that holds the size of the entry in bytes.
  // This is used for asserting the integrity of the data as well as for
  // skipping the read head forward if it's going to be overwritten by the
  // write head
  static const size_t ENTRY_HEADER_SIZE = sizeof(uint16_t);

  // The underlying ring buffer
  uint8_t* buffer_ = nullptr;

  // NOTE: The following u64s are unwrapped indices into the ring buffer, so
  // they must always be masked off with BUFFER_MASK before using them to
  // access buffer_:

  // Represents how much has been written into the ring buffer and is ready
  // for reading
  uint64_t writeHead_ = 0;

  // Represents how much has been read from the ring buffer
  uint64_t readHead_ = 0;

  // When not equal to writeHead_, this represents unfinished write progress
  // into the buffer. After each entry successfully finished writing,
  // writeHead_ is set to this value
  uint64_t uncommittedWriteHead_ = 0;

  // Similar to uncommittedWriteHead_, but for the purposes of reading
  uint64_t uncommittedReadHead_ = 0;

 public:
  ~TracingBuffer() {
    if (buffer_) {
      js_free(buffer_);
    }
  }

  bool init() {
    buffer_ = static_cast<uint8_t*>(js_malloc(BUFFER_SIZE));
    return buffer_ != nullptr;
  }

  bool readable() { return writeHead_ > readHead_; }

  void beginWritingEntry() {
    // uncommittedWriteHead_ can be > writeHead_ if a previous write failed.
    // In that case, this effectively discards whatever was written during that
    // time
    MOZ_ASSERT(uncommittedWriteHead_ >= writeHead_);
    uncommittedWriteHead_ = writeHead_;
    uncommittedWriteHead_ += ENTRY_HEADER_SIZE;
  }

  void finishWritingEntry() {
    MOZ_ASSERT(uncommittedWriteHead_ - writeHead_ <=
               std::numeric_limits<uint16_t>::max());
    uint16_t entryHeader = uint16_t(uncommittedWriteHead_ - writeHead_);
    writeBytesAtOffset(reinterpret_cast<const uint8_t*>(&entryHeader),
                       sizeof(entryHeader), writeHead_);
    writeHead_ = uncommittedWriteHead_;
  }

  void beginReadingEntry() {
    MOZ_ASSERT(uncommittedReadHead_ == readHead_);
    // We will read the entry header (still pointed to by readHead_) from
    // inside finishReadingEntry
    uncommittedReadHead_ += ENTRY_HEADER_SIZE;
  }

  void finishReadingEntry() {
    uint16_t entryHeader;
    readBytesAtOffset(reinterpret_cast<uint8_t*>(&entryHeader),
                      sizeof(entryHeader), readHead_);
    size_t read = uncommittedReadHead_ - readHead_;

    MOZ_RELEASE_ASSERT(entryHeader == uint16_t(read));
    readHead_ += entryHeader;
    uncommittedReadHead_ = readHead_;
  }

  void skipEntry() {
    uint16_t entryHeader;
    readBytesAtOffset(reinterpret_cast<uint8_t*>(&entryHeader),
                      sizeof(entryHeader), readHead_);
    readHead_ += entryHeader;
    uncommittedReadHead_ = readHead_;
  }

  void writeBytesAtOffset(const uint8_t* bytes, size_t length,
                          uint64_t offset) {
    MOZ_ASSERT(offset + length <= readHead_ + BUFFER_SIZE);

    size_t maskedWriteHead = offset & BUFFER_MASK;
    if (maskedWriteHead + length > BUFFER_SIZE) {
      size_t firstChunk = BUFFER_SIZE - maskedWriteHead;
      memcpy(buffer_ + maskedWriteHead, bytes, firstChunk);
      memcpy(buffer_, bytes + firstChunk, length - firstChunk);
    } else {
      memcpy(buffer_ + maskedWriteHead, bytes, length);
    }
  }

  void writeBytes(const uint8_t* bytes, size_t length) {
    // Skip the read head forward if we're about to overwrite unread entries
    while (MOZ_UNLIKELY(uncommittedWriteHead_ + length >
                        readHead_ + BUFFER_SIZE)) {
      skipEntry();
    }

    writeBytesAtOffset(bytes, length, uncommittedWriteHead_);
    uncommittedWriteHead_ += length;
  }

  template <typename T>
  void write(T val) {
    // No magic hidden work allowed here - we are just reducing duplicate code
    // serializing integers and floats.
    static_assert(std::is_arithmetic_v<T>);
    writeBytes(reinterpret_cast<const uint8_t*>(&val), sizeof(T));
  }

  void writeEmptyString() {
    write(uint8_t(TracerStringEncoding::Latin1));
    write(uint32_t(0));  // length
  }

  bool writeString(JSContext* cx, JS::Handle<JSString*> str) {
    TracerStringEncoding encoding;
    if (str->hasLatin1Chars()) {
      encoding = TracerStringEncoding::Latin1;
    } else {
      encoding = TracerStringEncoding::TwoByte;
    }

    // TODO: if ropes are common we can certainly serialize them without
    // linearizing - this is just easy
    JSLinearString* linear = str->ensureLinear(cx);
    if (!linear) {
      return false;
    }
    write(uint8_t(encoding));
    size_t length = linear->length();
    MOZ_ASSERT(length <= std::numeric_limits<uint32_t>::max());
    write(uint32_t(length));
    size_t size = length;
    JS::AutoAssertNoGC nogc;
    const uint8_t* charBuffer = nullptr;
    if (encoding == TracerStringEncoding::TwoByte) {
      size *= sizeof(char16_t);
      charBuffer = reinterpret_cast<const uint8_t*>(linear->twoByteChars(nogc));
    } else {
      charBuffer = reinterpret_cast<const uint8_t*>(linear->latin1Chars(nogc));
    }
    writeBytes(charBuffer, size);
    return true;
  }

  template <typename CharType, TracerStringEncoding Encoding>
  void writeCString(const CharType* chars) {
    size_t length = std::char_traits<CharType>::length(chars);
    static_assert(sizeof(CharType) == 1 ||
                  Encoding == TracerStringEncoding::TwoByte);
    static_assert(sizeof(CharType) <= 2);
    write(uint8_t(Encoding));
    MOZ_ASSERT(length <= std::numeric_limits<uint32_t>::max());
    write(uint32_t(length));
    const size_t size = length * sizeof(CharType);
    writeBytes(reinterpret_cast<const uint8_t*>(chars), size);
  }

  void readBytesAtOffset(uint8_t* bytes, size_t length, uint64_t offset) {
    size_t maskedReadHead = offset & BUFFER_MASK;
    if (maskedReadHead + length > BUFFER_SIZE) {
      size_t firstChunk = BUFFER_SIZE - maskedReadHead;
      memcpy(bytes, buffer_ + maskedReadHead, firstChunk);
      memcpy(bytes + firstChunk, buffer_, length - firstChunk);
    } else {
      memcpy(bytes, buffer_ + maskedReadHead, length);
    }
  }

  void readBytes(uint8_t* bytes, size_t length) {
    readBytesAtOffset(bytes, length, uncommittedReadHead_);
    uncommittedReadHead_ += length;
  }

  template <typename T>
  void read(T* val) {
    static_assert(std::is_arithmetic_v<T>);
    readBytes(reinterpret_cast<uint8_t*>(val), sizeof(T));
  }

  bool readString(JSContext* cx, JS::MutableHandle<JSString*> result) {
    uint8_t encodingByte;
    read(&encodingByte);
    TracerStringEncoding encoding = TracerStringEncoding(encodingByte);
    uint32_t length;
    read(&length);

    if (length == 0) {
      result.set(JS_GetEmptyString(cx));
      return true;
    }

    JSLinearString* str = nullptr;
    if (encoding == TracerStringEncoding::UTF8 ||
        encoding == TracerStringEncoding::Latin1) {
      UniquePtr<unsigned char[], JS::FreePolicy> chars(
          cx->make_pod_arena_array<unsigned char>(StringBufferArena, length));
      if (!chars) {
        return false;
      }
      readBytes(reinterpret_cast<uint8_t*>(chars.get()), length);
      if (encoding == TracerStringEncoding::UTF8) {
        str = NewStringCopyUTF8N(
            cx, JS::UTF8Chars(reinterpret_cast<char*>(chars.get()), length));
      } else {
        str = NewString<CanGC>(cx, std::move(chars), length);
      }
    } else {
      MOZ_ASSERT(encoding == TracerStringEncoding::TwoByte);
      UniquePtr<char16_t[], JS::FreePolicy> chars(
          cx->make_pod_arena_array<char16_t>(StringBufferArena, length));
      if (!chars) {
        return false;
      }
      readBytes((uint8_t*)chars.get(), length * sizeof(char16_t));
      str = NewString<CanGC>(cx, std::move(chars), length);
    }

    if (!str) {
      return false;
    }

    result.set(str);

    return true;
  }
};

// These sizes are to some degree picked out of a hat, and eventually it might
// be nice to make them configurable. For reference, I measured it costing
// 145MB to open gdocs and create an empty document, so 256MB is just some
// extra wiggle room for complex use cases.
using InlineDataBuffer = TracingBuffer<1 << 28>;

// The size for the out of line data is much smaller, so I just picked a size
// that was much smaller but big enough that I didn't see us running out of it
// when playing around on various complex apps. Again, it would be great in the
// future for this to be configurable.
using OutOfLineDataBuffer = TracingBuffer<1 << 22>;

// An ExecutionTracer is responsible for recording JS execution while it is
// enabled to a set of ring buffers, and providing that information as a JS
// object when requested. See Debugger.md (collectNativeTrace) for more details.
class ExecutionTracer {
 public:
  enum class EventKind {
    FunctionEnter = 0,
    FunctionLeave = 1,
    LabelEnter = 2,
    LabelLeave = 3,
  };

  enum class ImplementationType : uint8_t {
    Interpreter = 0,
    Baseline = 1,
    Ion = 2,
    Wasm = 3,
  };

 private:
  // This holds the actual entries, one for each push or pop of a frame or label
  InlineDataBuffer inlineData_;

  // This holds data that may be duplicated across entries, like script URLs or
  // function names. This should generally be much smaller in terms of raw
  // bytes. Note however that we can still wrap around this buffer and lose
  // entries - the system is best effort, and the consumer must accomodate the
  // fact that entries from inlineData_ may reference expired data from
  // outOfLineData_
  OutOfLineDataBuffer outOfLineData_;

  void writeScriptUrl(ScriptSource* scriptSource);

  // Writes an atom into the outOfLineData_, associating it with the specified
  // id. In practice, `id` comes from an atom id inside a cache in the
  // JSContext which is incremented each time a new atom is registered and
  // cleared when tracing is done.
  bool writeAtom(JSContext* cx, JS::Handle<JSAtom*> atom, uint32_t id);
  bool writeFunctionFrame(JSContext* cx, AbstractFramePtr frame);
  bool readFunctionFrame(JSContext* cx, JS::Handle<JSObject*> result,
                         EventKind kind);
  bool readStackFunctionEnter(JSContext* cx, JS::Handle<JSObject*> events);
  bool readStackFunctionLeave(JSContext* cx, JS::Handle<JSObject*> events);
  bool readScriptURLEntry(JSContext* cx, JS::Handle<JSObject*> scriptUrls);
  bool readAtomEntry(JSContext* cx, JS::Handle<JSObject*> atoms);
  bool readLabel(JSContext* cx, JS::Handle<JSObject*> events, EventKind kind);
  bool readInlineEntry(JSContext* cx, JS::Handle<JSObject*> events);
  bool readOutOfLineEntry(JSContext* cx, JS::Handle<JSObject*> scriptUrls,
                          JS::Handle<JSObject*> atoms);
  bool readInlineEntries(JSContext* cx, JS::Handle<JSObject*> events);
  bool readOutOfLineEntries(JSContext* cx, JS::Handle<JSObject*> scriptUrls,
                            JS::Handle<JSObject*> atoms);

 public:
  bool init() {
    if (!inlineData_.init()) {
      return false;
    }
    if (!outOfLineData_.init()) {
      return false;
    }
    return true;
  }

  bool onEnterFrame(JSContext* cx, AbstractFramePtr frame);
  bool onLeaveFrame(JSContext* cx, AbstractFramePtr frame);

  template <typename CharType, TracerStringEncoding Encoding>
  void onEnterLabel(const CharType* eventType);
  template <typename CharType, TracerStringEncoding Encoding>
  void onLeaveLabel(const CharType* eventType);

  // Reads the execution trace from the underlying ring buffers and outputs it
  // into a JS object. For the format of this object see
  // js/src/doc/Debugger/Debugger.md
  bool getTrace(JSContext* cx, JS::Handle<JSObject*> result);
};

}  // namespace js

#endif /* debugger_ExecutionTracer_h */
