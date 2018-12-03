/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_interceptor_PatcherDetour_h
#define mozilla_interceptor_PatcherDetour_h

#include "mozilla/interceptor/PatcherBase.h"
#include "mozilla/interceptor/Trampoline.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/TypedEnumBits.h"

#define COPY_CODES(NBYTES)                          \
  do {                                              \
    tramp.CopyFrom(origBytes.GetAddress(), NBYTES); \
    origBytes += NBYTES;                            \
  } while (0)

namespace mozilla {
namespace interceptor {

enum class DetourFlags : uint32_t {
  eDefault = 0,
  eEnable10BytePatch = 1,  // Allow 10-byte patches when conditions allow
  eTestOnlyForce10BytePatch =
      3,  // Force 10-byte patches at all times (testing only)
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(DetourFlags)

template <typename VMPolicy>
class WindowsDllDetourPatcher final : public WindowsDllPatcherBase<VMPolicy> {
  typedef typename VMPolicy::MMPolicyT MMPolicyT;
  DetourFlags mFlags;

 public:
  template <typename... Args>
  explicit WindowsDllDetourPatcher(Args... aArgs)
      : WindowsDllPatcherBase<VMPolicy>(std::forward<Args>(aArgs)...),
        mFlags(DetourFlags::eDefault) {}

  ~WindowsDllDetourPatcher() { Clear(); }

  WindowsDllDetourPatcher(const WindowsDllDetourPatcher&) = delete;
  WindowsDllDetourPatcher(WindowsDllDetourPatcher&&) = delete;
  WindowsDllDetourPatcher& operator=(const WindowsDllDetourPatcher&) = delete;
  WindowsDllDetourPatcher& operator=(WindowsDllDetourPatcher&&) = delete;

  void Clear() {
    if (!this->mVMPolicy.ShouldUnhookUponDestruction()) {
      return;
    }

#if defined(_M_IX86)
    size_t nBytes = 1 + sizeof(intptr_t);
#elif defined(_M_X64)
    size_t nBytes = 2 + sizeof(intptr_t);
#elif defined(_M_ARM64)
    size_t nBytes = 4;
#else
#error "Unknown processor type"
#endif

    const auto& tramps = this->mVMPolicy.Items();
    for (auto&& tramp : tramps) {
#if defined(_M_ARM64)
      MOZ_RELEASE_ASSERT(false, "Shouldn't get here");
#endif

      // First we read the pointer to the interceptor instance.
      Maybe<uintptr_t> instance = tramp.ReadEncodedPointer();
      if (!instance) {
        continue;
      }

      if (instance.value() != reinterpret_cast<uintptr_t>(this)) {
        // tramp does not belong to this interceptor instance.
        continue;
      }

      auto clearInstance = MakeScopeExit([&tramp]() -> void {
        // Clear the instance pointer so that no future instances with the same
        // |this| pointer will attempt to reset its hook.
        tramp.Rewind();
        tramp.WriteEncodedPointer(nullptr);
      });

      // Now we read the pointer to the intercepted function.
      Maybe<uintptr_t> interceptedFn = tramp.ReadEncodedPointer();
      if (!interceptedFn) {
        continue;
      }

      WritableTargetFunction<MMPolicyT> origBytes(
          this->mVMPolicy, interceptedFn.value(), nBytes);
      if (!origBytes) {
        continue;
      }

      Maybe<uint8_t> maybeOpcode1 = origBytes.ReadByte();
      if (!maybeOpcode1) {
        continue;
      }

      uint8_t opcode1 = maybeOpcode1.value();

#if defined(_M_IX86)
      // Ensure the JMP from CreateTrampoline is where we expect it to be.
      MOZ_ASSERT(opcode1 == 0xE9);
      if (opcode1 != 0xE9) {
        continue;
      }

      intptr_t startOfTrampInstructions =
          static_cast<intptr_t>(tramp.GetCurrentRemoteAddress());

      origBytes.WriteDisp32(startOfTrampInstructions);
      if (!origBytes) {
        continue;
      }

      origBytes.Commit();
#elif defined(_M_X64)
      if (opcode1 == 0x49) {
        if (!Clear13BytePatch(origBytes, tramp.GetCurrentRemoteAddress())) {
          continue;
        }
      } else if (opcode1 == 0xB8) {
        if (!Clear10BytePatch(origBytes)) {
          continue;
        }
      } else {
        MOZ_ASSERT_UNREACHABLE("Unrecognized patch!");
        continue;
      }
#elif defined(_M_ARM64)
      MOZ_RELEASE_ASSERT(false, "Shouldn't get here");
#else
#error "Unknown processor type"
#endif
    }

    this->mVMPolicy.Clear();
  }

  bool Clear13BytePatch(WritableTargetFunction<MMPolicyT>& aOrigBytes,
                        const uintptr_t aResetToAddress) {
    Maybe<uint8_t> maybeOpcode2 = aOrigBytes.ReadByte();
    if (!maybeOpcode2) {
      return false;
    }

    uint8_t opcode2 = maybeOpcode2.value();
    if (opcode2 != 0xBB) {
      return false;
    }

    aOrigBytes.WritePointer(aResetToAddress);
    if (!aOrigBytes) {
      return false;
    }

    return aOrigBytes.Commit();
  }

  bool Clear10BytePatch(WritableTargetFunction<MMPolicyT>& aOrigBytes) {
    Maybe<uint32_t> maybePtr32 = aOrigBytes.ReadLong();
    if (!maybePtr32) {
      return false;
    }

    uint32_t ptr32 = maybePtr32.value();
    // We expect the high bit to be clear
    if (ptr32 & 0x80000000) {
      return false;
    }

    uintptr_t trampPtr = ptr32;

    // trampPtr points to an intermediate trampoline that contains a 13-byte
    // patch. We back up by sizeof(uintptr_t) so that we can access the pointer
    // to the stub trampoline.
    WritableTargetFunction<MMPolicyT> writableIntermediate(
        this->mVMPolicy, trampPtr - sizeof(uintptr_t), 13 + sizeof(uintptr_t));
    if (!writableIntermediate) {
      return false;
    }

    Maybe<uintptr_t> stubTramp = writableIntermediate.ReadEncodedPtr();
    if (!stubTramp || !stubTramp.value()) {
      return false;
    }

    Maybe<uint8_t> maybeOpcode1 = writableIntermediate.ReadByte();
    if (!maybeOpcode1) {
      return false;
    }

    // We expect this opcode to be the beginning of our normal mov r11, ptr
    // patch sequence.
    uint8_t opcode1 = maybeOpcode1.value();
    if (opcode1 != 0x49) {
      return false;
    }

    // Now we can just delegate the rest to our normal 13-byte patch clearing.
    return Clear13BytePatch(writableIntermediate, stubTramp.value());
  }

  void Init(DetourFlags aFlags = DetourFlags::eDefault, int aNumHooks = 0) {
    if (Initialized()) {
      return;
    }

    mFlags = aFlags;

    if (aNumHooks == 0) {
      // Win32 allocates VM addresses at a 64KiB granularity, so by default we
      // might as well utilize that entire 64KiB reservation instead of
      // artifically constraining ourselves to the page size.
      aNumHooks = this->mVMPolicy.GetAllocGranularity() / kHookSize;
    }

    ReservationFlags resFlags = ReservationFlags::eDefault;
    if (aFlags & DetourFlags::eEnable10BytePatch) {
      resFlags |= ReservationFlags::eForceFirst2GB;
    }

    this->mVMPolicy.Reserve(aNumHooks, resFlags);
  }

  bool Initialized() const { return !!this->mVMPolicy; }

  bool AddHook(FARPROC aTargetFn, intptr_t aHookDest, void** aOrigFunc) {
    ReadOnlyTargetFunction<MMPolicyT> target(
        this->ResolveRedirectedAddress(aTargetFn));

    CreateTrampoline(target, aHookDest, aOrigFunc);
    if (!*aOrigFunc) {
      return false;
    }

    return true;
  }

 protected:
  const static int kPageSize = 4096;
  const static int kHookSize = 128;

  // rex bits
  static const BYTE kMaskHighNibble = 0xF0;
  static const BYTE kRexOpcode = 0x40;
  static const BYTE kMaskRexW = 0x08;
  static const BYTE kMaskRexR = 0x04;
  static const BYTE kMaskRexX = 0x02;
  static const BYTE kMaskRexB = 0x01;

  // mod r/m bits
  static const BYTE kRegFieldShift = 3;
  static const BYTE kMaskMod = 0xC0;
  static const BYTE kMaskReg = 0x38;
  static const BYTE kMaskRm = 0x07;
  static const BYTE kRmNeedSib = 0x04;
  static const BYTE kModReg = 0xC0;
  static const BYTE kModDisp32 = 0x80;
  static const BYTE kModDisp8 = 0x40;
  static const BYTE kModNoRegDisp = 0x00;
  static const BYTE kRmNoRegDispDisp32 = 0x05;

  // sib bits
  static const BYTE kMaskSibScale = 0xC0;
  static const BYTE kMaskSibIndex = 0x38;
  static const BYTE kMaskSibBase = 0x07;
  static const BYTE kSibBaseEbp = 0x05;

  // Register bit IDs.
  static const BYTE kRegAx = 0x0;
  static const BYTE kRegCx = 0x1;
  static const BYTE kRegDx = 0x2;
  static const BYTE kRegBx = 0x3;
  static const BYTE kRegSp = 0x4;
  static const BYTE kRegBp = 0x5;
  static const BYTE kRegSi = 0x6;
  static const BYTE kRegDi = 0x7;

  // Special ModR/M codes.  These indicate operands that cannot be simply
  // memcpy-ed.
  // Operand is a 64-bit RIP-relative address.
  static const int kModOperand64 = -2;
  // Operand is not yet handled by our trampoline.
  static const int kModUnknown = -1;

  /**
   * Returns the number of bytes taken by the ModR/M byte, SIB (if present)
   * and the instruction's operand.  In special cases, the special MODRM codes
   * above are returned.
   * aModRm points to the ModR/M byte of the instruction.
   * On return, aSubOpcode (if present) is filled with the subopcode/register
   * code found in the ModR/M byte.
   */
  int CountModRmSib(const ReadOnlyTargetFunction<MMPolicyT>& aModRm,
                    BYTE* aSubOpcode = nullptr) {
    int numBytes = 1;  // Start with 1 for mod r/m byte itself
    switch (*aModRm & kMaskMod) {
      case kModReg:
        return numBytes;
      case kModDisp8:
        numBytes += 1;
        break;
      case kModDisp32:
        numBytes += 4;
        break;
      case kModNoRegDisp:
        if ((*aModRm & kMaskRm) == kRmNoRegDispDisp32) {
#if defined(_M_X64)
          if (aSubOpcode) {
            *aSubOpcode = (*aModRm & kMaskReg) >> kRegFieldShift;
          }
          return kModOperand64;
#else
          // On IA-32, all ModR/M instruction modes address memory relative to 0
          numBytes += 4;
#endif
        } else if (((*aModRm & kMaskRm) == kRmNeedSib &&
                    (*(aModRm + 1) & kMaskSibBase) == kSibBaseEbp)) {
          numBytes += 4;
        }
        break;
      default:
        // This should not be reachable
        MOZ_ASSERT_UNREACHABLE("Impossible value for modr/m byte mod bits");
        return kModUnknown;
    }
    if ((*aModRm & kMaskRm) == kRmNeedSib) {
      // SIB byte
      numBytes += 1;
    }
    if (aSubOpcode) {
      *aSubOpcode = (*aModRm & kMaskReg) >> kRegFieldShift;
    }
    return numBytes;
  }

#if defined(_M_X64)
  enum class JumpType{Je, Jne, Jmp, Call};

  static bool GenerateJump(Trampoline<MMPolicyT>& aTramp,
                           uintptr_t aAbsTargetAddress, const JumpType aType) {
    // Near call, absolute indirect, address given in r/m32
    if (aType == JumpType::Call) {
      // CALL [RIP+0]
      aTramp.WriteByte(0xff);
      aTramp.WriteByte(0x15);
      // The offset to jump destination -- 2 bytes after the current position.
      aTramp.WriteInteger(2);
      aTramp.WriteByte(0xeb);  // JMP + 8 (jump over target address)
      aTramp.WriteByte(8);
      aTramp.WritePointer(aAbsTargetAddress);
      return !!aTramp;
    }

    if (aType == JumpType::Je) {
      // JNE RIP+14
      aTramp.WriteByte(0x75);
      aTramp.WriteByte(14);
    } else if (aType == JumpType::Jne) {
      // JE RIP+14
      aTramp.WriteByte(0x74);
      aTramp.WriteByte(14);
    }

    // Near jmp, absolute indirect, address given in r/m32
    // JMP [RIP+0]
    aTramp.WriteByte(0xff);
    aTramp.WriteByte(0x25);
    // The offset to jump destination is 0
    aTramp.WriteInteger(0);
    aTramp.WritePointer(aAbsTargetAddress);

    return !!aTramp;
  }
#endif

  enum ePrefixGroupBits {
    eNoPrefixes = 0,
    ePrefixGroup1 = (1 << 0),
    ePrefixGroup2 = (1 << 1),
    ePrefixGroup3 = (1 << 2),
    ePrefixGroup4 = (1 << 3)
  };

  int CountPrefixBytes(const ReadOnlyTargetFunction<MMPolicyT>& aBytes,
                       const int aBytesIndex, unsigned char* aOutGroupBits) {
    unsigned char& groupBits = *aOutGroupBits;
    groupBits = eNoPrefixes;
    int index = aBytesIndex;
    while (true) {
      switch (aBytes[index]) {
        // Group 1
        case 0xF0:  // LOCK
        case 0xF2:  // REPNZ
        case 0xF3:  // REP / REPZ
          if (groupBits & ePrefixGroup1) {
            return -1;
          }
          groupBits |= ePrefixGroup1;
          ++index;
          break;

        // Group 2
        case 0x2E:  // CS override / branch not taken
        case 0x36:  // SS override
        case 0x3E:  // DS override / branch taken
        case 0x64:  // FS override
        case 0x65:  // GS override
          if (groupBits & ePrefixGroup2) {
            return -1;
          }
          groupBits |= ePrefixGroup2;
          ++index;
          break;

        // Group 3
        case 0x66:  // operand size override
          if (groupBits & ePrefixGroup3) {
            return -1;
          }
          groupBits |= ePrefixGroup3;
          ++index;
          break;

        // Group 4
        case 0x67:  // Address size override
          if (groupBits & ePrefixGroup4) {
            return -1;
          }
          groupBits |= ePrefixGroup4;
          ++index;
          break;

        default:
          return index - aBytesIndex;
      }
    }
  }

  // Return a ModR/M byte made from the 2 Mod bits, the register used for the
  // reg bits and the register used for the R/M bits.
  BYTE BuildModRmByte(BYTE aModBits, BYTE aReg, BYTE aRm) {
    MOZ_ASSERT((aRm & kMaskRm) == aRm);
    MOZ_ASSERT((aModBits & kMaskMod) == aModBits);
    MOZ_ASSERT(((aReg << kRegFieldShift) & kMaskReg) ==
               (aReg << kRegFieldShift));
    return aModBits | (aReg << kRegFieldShift) | aRm;
  }

  void CreateTrampoline(ReadOnlyTargetFunction<MMPolicyT>& origBytes,
                        intptr_t aDest, void** aOutTramp) {
    *aOutTramp = nullptr;

    Trampoline<MMPolicyT> tramp(this->mVMPolicy.GetNextTrampoline());
    if (!tramp) {
      return;
    }

    // The beginning of the trampoline contains two pointer-width slots:
    // [0]: |this|, so that we know whether the trampoline belongs to us;
    // [1]: Pointer to original function, so that we can reset the hook upon
    //      destruction.
    tramp.WriteEncodedPointer(this);
    if (!tramp) {
      return;
    }

    auto clearInstanceOnFailure = MakeScopeExit([aOutTramp, &tramp]() -> void {
      // *aOutTramp is not set until CreateTrampoline has completed
      // successfully, so we can use that to check for success.
      if (*aOutTramp) {
        return;
      }

      // Clear the instance pointer so that we don't try to reset a nonexistent
      // hook.
      tramp.Rewind();
      tramp.WriteEncodedPointer(nullptr);
    });

    tramp.WritePointer(origBytes.AsEncodedPtr());
    if (!tramp) {
      return;
    }

    tramp.StartExecutableCode();

#if defined(_M_IX86)
    int pJmp32 = -1;
    while (origBytes.GetOffset() < 5) {
      // Understand some simple instructions that might be found in a
      // prologue; we might need to extend this as necessary.
      //
      // Note!  If we ever need to understand jump instructions, we'll
      // need to rewrite the displacement argument.
      unsigned char prefixGroups;
      int numPrefixBytes =
          CountPrefixBytes(origBytes, origBytes.GetOffset(), &prefixGroups);
      if (numPrefixBytes < 0 ||
          (prefixGroups & (ePrefixGroup3 | ePrefixGroup4))) {
        // Either the prefix sequence was bad, or there are prefixes that
        // we don't currently support (groups 3 and 4)
        MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
        return;
      }

      origBytes += numPrefixBytes;
      if (*origBytes >= 0x88 && *origBytes <= 0x8B) {
        // various MOVs
        ++origBytes;
        int len = CountModRmSib(origBytes);
        if (len < 0) {
          MOZ_ASSERT_UNREACHABLE("Unrecognized MOV opcode sequence");
          return;
        }
        origBytes += len;
      } else if (*origBytes == 0x0f &&
                 (origBytes[1] == 0x10 || origBytes[1] == 0x11)) {
        // SSE: movups xmm, xmm/m128
        //      movups xmm/m128, xmm
        origBytes += 2;
        int len = CountModRmSib(origBytes);
        if (len < 0) {
          MOZ_ASSERT_UNREACHABLE("Unrecognized MOV opcode sequence");
          return;
        }
        origBytes += len;
      } else if (*origBytes == 0xA1) {
        // MOV eax, [seg:offset]
        origBytes += 5;
      } else if (*origBytes == 0xB8) {
        // MOV 0xB8: http://ref.x86asm.net/coder32.html#xB8
        origBytes += 5;
      } else if (*origBytes == 0x33 && (origBytes[1] & kMaskMod) == kModReg) {
        // XOR r32, r32
        origBytes += 2;
      } else if ((*origBytes & 0xf8) == 0x40) {
        // INC r32
        origBytes += 1;
      } else if (*origBytes == 0x83) {
        // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r/m, imm8
        unsigned char b = origBytes[1];
        if ((b & 0xc0) == 0xc0) {
          // ADD|ODR|ADC|SBB|AND|SUB|XOR|CMP r, imm8
          origBytes += 3;
        } else {
          // bail
          MOZ_ASSERT_UNREACHABLE("Unrecognized bit opcode sequence");
          return;
        }
      } else if (*origBytes == 0x68) {
        // PUSH with 4-byte operand
        origBytes += 5;
      } else if ((*origBytes & 0xf0) == 0x50) {
        // 1-byte PUSH/POP
        ++origBytes;
      } else if (*origBytes == 0x6A) {
        // PUSH imm8
        origBytes += 2;
      } else if (*origBytes == 0xe9) {
        pJmp32 = origBytes.GetOffset();
        // jmp 32bit offset
        origBytes += 5;
      } else if (*origBytes == 0xff && origBytes[1] == 0x25) {
        // jmp [disp32]
        origBytes += 6;
      } else if (*origBytes == 0xc2) {
        // ret imm16.  We can't handle this but it happens.  We don't ASSERT but
        // we do fail to hook.
#if defined(MOZILLA_INTERNAL_API)
        NS_WARNING("Cannot hook method -- RET opcode found");
#endif
        return;
      } else {
        // printf ("Unknown x86 instruction byte 0x%02x, aborting trampoline\n",
        // *origBytes);
        MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
        return;
      }
    }

    // The trampoline is a copy of the instructions that we just traced,
    // followed by a jump that we add below.
    tramp.CopyFrom(origBytes.GetBaseAddress(), origBytes.GetOffset());
    if (!tramp) {
      return;
    }
#elif defined(_M_X64)
    bool foundJmp = false;
    // |use10BytePatch| should always default to |false| in production. It is
    // not set to true unless we detect that a 10-byte patch is necessary.
    // OTOH, for testing purposes, if we want to force a 10-byte patch, we
    // always initialize |use10BytePatch| to |true|.
    bool use10BytePatch = (mFlags & DetourFlags::eTestOnlyForce10BytePatch) ==
                          DetourFlags::eTestOnlyForce10BytePatch;
    const uint32_t bytesRequired = use10BytePatch ? 10 : 13;

    while (origBytes.GetOffset() < bytesRequired) {
      // If we found JMP 32bit offset, we require that the next bytes must
      // be NOP or INT3.  There is no reason to copy them.
      // TODO: This used to trigger for Je as well.  Now that I allow
      // instructions after CALL and JE, I don't think I need that.
      // The only real value of this condition is that if code follows a JMP
      // then its _probably_ the target of a JMP somewhere else and we
      // will be overwriting it, which would be tragic.  This seems
      // highly unlikely.
      if (foundJmp) {
        if (*origBytes == 0x90 || *origBytes == 0xcc) {
          ++origBytes;
          continue;
        }

        // If our trampoline space is located in the lowest 2GB, we can do a ten
        // byte patch instead of a thirteen byte patch.
        if (this->mVMPolicy.IsTrampolineSpaceInLowest2GB() &&
            origBytes.GetOffset() >= 10) {
          use10BytePatch = true;
          break;
        }

        MOZ_ASSERT_UNREACHABLE("Opcode sequence includes commands after JMP");
        return;
      }
      if (*origBytes == 0x0f) {
        COPY_CODES(1);
        if (*origBytes == 0x1f) {
          // nop (multibyte)
          COPY_CODES(1);
          if ((*origBytes & 0xc0) == 0x40 && (*origBytes & 0x7) == 0x04) {
            COPY_CODES(3);
          } else {
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
        } else if (*origBytes == 0x05) {
          // syscall
          COPY_CODES(1);
        } else if (*origBytes == 0x10 || *origBytes == 0x11) {
          // SSE: movups xmm, xmm/m128
          //      movups xmm/m128, xmm
          COPY_CODES(1);
          int nModRmSibBytes = CountModRmSib(origBytes);
          if (nModRmSibBytes < 0) {
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          } else {
            COPY_CODES(nModRmSibBytes);
          }
        } else if (*origBytes == 0x84) {
          // je rel32
          ++origBytes;
          --tramp;  // overwrite the 0x0f we copied above

          if (!GenerateJump(tramp, origBytes.ReadDisp32AsAbsolute(),
                            JumpType::Je)) {
            return;
          }
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes >= 0x88 && *origBytes <= 0x8B) {
        // various 32-bit MOVs
        COPY_CODES(1);
        int len = CountModRmSib(origBytes);
        if (len < 0) {
          MOZ_ASSERT_UNREACHABLE("Unrecognized MOV opcode sequence");
          return;
        }
        COPY_CODES(len);
      } else if (*origBytes == 0x40 || *origBytes == 0x41) {
        // Plain REX or REX.B
        COPY_CODES(1);
        if ((*origBytes & 0xf0) == 0x50) {
          // push/pop with Rx register
          COPY_CODES(1);
        } else if (*origBytes >= 0xb8 && *origBytes <= 0xbf) {
          // mov r32, imm32
          COPY_CODES(5);
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes == 0x44) {
        // REX.R
        COPY_CODES(1);

        // TODO: Combine with the "0x89" case below in the REX.W section
        if (*origBytes == 0x89) {
          // mov r/m32, r32
          COPY_CODES(1);
          int len = CountModRmSib(origBytes);
          if (len < 0) {
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
          COPY_CODES(len);
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes == 0x45) {
        // REX.R & REX.B
        COPY_CODES(1);

        if (*origBytes == 0x33) {
          // xor r32, r32
          COPY_CODES(2);
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if ((*origBytes & 0xfa) == 0x48) {
        // REX.W | REX.WR | REX.WRB | REX.WB
        COPY_CODES(1);

        if (*origBytes == 0x81 && (origBytes[1] & 0xf8) == 0xe8) {
          // sub r, dword
          COPY_CODES(6);
        } else if (*origBytes == 0x83 && (origBytes[1] & 0xf8) == 0xe8) {
          // sub r, byte
          COPY_CODES(3);
        } else if (*origBytes == 0x83 &&
                   (origBytes[1] & (kMaskMod | kMaskReg)) == kModReg) {
          // add r, byte
          COPY_CODES(3);
        } else if (*origBytes == 0x83 && (origBytes[1] & 0xf8) == 0x60) {
          // and [r+d], imm8
          COPY_CODES(5);
        } else if (*origBytes == 0x2b && (origBytes[1] & kMaskMod) == kModReg) {
          // sub r64, r64
          COPY_CODES(2);
        } else if (*origBytes == 0x85) {
          // 85 /r => TEST r/m32, r32
          if ((origBytes[1] & 0xc0) == 0xc0) {
            COPY_CODES(2);
          } else {
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
        } else if ((*origBytes & 0xfd) == 0x89) {
          // MOV r/m64, r64 | MOV r64, r/m64
          BYTE reg;
          int len = CountModRmSib(origBytes + 1, &reg);
          if (len < 0) {
            MOZ_ASSERT(len == kModOperand64);
            if (len != kModOperand64) {
              return;
            }
            origBytes += 2;  // skip the MOV and MOD R/M bytes

            // The instruction MOVs 64-bit data from a RIP-relative memory
            // address (determined with a 32-bit offset from RIP) into a
            // 64-bit register.
            uintptr_t absAddr = origBytes.ReadDisp32AsAbsolute();

            if (reg == kRegAx) {
              // Destination is RAX.  Encode instruction as MOVABS with a
              // 64-bit absolute address as its immediate operand.
              tramp.WriteByte(0xa1);
              tramp.WritePointer(absAddr);
            } else {
              // The MOV must be done in two steps.  First, we MOVABS the
              // absolute 64-bit address into our target register.
              // Then, we MOV from that address into the register
              // using register-indirect addressing.
              tramp.WriteByte(0xb8 + reg);
              tramp.WritePointer(absAddr);
              tramp.WriteByte(0x48);
              tramp.WriteByte(0x8b);
              tramp.WriteByte(BuildModRmByte(kModNoRegDisp, reg, reg));
            }
          } else {
            COPY_CODES(len + 1);
          }
        } else if (*origBytes == 0xc7) {
          // MOV r/m64, imm32
          if (origBytes[1] == 0x44) {
            // MOV [r64+disp8], imm32
            // ModR/W + SIB + disp8 + imm32
            COPY_CODES(8);
          } else {
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
        } else if (*origBytes == 0xff) {
          // JMP /4
          if ((origBytes[1] & 0xc0) == 0x0 && (origBytes[1] & 0x07) == 0x5) {
            origBytes += 2;
            --tramp;  // overwrite the REX.W/REX.RW we copied above

            if (!GenerateJump(tramp, origBytes.ChasePointerFromDisp(),
                              JumpType::Jmp)) {
              return;
            }

            foundJmp = true;
          } else {
            // not support yet!
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
        } else if (*origBytes == 0x8d) {
          // LEA reg, addr
          if ((origBytes[1] & kMaskMod) == 0x0 &&
              (origBytes[1] & kMaskRm) == 0x5) {
            // [rip+disp32]
            // convert 32bit offset to 64bit direct and convert instruction
            // to a simple 64-bit mov
            BYTE reg = (origBytes[1] & kMaskReg) >> kRegFieldShift;
            origBytes += 2;
            uintptr_t absAddr = origBytes.ReadDisp32AsAbsolute();
            tramp.WriteByte(0xb8 + reg);  // move
            tramp.WritePointer(absAddr);
          } else {
            // Above we dealt with RIP-relative instructions.  Any other
            // operand form can simply be copied.
            int len = CountModRmSib(origBytes + 1);
            // We handled the kModOperand64 -- ie RIP-relative -- case above
            MOZ_ASSERT(len > 0);
            COPY_CODES(len + 1);
          }
        } else if (*origBytes == 0x63 && (origBytes[1] & kMaskMod) == kModReg) {
          // movsxd r64, r32 (move + sign extend)
          COPY_CODES(2);
        } else {
          // not support yet!
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes == 0x66) {
        // operand override prefix
        COPY_CODES(1);
        // This is the same as the x86 version
        if (*origBytes >= 0x88 && *origBytes <= 0x8B) {
          // various MOVs
          unsigned char b = origBytes[1];
          if (((b & 0xc0) == 0xc0) ||
              (((b & 0xc0) == 0x00) && ((b & 0x07) != 0x04) &&
               ((b & 0x07) != 0x05))) {
            // REG=r, R/M=r or REG=r, R/M=[r]
            COPY_CODES(2);
          } else if ((b & 0xc0) == 0x40) {
            if ((b & 0x07) == 0x04) {
              // REG=r, R/M=[SIB + disp8]
              COPY_CODES(4);
            } else {
              // REG=r, R/M=[r + disp8]
              COPY_CODES(3);
            }
          } else {
            // complex MOV, bail
            MOZ_ASSERT_UNREACHABLE("Unrecognized MOV opcode sequence");
            return;
          }
        } else if (*origBytes == 0x44 && origBytes[1] == 0x89) {
          // mov word ptr [reg+disp8], reg
          COPY_CODES(2);
          int len = CountModRmSib(origBytes);
          if (len < 0) {
            // no way to support this yet.
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
          COPY_CODES(len);
        }
      } else if ((*origBytes & 0xf0) == 0x50) {
        // 1-byte push/pop
        COPY_CODES(1);
      } else if (*origBytes == 0x65) {
        // GS prefix
        //
        // The entry of GetKeyState on Windows 10 has the following code.
        // 65 48 8b 04 25 30 00 00 00    mov   rax,qword ptr gs:[30h]
        // (GS prefix + REX + MOV (0x8b) ...)
        if (origBytes[1] == 0x48 &&
            (origBytes[2] >= 0x88 && origBytes[2] <= 0x8b)) {
          COPY_CODES(3);
          int len = CountModRmSib(origBytes);
          if (len < 0) {
            // no way to support this yet.
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }
          COPY_CODES(len);
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes == 0x80 && origBytes[1] == 0x3d) {
        origBytes += 2;

        // cmp byte ptr [rip-relative address], imm8
        // We'll compute the absolute address and do the cmp in r11

        // push r11 (to save the old value)
        tramp.WriteByte(0x49);
        tramp.WriteByte(0x53);

        uintptr_t absAddr = origBytes.ReadDisp32AsAbsolute();

        // mov r11, absolute address
        tramp.WriteByte(0x49);
        tramp.WriteByte(0xbb);
        tramp.WritePointer(absAddr);

        // cmp byte ptr [r11],...
        tramp.WriteByte(0x41);
        tramp.WriteByte(0x80);
        tramp.WriteByte(0x3b);

        // ...imm8
        COPY_CODES(1);

        // pop r11 (doesn't affect the flags from the cmp)
        tramp.WriteByte(0x49);
        tramp.WriteByte(0x5b);
      } else if (*origBytes == 0x90) {
        // nop
        COPY_CODES(1);
      } else if ((*origBytes & 0xf8) == 0xb8) {
        // MOV r32, imm32
        COPY_CODES(5);
      } else if (*origBytes == 0x33) {
        // xor r32, r/m32
        COPY_CODES(2);
      } else if (*origBytes == 0xf6) {
        // test r/m8, imm8 (used by ntdll on Windows 10 x64)
        // (no flags are affected by near jmp since there is no task switch,
        // so it is ok for a jmp to be written immediately after a test)
        BYTE subOpcode = 0;
        int nModRmSibBytes = CountModRmSib(origBytes + 1, &subOpcode);
        if (nModRmSibBytes < 0 || subOpcode != 0) {
          // Unsupported
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
        COPY_CODES(2 + nModRmSibBytes);
      } else if (*origBytes == 0x85) {
        // test r/m32, r32
        int nModRmSibBytes = CountModRmSib(origBytes + 1);
        if (nModRmSibBytes < 0) {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
        COPY_CODES(1 + nModRmSibBytes);
      } else if (*origBytes == 0xd1 && (origBytes[1] & kMaskMod) == kModReg) {
        // bit shifts/rotates : (SA|SH|RO|RC)(R|L) r32
        // (e.g. 0xd1 0xe0 is SAL, 0xd1 0xc8 is ROR)
        COPY_CODES(2);
      } else if (*origBytes == 0xc3) {
        // ret
        COPY_CODES(1);
      } else if (*origBytes == 0xcc) {
        // int 3
        COPY_CODES(1);
      } else if (*origBytes == 0xe8 || *origBytes == 0xe9) {
        // CALL (0xe8) or JMP (0xe9) 32bit offset
        foundJmp = *origBytes == 0xe9;
        ++origBytes;

        if (!GenerateJump(tramp, origBytes.ReadDisp32AsAbsolute(),
                          foundJmp ? JumpType::Jmp : JumpType::Call)) {
          return;
        }
      } else if (*origBytes == 0x74 ||  // je rel8 (0x74)
                 *origBytes == 0x75) {  // jne rel8 (0x75)
        uint8_t offset = origBytes[1];
        auto jumpType = JumpType::Je;
        if (*origBytes == 0x75) {
          jumpType = JumpType::Jne;
        }

        origBytes += 2;

        if (!GenerateJump(tramp, origBytes.OffsetToAbsolute(offset),
                          jumpType)) {
          return;
        }
      } else if (*origBytes == 0xff) {
        if ((origBytes[1] & (kMaskMod | kMaskReg)) == 0xf0) {
          // push r64
          COPY_CODES(2);
        } else if (origBytes[1] == 0x25) {
          // jmp absolute indirect m32
          foundJmp = true;

          origBytes += 2;

          uintptr_t jmpDest = origBytes.ChasePointerFromDisp();

          if (!GenerateJump(tramp, jmpDest, JumpType::Jmp)) {
            return;
          }
        } else if ((origBytes[1] & (kMaskMod | kMaskReg)) ==
                   BuildModRmByte(kModReg, 2, 0)) {
          // CALL reg (ff nn)
          COPY_CODES(2);
        } else if (((origBytes[1] & kMaskReg) >> kRegFieldShift) == 4) {
          // JMP r/m
          int len = CountModRmSib(origBytes + 1);
          if (len < 0) {
            // RIP-relative not yet supported
            MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
            return;
          }

          COPY_CODES(len + 1);

          foundJmp = true;
        } else {
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
      } else if (*origBytes == 0x83 && (origBytes[1] & 0xf8) == 0x60) {
        // and [r+d], imm8
        COPY_CODES(5);
      } else if (*origBytes == 0xc6) {
        // mov [r+d], imm8
        int len = CountModRmSib(origBytes + 1);
        if (len < 0) {
          // RIP-relative not yet supported
          MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
          return;
        }
        COPY_CODES(len + 1);
      } else {
        MOZ_ASSERT_UNREACHABLE("Unrecognized opcode sequence");
        return;
      }
    }
#elif defined(_M_ARM64)
    MOZ_RELEASE_ASSERT(false, "Shouldn't get here");
#else
#error "Unknown processor type"
#endif

    if (origBytes.GetOffset() > 100) {
      // printf ("Too big!");
      return;
    }

#if defined(_M_IX86)
    if (pJmp32 >= 0) {
      // Jump directly to the original target of the jump instead of jumping to
      // the original function. Adjust jump target displacement to jump location
      // in the trampoline.
      tramp.AdjustDisp32AtOffset(pJmp32 + 1, origBytes.GetBaseAddress());
    } else {
      tramp.WriteByte(0xe9);  // jmp
      tramp.WriteDisp32(origBytes.GetAddress());
    }
#elif defined(_M_X64)
    // If the we found a Jmp, we don't need to add another instruction. However,
    // if we found a _conditional_ jump or a CALL (or no control operations
    // at all) then we still need to run the rest of aOriginalFunction.
    if (!foundJmp) {
      if (!GenerateJump(tramp, origBytes.GetAddress(), JumpType::Jmp)) {
        return;
      }
    }
#endif

    // The trampoline is now complete.
    void* trampPtr = tramp.EndExecutableCode();
    if (!trampPtr) {
      return;
    }

    WritableTargetFunction<MMPolicyT> target(origBytes.Promote());
    if (!target) {
      return;
    }

#if defined(_M_IX86)
    // now modify the original bytes
    target.WriteByte(0xe9);     // jmp
    target.WriteDisp32(aDest);  // hook displacement
#elif defined(_M_X64)
    if (use10BytePatch) {
      // Okay, now we can write the actual tramp.
      // Note: Even if the target function is also below 2GB, we still use an
      // intermediary trampoline so that we consistently have a 64-bit pointer
      // that we can use to reset the trampoline upon interceptor shutdown.
      Trampoline<MMPolicyT> callTramp(this->mVMPolicy.GetNextTrampoline());
      if (!callTramp) {
        return;
      }

      // Write a null instance so that Clear() does not consider this tramp to
      // be a normal tramp to be torn down.
      callTramp.WriteEncodedPointer(nullptr);
      // Use the second pointer slot to store a pointer to the primary tramp
      callTramp.WriteEncodedPointer(trampPtr);
      callTramp.StartExecutableCode();

      // mov r11, address
      callTramp.WriteByte(0x49);
      callTramp.WriteByte(0xbb);
      callTramp.WritePointer(aDest);

      // jmp r11
      callTramp.WriteByte(0x41);
      callTramp.WriteByte(0xff);
      callTramp.WriteByte(0xe3);

      void* callTrampStart = callTramp.EndExecutableCode();
      if (!callTrampStart) {
        return;
      }

      target.WriteByte(0xB8);  // MOV EAX, IMM32

      // Assert that the topmost 33 bits are 0
      MOZ_ASSERT(
          !(reinterpret_cast<uintptr_t>(callTrampStart) & (~0x7FFFFFFFULL)));

      target.WriteLong(static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(callTrampStart) & 0x7FFFFFFFU));
      target.WriteByte(0x48);  // REX.W
      target.WriteByte(0x63);  // MOVSXD r64, r/m32
      // dest: rax, src: eax
      target.WriteByte(BuildModRmByte(kModReg, kRegAx, kRegAx));
      target.WriteByte(0xFF);                                // JMP /4
      target.WriteByte(BuildModRmByte(kModReg, 4, kRegAx));  // rax
    } else {
      // mov r11, address
      target.WriteByte(0x49);
      target.WriteByte(0xbb);
      target.WritePointer(aDest);

      // jmp r11
      target.WriteByte(0x41);
      target.WriteByte(0xff);
      target.WriteByte(0xe3);
    }
#endif

    if (!target.Commit()) {
      return;
    }

    // Output the trampoline, thus signalling that this call was a success
    *aOutTramp = trampPtr;
  }
};

}  // namespace interceptor
}  // namespace mozilla

#endif  // mozilla_interceptor_PatcherDetour_h
