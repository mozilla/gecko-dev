/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright 2021 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#ifdef JS_SIMULATOR_RISCV64
#include "jit/riscv64/Simulator-riscv64.h"

#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"

#include <float.h>
#include <limits>

#include "jit/AtomicOperations.h"
#include "jit/riscv64/Assembler-riscv64.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "threading/LockGuard.h"
#include "vm/Runtime.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmSignalHandlers.h"

#define I8(v) static_cast<int8_t>(v)
#define I16(v) static_cast<int16_t>(v)
#define U16(v) static_cast<uint16_t>(v)
#define I32(v) static_cast<int32_t>(v)
#define U32(v) static_cast<uint32_t>(v)
#define I64(v) static_cast<int64_t>(v)
#define U64(v) static_cast<uint64_t>(v)
#define I128(v) static_cast<__int128_t>(v)
#define U128(v) static_cast<__uint128_t>(v)

#define REGIx_FORMAT PRIx64
#define REGId_FORMAT PRId64

#define I32_CHECK(v)                   \
  ({                                   \
    MOZ_ASSERT(I64(I32(v)) == I64(v)); \
    I32((v));                          \
  })

namespace js {
namespace jit {

//TODO
static const Instr kCallRedirInstr = 0xfffff;

// Utils functions.
static uint32_t GetFCSRConditionBit(uint32_t cc) {
  if (cc == 0) {
    return 23;
  }
  return 24 + cc;
}

static void UNIMPLEMENTED() {
  printf("UNIMPLEMENTED instruction.\n");
  MOZ_CRASH();
}
static void UNREACHABLE() {
  printf("UNREACHABLE instruction.\n");
  MOZ_CRASH();
}
static void UNSUPPORTED() {
  printf("Unsupported instruction.\n");
  MOZ_CRASH();
}

static char* ReadLine(const char* prompt) {
  UniqueChars result;
  char lineBuf[256];
  int offset = 0;
  bool keepGoing = true;
  fprintf(stdout, "%s", prompt);
  fflush(stdout);
  while (keepGoing) {
    if (fgets(lineBuf, sizeof(lineBuf), stdin) == nullptr) {
      // fgets got an error. Just give up.
      return nullptr;
    }
    int len = strlen(lineBuf);
    if (len > 0 && lineBuf[len - 1] == '\n') {
      // Since we read a new line we are done reading the line. This
      // will exit the loop after copying this buffer into the result.
      keepGoing = false;
    }
    if (!result) {
      // Allocate the initial result and make room for the terminating '\0'
      result.reset(js_pod_malloc<char>(len + 1));
      if (!result) {
        return nullptr;
      }
    } else {
      // Allocate a new result with enough room for the new addition.
      int new_len = offset + len + 1;
      char* new_result = js_pod_malloc<char>(new_len);
      if (!new_result) {
        return nullptr;
      }
      // Copy the existing input into the new array and set the new
      // array as the result.
      memcpy(new_result, result.get(), offset * sizeof(char));
      result.reset(new_result);
    }
    // Copy the newly read line into the result.
    memcpy(result.get() + offset, lineBuf, len * sizeof(char));
    offset += len;
  }

  MOZ_ASSERT(result);
  result[offset] = '\0';
  return result.release();
}

// -----------------------------------------------------------------------------
// MIPS assembly various constants.

class SimInstruction {
 public:
  enum {
    kInstrSize = 4,
    // On MIPS PC cannot actually be directly accessed. We behave as if PC was
    // always the value of the current instruction being executed.
    kPCReadOffset = 0
  };

  // Get the raw instruction bits.
  inline Instr instructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  inline void setinstructionBits(Instr value) {
    *reinterpret_cast<Instr*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int bit(int nr) const { return (instructionBits() >> nr) & 1; }

  // Read a bit field out of the instruction bits.
  inline int bits(int hi, int lo) const {
    return (instructionBits() >> lo) & ((2 << (hi - lo)) - 1);
  }

  // SimInstruction type.
  enum Type { kRegisterType, kImmediateType, kJumpType, kUnsupported = -1 };

  // Get the encoding type of the instruction.
  Type instructionType() const;

 // Accessors for the different named fields used in the RISC-V encoding.
  inline BaseOpcode BaseOpcodeValue() const {
    return static_cast<BaseOpcode>(
        bits(kBaseOpcodeShift + kBaseOpcodeBits - 1, kBaseOpcodeShift));
  }

  inline int Rs1Value() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kRType ||
    //        this->InstructionType() == InstructionBase::kR4Type ||
    //        this->InstructionType() == InstructionBase::kIType ||
    //        this->InstructionType() == InstructionBase::kSType ||
    //        this->InstructionType() == InstructionBase::kBType ||
    //        this->InstructionType() == InstructionBase::kIType ||
    //        this->InstructionType() == InstructionBase::kVType);
    return bits(kRs1Shift + kRs1Bits - 1, kRs1Shift);
  }

  inline int Rs2Value() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kRType ||
    //        this->InstructionType() == InstructionBase::kR4Type ||
    //        this->InstructionType() == InstructionBase::kSType ||
    //        this->InstructionType() == InstructionBase::kBType ||
    //        this->InstructionType() == InstructionBase::kIType ||
    //        this->InstructionType() == InstructionBase::kVType);
    return bits(kRs2Shift + kRs2Bits - 1, kRs2Shift);
  }

  inline int Rs3Value() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kR4Type);
    return bits(kRs3Shift + kRs3Bits - 1, kRs3Shift);
  }

  inline int RdValue() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kRType ||
    //        this->InstructionType() == InstructionBase::kR4Type ||
    //        this->InstructionType() == InstructionBase::kIType ||
    //        this->InstructionType() == InstructionBase::kSType ||
    //        this->InstructionType() == InstructionBase::kUType ||
    //        this->InstructionType() == InstructionBase::kJType ||
    //        this->InstructionType() == InstructionBase::kVType);
    return bits(kRdShift + kRdBits - 1, kRdShift);
  }

  // Return the fields at their original place in the instruction encoding.
  inline BaseOpcode BaseOpcodeFieldRaw() const {
    return static_cast<BaseOpcode>(instructionBits() & kBaseOpcodeMask);
  }

  inline int Imm12Value() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kIType);
    int Value = bits(kImm12Shift + kImm12Bits - 1, kImm12Shift);
    return Value << 20 >> 20;
  }

  inline int Imm20UValue() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kUType);
    // | imm[31:12] | rd | opcode |
    //  31        12
    int32_t Bits = instructionBits();
    return Bits >> 12;
  }

  inline int Imm20JValue() const {
    // MOZ_ASSERT(this->InstructionType() == InstructionBase::kJType);
    // | imm[20|10:1|11|19:12] | rd | opcode |
    //  31                   12
    uint32_t Bits = instructionBits();
    int32_t imm20 = ((Bits & 0x7fe00000) >> 20) | ((Bits & 0x100000) >> 9) |
                    (Bits & 0xff000) | ((Bits & 0x80000000) >> 11);
    return imm20 << 11 >> 11;
  }

  // Say if the instruction 'links'. e.g. jal, bal.
  bool isLinkingInstruction() const;
  // Say if the instruction is a debugger break/trap.
  bool isTrap() const;

 private:
  SimInstruction() = delete;
  SimInstruction(const SimInstruction& other) = delete;
  void operator=(const SimInstruction& other) = delete;
};

bool SimInstruction::isLinkingInstruction() const {
  UNSUPPORTED();
  return false;
}

bool SimInstruction::isTrap() const {
   UNSUPPORTED();
   return false;
}

SimInstruction::Type SimInstruction::instructionType() const {
  UNSUPPORTED();
  return kUnsupported;
}

// C/C++ argument slots size.
const int kCArgSlotCount = 0;
const int kCArgsSlotsSize = kCArgSlotCount * sizeof(uintptr_t);
const int kBranchReturnOffset = 2 * SimInstruction::kInstrSize;

class CachePage {
 public:
  static const int LINE_VALID = 0;
  static const int LINE_INVALID = 1;

  static const int kPageShift = 12;
  static const int kPageSize = 1 << kPageShift;
  static const int kPageMask = kPageSize - 1;
  static const int kLineShift = 2;  // The cache line is only 4 bytes right now.
  static const int kLineLength = 1 << kLineShift;
  static const int kLineMask = kLineLength - 1;

  CachePage() { memset(&validity_map_, LINE_INVALID, sizeof(validity_map_)); }

  char* validityByte(int offset) {
    return &validity_map_[offset >> kLineShift];
  }

  char* cachedData(int offset) { return &data_[offset]; }

 private:
  char data_[kPageSize];  // The cached data.
  static const int kValidityMapSize = kPageSize >> kLineShift;
  char validity_map_[kValidityMapSize];  // One byte per line.
};

// Protects the icache() and redirection() properties of the
// Simulator.
class AutoLockSimulatorCache : public LockGuard<Mutex> {
  using Base = LockGuard<Mutex>;

 public:
  explicit AutoLockSimulatorCache()
      : Base(SimulatorProcess::singleton_->cacheLock_) {}
};

mozilla::Atomic<size_t, mozilla::ReleaseAcquire>
    SimulatorProcess::ICacheCheckingDisableCount(
        1);  // Checking is disabled by default.
SimulatorProcess* SimulatorProcess::singleton_ = nullptr;

int64_t Simulator::StopSimAt = -1;

Simulator* Simulator::Create() {
  auto sim = MakeUnique<Simulator>();
  if (!sim) {
    return nullptr;
  }

  if (!sim->init()) {
    return nullptr;
  }

  int64_t stopAt;
  char* stopAtStr = getenv("MIPS_SIM_STOP_AT");
  if (stopAtStr && sscanf(stopAtStr, "%" PRIi64, &stopAt) == 1) {
    fprintf(stderr, "\nStopping simulation at icount %" PRIi64 "\n", stopAt);
    Simulator::StopSimAt = stopAt;
  }

  return sim.release();
}

void Simulator::Destroy(Simulator* sim) { js_delete(sim); }

// The RiscvDebugger class is used by the simulator while debugging simulated
// code.
class RiscvDebugger {
 public:
  explicit RiscvDebugger(Simulator* sim) : sim_(sim) {}

  void Debug();
  // Print all registers with a nice formatting.
  void PrintRegs(char name_prefix, int start_index, int end_index);
  void printAllRegs();
  void printAllRegsIncludingFPU();

  static const Instr kNopInstr = 0x0;

 private:
  Simulator* sim_;

  int64_t GetRegisterValue(int regnum);
  int64_t GetFPURegisterValue(int regnum);
  float GetFPURegisterValueFloat(int regnum);
  double GetFPURegisterValueDouble(int regnum);
#ifdef CAN_USE_RVV_INSTRUCTIONS
  __int128_t GetVRegisterValue(int regnum);
#endif
  bool GetValue(const char* desc, int64_t* value);
};


int64_t RiscvDebugger::GetRegisterValue(int regnum) {
  if (regnum == Simulator::Register::kNumSimuRegisters) {
    return sim_->get_pc();
  } else {
    return sim_->getRegister(regnum);
  }
}

int64_t RiscvDebugger::GetFPURegisterValue(int regnum) {
  if (regnum == Simulator::FPURegister::kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->getFpuRegister(regnum);
  }
}

float RiscvDebugger::GetFPURegisterValueFloat(int regnum) {
  if (regnum == Simulator::FPURegister::kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->getFpuRegisterFloat(regnum);
  }
}

double RiscvDebugger::GetFPURegisterValueDouble(int regnum) {
  if (regnum == Simulator::FPURegister::kNumFPURegisters) {
    return sim_->get_pc();
  } else {
    return sim_->getFpuRegisterFloat(regnum);
  }
}

#ifdef CAN_USE_RVV_INSTRUCTIONS
__int128_t RiscvDebugger::GetVRegisterValue(int regnum) {
  if (regnum == kNumVRegisters) {
    return sim_->get_pc();
  } else {
    return sim_->get_vregister(regnum);
  }
}
#endif

bool RiscvDebugger::GetValue(const char* desc, int64_t* value) {
  int regnum = Registers::FromName(desc);
  int fpuregnum = FloatRegisters::FromName(desc);

  if (regnum != Registers::invalid_reg) {
    *value = GetRegisterValue(regnum);
    return true;
  } else if (fpuregnum != FloatRegisters::invalid_reg) {
    *value = GetFPURegisterValue(fpuregnum);
    return true;
  } else if (strncmp(desc, "0x", 2) == 0) {
    return scanf(desc + 2, "%" SCNx64, reinterpret_cast<int64_t*>(value)) == 1;
  } else {
    return scanf(desc, "%" SCNu64, reinterpret_cast<int64_t*>(value)) == 1;
  }
}


void RiscvDebugger::PrintRegs(char name_prefix, int start_index,
                              int end_index) {
  MOZ_ASSERT(name_prefix == 'a' || name_prefix == 't' || name_prefix == 's');
  MOZ_ASSERT(start_index >= 0 && end_index <= 99);
  int num_registers = (end_index - start_index) + 1;
  UNSUPPORTED();
}

void RiscvDebugger::printAllRegs() {
  UNSUPPORTED();  
}

#undef REG_INFO

void RiscvDebugger::printAllRegsIncludingFPU() {
  UNSUPPORTED();
}

static void DisassembleInstruction(uint64_t pc) {
  printf("Not supported on loongarch64 yet\n");
  UNSUPPORTED();
}

void RiscvDebugger::Debug() {
  intptr_t last_pc = -1;
  bool done = false;

#define COMMAND_SIZE 63
#define ARG_SIZE 255

#define STR(a) #a
#define XSTR(a) STR(a)

  char cmd[COMMAND_SIZE + 1];
  char arg1[ARG_SIZE + 1];
  char arg2[ARG_SIZE + 1];
  char* argv[3] = {cmd, arg1, arg2};

  // Make sure to have a proper terminating character if reaching the limit.
  cmd[COMMAND_SIZE] = 0;
  arg1[ARG_SIZE] = 0;
  arg2[ARG_SIZE] = 0;

  while (!done && (sim_->get_pc() != Simulator::end_sim_pc)) {
    if (last_pc != sim_->get_pc()) {
      DisassembleInstruction(sim_->get_pc());
      printf("  0x%016" PRIi64 "  \n", sim_->get_pc());
      last_pc = sim_->get_pc();
    }
    char* line = ReadLine("sim> ");
    if (line == nullptr) {
      break;
    } else {
      char* last_input = sim_->lastDebuggerInput();
      if (strcmp(line, "\n") == 0 && last_input != nullptr) {
        line = last_input;
      } else {
        // Ownership is transferred to sim_;
        sim_->setLastDebuggerInput(line);
      }
      // Use sscanf to parse the individual parts of the command line. At the
      // moment no command expects more than two parameters.
      int argc = scanf(
            line,
            "%" XSTR(COMMAND_SIZE) "s "
            "%" XSTR(ARG_SIZE) "s "
            "%" XSTR(ARG_SIZE) "s",
            cmd, arg1, arg2);
      if ((strcmp(cmd, "si") == 0) || (strcmp(cmd, "stepi") == 0)) {
        SimInstruction* instr =
            reinterpret_cast<SimInstruction*>(sim_->get_pc());
        if (!(instr->isTrap()) ||
            instr->instructionBits() == rtCallRedirInstr) {
          sim_->icount_++;
          sim_->instructionDecode(
              reinterpret_cast<SimInstruction*>(sim_->get_pc()));
        } else {
          // Allow si to jump over generated breakpoints.
          printf("/!\\ Jumping over generated breakpoint.\n");
          sim_->set_pc(sim_->get_pc() + kInstrSize);
        }
      } else if ((strcmp(cmd, "c") == 0) || (strcmp(cmd, "cont") == 0)) {
        // Execute the one instruction we broke at with breakpoints disabled.
        sim_->instructionDecode(reinterpret_cast<SimInstruction*>(sim_->get_pc()));
        // Leave the debugger shell.
        done = true;
      } else if ((strcmp(cmd, "p") == 0) || (strcmp(cmd, "print") == 0)) {
        if (argc == 2) {
          int64_t value;
          int64_t fvalue;
          double dvalue;
          if (strcmp(arg1, "all") == 0) {
            printAllRegs();
          } else if (strcmp(arg1, "allf") == 0) {
            printAllRegsIncludingFPU();
          } else {
            int regnum = Registers::FromName(arg1);
            int fpuregnum = FloatRegisters::FromName(arg1);
#ifdef CAN_USE_RVV_INSTRUCTIONS
            int vregnum = VRegisters::FromName(arg1);
#endif
            if (regnum != Registers::invalid_reg) {
              value = GetRegisterValue(regnum);
              printf("%s: 0x%08" REGIx_FORMAT "  %" REGId_FORMAT "  \n", arg1,
                     value, value);
            } else if (fpuregnum != FloatRegisters::invalid_reg) {
              fvalue = GetFPURegisterValue(fpuregnum);
              dvalue = GetFPURegisterValueDouble(fpuregnum);
              printf("%3s: 0x%016" PRIx64 "  %16.4e\n",
                     FloatRegisters::GetName(fpuregnum), fvalue, dvalue);
#ifdef CAN_USE_RVV_INSTRUCTIONS
            } else if (vregnum != kInvalidVRegister) {
              __int128_t v = GetVRegisterValue(vregnum);
              printf("\t%s:0x%016" REGIx_FORMAT "%016" REGIx_FORMAT "\n",
                     VRegisters::GetName(vregnum), (uint64_t)(v >> 64),
                     (uint64_t)v);
#endif
            } else {
              printf("%s unrecognized\n", arg1);
            }
          }
        } else {
          if (argc == 3) {
            if (strcmp(arg2, "single") == 0) {
              int64_t value;
              float fvalue;
              int fpuregnum = FloatRegisters::FromName(arg1);

              if (fpuregnum != FloatRegisters::invalid_reg) {
                value = GetFPURegisterValue(fpuregnum);
                value &= 0xFFFFFFFFUL;
                fvalue = GetFPURegisterValueFloat(fpuregnum);
                printf("%s: 0x%08" PRIx64 "  %11.4e\n", arg1, value, fvalue);
              } else {
                printf("%s unrecognized\n", arg1);
              }
            } else {
              printf("print <fpu register> single\n");
            }
          } else {
            printf("print <register> or print <fpu register> single\n");
          }
        }
      } else if ((strcmp(cmd, "po") == 0) ||
                 (strcmp(cmd, "printobject") == 0)) {
         UNSUPPORTED();
      } else if (strcmp(cmd, "stack") == 0 || strcmp(cmd, "mem") == 0) {
        int64_t* cur = nullptr;
        int64_t* end = nullptr;
        int next_arg = 1;
        if (argc < 2) {
          printf("Need to specify <address> to memhex command\n");
          continue;
        }
        int64_t value;
        if (!GetValue(arg1, &value)) {
          printf("%s unrecognized\n", arg1);
          continue;
        }
        cur = reinterpret_cast<int64_t*>(value);
        next_arg++;

        int64_t words;
        if (argc == next_arg) {
          words = 10;
        } else {
          if (!GetValue(argv[next_arg], &words)) {
            words = 10;
          }
        }
        end = cur + words;

        while (cur < end) {
          printf("  0x%012" PRIxPTR " :  0x%016" REGIx_FORMAT
                 "  %14" REGId_FORMAT " ",
                 reinterpret_cast<intptr_t>(cur), *cur, *cur);
          printf("\n");
          cur++;
        }
      } else if ((strcmp(cmd, "watch") == 0)) {
        if (argc < 2) {
          printf("Need to specify <address> to mem command\n");
          continue;
        }
        int64_t value;
        if (!GetValue(arg1, &value)) {
          printf("%s unrecognized\n", arg1);
          continue;
        }
        sim_->watch_address_ = reinterpret_cast<int64_t*>(value);
        sim_->watch_value_ = *(sim_->watch_address_);
      } else if ((strcmp(cmd, "disasm") == 0) || (strcmp(cmd, "dpc") == 0) ||
                 (strcmp(cmd, "di") == 0)) {
        UNSUPPORTED();
      } else if (strcmp(cmd, "trace") == 0) {
         UNSUPPORTED();
      } else if (strcmp(cmd, "break") == 0 || strcmp(cmd, "b") == 0 ||
                 strcmp(cmd, "tbreak") == 0) {
        bool is_tbreak = strcmp(cmd, "tbreak") == 0;
        if (argc == 2) {
          int64_t value;
          if (GetValue(arg1, &value)) {
            sim_->SetBreakpoint(reinterpret_cast<SimInstruction*>(value),
                                is_tbreak);
          } else {
            printf("%s unrecognized\n", arg1);
          }
        } else {
          sim_->ListBreakpoints();
          printf("Use `break <address>` to set or disable a breakpoint\n");
          printf(
              "Use `tbreak <address>` to set or disable a temporary "
              "breakpoint\n");
        }
      } else if (strcmp(cmd, "flags") == 0) {
        printf("No flags on RISC-V !\n");
      } else if (strcmp(cmd, "stop") == 0) {
        int64_t value;
        if (argc == 3) {
          // Print information about all/the specified breakpoint(s).
          if (strcmp(arg1, "info") == 0) {
            if (strcmp(arg2, "all") == 0) {
              printf("Stop information:\n");
              for (uint32_t i = kMaxWatchpointCode + 1; i <= kMaxStopCode;
                   i++) {
                sim_->printStopInfo(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->printStopInfo(value);
            } else {
              printf("Unrecognized argument.\n");
            }
          } else if (strcmp(arg1, "enable") == 0) {
            // Enable all/the specified breakpoint(s).
            if (strcmp(arg2, "all") == 0) {
              for (uint32_t i = kMaxWatchpointCode + 1; i <= kMaxStopCode;
                   i++) {
                sim_->enableStop(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->enableStop(value);
            } else {
              printf("Unrecognized argument.\n");
            }
          } else if (strcmp(arg1, "disable") == 0) {
            // Disable all/the specified breakpoint(s).
            if (strcmp(arg2, "all") == 0) {
              for (uint32_t i = kMaxWatchpointCode + 1; i <= kMaxStopCode;
                   i++) {
                sim_->disableStop(i);
              }
            } else if (GetValue(arg2, &value)) {
              sim_->disableStop(value);
            } else {
              printf("Unrecognized argument.\n");
            }
          }
        } else {
          printf("Wrong usage. Use help command for more information.\n");
        }
      } else if ((strcmp(cmd, "stat") == 0) || (strcmp(cmd, "st") == 0)) {
         UNSUPPORTED();
      } else if ((strcmp(cmd, "h") == 0) || (strcmp(cmd, "help") == 0)) {
        printf("cont (alias 'c')\n");
        printf("  Continue execution\n");
        printf("stepi (alias 'si')\n");
        printf("  Step one instruction\n");
        printf("print (alias 'p')\n");
        printf("  print <register>\n");
        printf("  Print register content\n");
        printf("  Use register name 'all' to print all GPRs\n");
        printf("  Use register name 'allf' to print all GPRs and FPRs\n");
        printf("printobject (alias 'po')\n");
        printf("  printobject <register>\n");
        printf("  Print an object from a register\n");
        printf("stack\n");
        printf("  stack [<words>]\n");
        printf("  Dump stack content, default dump 10 words)\n");
        printf("mem\n");
        printf("  mem <address> [<words>]\n");
        printf("  Dump memory content, default dump 10 words)\n");
        printf("watch\n");
        printf("  watch <address> \n");
        printf("  watch memory content.)\n");
        printf("flags\n");
        printf("  print flags\n");
        printf("disasm (alias 'di')\n");
        printf("  disasm [<instructions>]\n");
        printf("  disasm [<address/register>] (e.g., disasm pc) \n");
        printf("  disasm [[<address/register>] <instructions>]\n");
        printf("  Disassemble code, default is 10 instructions\n");
        printf("  from pc\n");
        printf("gdb \n");
        printf("  Return to gdb if the simulator was started with gdb\n");
        printf("break (alias 'b')\n");
        printf("  break : list all breakpoints\n");
        printf("  break <address> : set / enable / disable a breakpoint.\n");
        printf("tbreak\n");
        printf("  tbreak : list all breakpoints\n");
        printf(
            "  tbreak <address> : set / enable / disable a temporary "
            "breakpoint.\n");
        printf("  Set a breakpoint enabled only for one stop. \n");
        printf("stop feature:\n");
        printf("  Description:\n");
        printf("    Stops are debug instructions inserted by\n");
        printf("    the Assembler::stop() function.\n");
        printf("    When hitting a stop, the Simulator will\n");
        printf("    stop and give control to the Debugger.\n");
        printf("    All stop codes are watched:\n");
        printf("    - They can be enabled / disabled: the Simulator\n");
        printf("       will / won't stop when hitting them.\n");
        printf("    - The Simulator keeps track of how many times they \n");
        printf("      are met. (See the info command.) Going over a\n");
        printf("      disabled stop still increases its counter. \n");
        printf("  Commands:\n");
        printf("    stop info all/<code> : print infos about number <code>\n");
        printf("      or all stop(s).\n");
        printf("    stop enable/disable all/<code> : enables / disables\n");
        printf("      all or number <code> stop(s)\n");
      } else {
        printf("Unknown command: %s\n", cmd);
      }
    }
  }

#undef COMMAND_SIZE
#undef ARG_SIZE

#undef STR
#undef XSTR
}


void Simulator::SetBreakpoint(SimInstruction* location, bool is_tbreak) {
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    if (breakpoints_.at(i).location == location) {
      if (breakpoints_.at(i).is_tbreak != is_tbreak) {
        printf("Change breakpoint at %p to %s breakpoint\n",
               reinterpret_cast<void*>(location),
               is_tbreak ? "temporary" : "regular");
        breakpoints_.at(i).is_tbreak = is_tbreak;
        return;
      }
      printf("Existing breakpoint at %p was %s\n",
             reinterpret_cast<void*>(location),
             breakpoints_.at(i).enabled ? "disabled" : "enabled");
      breakpoints_.at(i).enabled = !breakpoints_.at(i).enabled;
      return;
    }
  }
  Breakpoint new_breakpoint = {location, true, is_tbreak};
  breakpoints_.push_back(new_breakpoint);
  printf("Set a %sbreakpoint at %p\n", is_tbreak ? "temporary " : "",
         reinterpret_cast<void*>(location));
}

void Simulator::ListBreakpoints() {
  printf("Breakpoints:\n");
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    printf("%p  : %s %s\n",
           reinterpret_cast<void*>(breakpoints_.at(i).location),
           breakpoints_.at(i).enabled ? "enabled" : "disabled",
           breakpoints_.at(i).is_tbreak ? ": temporary" : "");
  }
}

void Simulator::CheckBreakpoints() {
  bool hit_a_breakpoint = false;
  bool is_tbreak = false;
  SimInstruction* pc_ = reinterpret_cast<SimInstruction*>(get_pc());
  for (unsigned i = 0; i < breakpoints_.size(); i++) {
    if ((breakpoints_.at(i).location == pc_) && breakpoints_.at(i).enabled) {
      hit_a_breakpoint = true;
      if (breakpoints_.at(i).is_tbreak) {
        // Disable a temporary breakpoint.
        is_tbreak = true;
        breakpoints_.at(i).enabled = false;
      }
      break;
    }
  }
  if (hit_a_breakpoint) {
    printf("Hit %sa breakpoint at %p.\n", is_tbreak ? "and disabled " : "",
           reinterpret_cast<void*>(pc_));
    RiscvDebugger dbg(this);
    dbg.Debug();
  }
}

static bool AllOnOnePage(uintptr_t start, int size) {
  intptr_t start_page = (start & ~CachePage::kPageMask);
  intptr_t end_page = ((start + size) & ~CachePage::kPageMask);
  return start_page == end_page;
}

void Simulator::setLastDebuggerInput(char* input) {
  js_free(lastDebuggerInput_);
  lastDebuggerInput_ = input;
}

static CachePage* GetCachePageLocked(SimulatorProcess::ICacheMap& i_cache,
                                     void* page) {
  SimulatorProcess::ICacheMap::AddPtr p = i_cache.lookupForAdd(page);
  if (p) {
    return p->value();
  }
  AutoEnterOOMUnsafeRegion oomUnsafe;
  CachePage* new_page = js_new<CachePage>();
  if (!new_page || !i_cache.add(p, page, new_page)) {
    oomUnsafe.crash("Simulator CachePage");
  }
  return new_page;
}

// Flush from start up to and not including start + size.
static void FlushOnePageLocked(SimulatorProcess::ICacheMap& i_cache,
                               intptr_t start, int size) {
  MOZ_ASSERT(size <= CachePage::kPageSize);
  MOZ_ASSERT(AllOnOnePage(start, size - 1));
  MOZ_ASSERT((start & CachePage::kLineMask) == 0);
  MOZ_ASSERT((size & CachePage::kLineMask) == 0);
  void* page = reinterpret_cast<void*>(start & (~CachePage::kPageMask));
  int offset = (start & CachePage::kPageMask);
  CachePage* cache_page = GetCachePageLocked(i_cache, page);
  char* valid_bytemap = cache_page->validityByte(offset);
  memset(valid_bytemap, CachePage::LINE_INVALID, size >> CachePage::kLineShift);
}

static void FlushICacheLocked(SimulatorProcess::ICacheMap& i_cache,
                              void* start_addr, size_t size) {
  intptr_t start = reinterpret_cast<intptr_t>(start_addr);
  int intra_line = (start & CachePage::kLineMask);
  start -= intra_line;
  size += intra_line;
  size = ((size - 1) | CachePage::kLineMask) + 1;
  int offset = (start & CachePage::kPageMask);
  while (!AllOnOnePage(start, size - 1)) {
    int bytes_to_flush = CachePage::kPageSize - offset;
    FlushOnePageLocked(i_cache, start, bytes_to_flush);
    start += bytes_to_flush;
    size -= bytes_to_flush;
    MOZ_ASSERT((start & CachePage::kPageMask) == 0);
    offset = 0;
  }
  if (size != 0) {
    FlushOnePageLocked(i_cache, start, size);
  }
}

/* static */
void SimulatorProcess::checkICacheLocked(SimInstruction* instr) {
  intptr_t address = reinterpret_cast<intptr_t>(instr);
  void* page = reinterpret_cast<void*>(address & (~CachePage::kPageMask));
  void* line = reinterpret_cast<void*>(address & (~CachePage::kLineMask));
  int offset = (address & CachePage::kPageMask);
  CachePage* cache_page = GetCachePageLocked(icache(), page);
  char* cache_valid_byte = cache_page->validityByte(offset);
  bool cache_hit = (*cache_valid_byte == CachePage::LINE_VALID);
  char* cached_line = cache_page->cachedData(offset & ~CachePage::kLineMask);

  if (cache_hit) {
    // Check that the data in memory matches the contents of the I-cache.
    int cmpret =
        memcmp(reinterpret_cast<void*>(instr), cache_page->cachedData(offset),
               SimInstruction::kInstrSize);
    MOZ_ASSERT(cmpret == 0);
  } else {
    // Cache miss.  Load memory into the cache.
    memcpy(cached_line, line, CachePage::kLineLength);
    *cache_valid_byte = CachePage::LINE_VALID;
  }
}

HashNumber SimulatorProcess::ICacheHasher::hash(const Lookup& l) {
  return U32(reinterpret_cast<uintptr_t>(l)) >> 2;
}

bool SimulatorProcess::ICacheHasher::match(const Key& k, const Lookup& l) {
  MOZ_ASSERT((reinterpret_cast<intptr_t>(k) & CachePage::kPageMask) == 0);
  MOZ_ASSERT((reinterpret_cast<intptr_t>(l) & CachePage::kPageMask) == 0);
  return k == l;
}

/* static */
void SimulatorProcess::FlushICache(void* start_addr, size_t size) {
  if (!ICacheCheckingDisableCount) {
    AutoLockSimulatorCache als;
    js::jit::FlushICacheLocked(icache(), start_addr, size);
  }
}

Simulator::Simulator() {
  // Set up simulator support first. Some of this information is needed to
  // setup the architecture state.

  // Note, allocation and anything that depends on allocated memory is
  // deferred until init(), in order to handle OOM properly.

  stack_ = nullptr;
  stackLimit_ = 0;
  pc_modified_ = false;
  icount_ = 0;
  break_count_ = 0;
  break_pc_ = nullptr;
  break_instr_ = 0;
  single_stepping_ = false;
  single_step_callback_ = nullptr;
  single_step_callback_arg_ = nullptr;

  // Set up architecture state.
  // All registers are initialized to zero to start with.
  for (int i = 0; i < Simulator::Register::kNumSimuRegisters; i++) {
    registers_[i] = 0;
  }
  for (int i = 0; i < Simulator::FPURegister::kNumFPURegisters; i++) {
    FPUregisters_[i] = 0;
  }
  FCSR_ = 0;
  LLBit_ = false;
  LLAddr_ = 0;
  lastLLValue_ = 0;

  // The ra and pc are initialized to a known bad value that will cause an
  // access violation if the simulator ever tries to execute it.
  registers_[pc] = bad_ra;
  registers_[ra] = bad_ra;

  for (int i = 0; i < kNumExceptions; i++) {
    exceptions[i] = 0;
  }

  lastDebuggerInput_ = nullptr;
}

bool Simulator::init() {
  // Allocate 2MB for the stack. Note that we will only use 1MB, see below.
  static const size_t stackSize = 2 * 1024 * 1024;
  stack_ = js_pod_malloc<char>(stackSize);
  if (!stack_) {
    return false;
  }

  // Leave a safety margin of 1MB to prevent overrunning the stack when
  // pushing values (total stack size is 2MB).
  stackLimit_ = reinterpret_cast<uintptr_t>(stack_) + 1024 * 1024;

  // The sp is initialized to point to the bottom (high address) of the
  // allocated stack area. To be safe in potential stack underflows we leave
  // some buffer below.
  registers_[sp] = reinterpret_cast<int64_t>(stack_) + stackSize - 64;

  return true;
}

// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a swi (software-interrupt) instruction that is handled by
// the simulator.  We write the original destination of the jump just at a known
// offset from the swi instruction so the simulator knows what to call.
class Redirection {
  friend class SimulatorProcess;

  // sim's lock must already be held.
  Redirection(void* nativeFunction, ABIFunctionType type)
      : nativeFunction_(nativeFunction),
        swiInstruction_(kCallRedirInstr),
        type_(type),
        next_(nullptr) {
    next_ = SimulatorProcess::redirection();
    if (!SimulatorProcess::ICacheCheckingDisableCount) {
      FlushICacheLocked(SimulatorProcess::icache(), addressOfSwiInstruction(),
                        SimInstruction::kInstrSize);
    }
    SimulatorProcess::setRedirection(this);
  }

 public:
  void* addressOfSwiInstruction() { return &swiInstruction_; }
  void* nativeFunction() const { return nativeFunction_; }
  ABIFunctionType type() const { return type_; }

  static Redirection* Get(void* nativeFunction, ABIFunctionType type) {
    AutoLockSimulatorCache als;

    Redirection* current = SimulatorProcess::redirection();
    for (; current != nullptr; current = current->next_) {
      if (current->nativeFunction_ == nativeFunction) {
        MOZ_ASSERT(current->type() == type);
        return current;
      }
    }

    // Note: we can't use js_new here because the constructor is private.
    AutoEnterOOMUnsafeRegion oomUnsafe;
    Redirection* redir = js_pod_malloc<Redirection>(1);
    if (!redir) {
      oomUnsafe.crash("Simulator redirection");
    }
    new (redir) Redirection(nativeFunction, type);
    return redir;
  }

  static Redirection* FromSwiInstruction(SimInstruction* swiInstruction) {
    uint8_t* addrOfSwi = reinterpret_cast<uint8_t*>(swiInstruction);
    uint8_t* addrOfRedirection =
        addrOfSwi - offsetof(Redirection, swiInstruction_);
    return reinterpret_cast<Redirection*>(addrOfRedirection);
  }

 private:
  void* nativeFunction_;
  uint32_t swiInstruction_;
  ABIFunctionType type_;
  Redirection* next_;
};

Simulator::~Simulator() { js_free(stack_); }

SimulatorProcess::SimulatorProcess()
    : cacheLock_(mutexid::SimulatorCacheLock), redirection_(nullptr) {
  if (getenv("MIPS_SIM_ICACHE_CHECKS")) {
    ICacheCheckingDisableCount = 0;
  }
}

SimulatorProcess::~SimulatorProcess() {
  Redirection* r = redirection_;
  while (r) {
    Redirection* next = r->next_;
    js_delete(r);
    r = next;
  }
}

/* static */
void* Simulator::RedirectNativeFunction(void* nativeFunction,
                                        ABIFunctionType type) {
  Redirection* redirection = Redirection::Get(nativeFunction, type);
  return redirection->addressOfSwiInstruction();
}

// Get the active Simulator for the current thread.
Simulator* Simulator::Current() {
  JSContext* cx = TlsContext.get();
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
  return cx->simulator();
}

// Sets the register in the architecture state. It will also deal with updating
// Simulator internal state for special registers such as PC.
void Simulator::setRegister(int reg, int64_t value) {
  MOZ_ASSERT((reg >= 0) && (reg < Simulator::FPURegister::kNumFPURegisters));
  if (reg == pc) {
    pc_modified_ = true;
  }

  // Zero register always holds 0.
  registers_[reg] = (reg == 0) ? 0 : value;
}

void Simulator::setFpuRegister(int fpureg, int64_t value) {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  FPUregisters_[fpureg] = value;
}

void Simulator::setFpuRegisterLo(int fpureg, int32_t value) {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  *mozilla::BitwiseCast<int32_t*>(&FPUregisters_[fpureg]) = value;
}

void Simulator::setFpuRegisterHi(int fpureg, int32_t value) {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  *((mozilla::BitwiseCast<int32_t*>(&FPUregisters_[fpureg])) + 1) = value;
}

void Simulator::setFpuRegisterFloat(int fpureg, float value) {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  *mozilla::BitwiseCast<float*>(&FPUregisters_[fpureg]) = value;
}

void Simulator::setFpuRegisterDouble(int fpureg, double value) {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  *mozilla::BitwiseCast<double*>(&FPUregisters_[fpureg]) = value;
}

// Get the register from the architecture state. This function does handle
// the special case of accessing the PC register.
int64_t Simulator::getRegister(int reg) const {
  MOZ_ASSERT((reg >= 0) && (reg < Simulator::FPURegister::kNumFPURegisters));
  if (reg == 0) {
    return 0;
  }
  return registers_[reg] + ((reg == pc) ? SimInstruction::kPCReadOffset : 0);
}

int64_t Simulator::getFpuRegister(int fpureg) const {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  return FPUregisters_[fpureg];
}

int32_t Simulator::getFpuRegisterLo(int fpureg) const {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  return *mozilla::BitwiseCast<int32_t*>(&FPUregisters_[fpureg]);
}

int32_t Simulator::getFpuRegisterHi(int fpureg) const {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  return *((mozilla::BitwiseCast<int32_t*>(&FPUregisters_[fpureg])) + 1);
}

float Simulator::getFpuRegisterFloat(int fpureg) const {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  return *mozilla::BitwiseCast<float*>(&FPUregisters_[fpureg]);
}

double Simulator::getFpuRegisterDouble(int fpureg) const {
  MOZ_ASSERT((fpureg >= 0) &&
             (fpureg < Simulator::FPURegister::kNumFPURegisters));
  return *mozilla::BitwiseCast<double*>(&FPUregisters_[fpureg]);
}

void Simulator::setCallResultDouble(double result) {
  setFpuRegisterDouble(fa0, result);
}

void Simulator::setCallResultFloat(float result) {
  setFpuRegisterFloat(fa0, result);
}

void Simulator::setCallResult(int64_t res) { setRegister(a0, res); }

void Simulator::setCallResult(__int128_t res) {
  setRegister(a0, I64(res));
  setRegister(a1, I64(res >> 64));
}

// Helper functions for setting and testing the FCSR register's bits.
void Simulator::setFCSRBit(uint32_t cc, bool value) {
  if (value) {
    FCSR_ |= (1 << cc);
  } else {
    FCSR_ &= ~(1 << cc);
  }
}

bool Simulator::testFCSRBit(uint32_t cc) { return FCSR_ & (1 << cc); }

// Sets the rounding error codes in FCSR based on the result of the rounding.
// Returns true if the operation was invalid.
template <typename T>
bool Simulator::setFCSRRoundError(double original, double rounded) {
  bool ret = false;

  setFCSRBit(kFCSRInexactCauseBit, false);
  setFCSRBit(kFCSRUnderflowCauseBit, false);
  setFCSRBit(kFCSROverflowCauseBit, false);
  setFCSRBit(kFCSRInvalidOpCauseBit, false);

  if (!std::isfinite(original) || !std::isfinite(rounded)) {
    setFCSRBit(kFCSRInvalidOpFlagBit, true);
    setFCSRBit(kFCSRInvalidOpCauseBit, true);
    ret = true;
  }

  if (original != rounded) {
    setFCSRBit(kFCSRInexactFlagBit, true);
    setFCSRBit(kFCSRInexactCauseBit, true);
  }

  if (rounded < DBL_MIN && rounded > -DBL_MIN && rounded != 0) {
    setFCSRBit(kFCSRUnderflowFlagBit, true);
    setFCSRBit(kFCSRUnderflowCauseBit, true);
    ret = true;
  }

  if ((long double)rounded > (long double)std::numeric_limits<T>::max() ||
      (long double)rounded < (long double)std::numeric_limits<T>::min()) {
    setFCSRBit(kFCSROverflowFlagBit, true);
    setFCSRBit(kFCSROverflowCauseBit, true);
    // The reference is not really clear but it seems this is required:
    setFCSRBit(kFCSRInvalidOpFlagBit, true);
    setFCSRBit(kFCSRInvalidOpCauseBit, true);
    ret = true;
  }

  return ret;
}

// Raw access to the PC register.
void Simulator::set_pc(int64_t value) {
  pc_modified_ = true;
  registers_[pc] = value;
}

bool Simulator::has_bad_pc() const {
  return ((registers_[pc] == bad_ra) || (registers_[pc] == end_sim_pc));
}

// Raw access to the PC register without the special adjustment when reading.
int64_t Simulator::get_pc() const { return registers_[pc]; }

JS::ProfilingFrameIterator::RegisterState Simulator::registerState() {
  wasm::RegisterState state;
  state.pc = (void*)get_pc();
  state.fp = (void*)getRegister(fp);
  state.sp = (void*)getRegister(sp);
  state.lr = (void*)getRegister(ra);
  return state;
}

static bool AllowUnaligned() {
  static bool hasReadFlag = false;
  static bool unalignedAllowedFlag = false;
  if (!hasReadFlag) {
    unalignedAllowedFlag = !!getenv("MIPS_UNALIGNED");
    hasReadFlag = true;
  }
  return unalignedAllowedFlag;
}

// MIPS memory instructions (except lw(d)l/r , sw(d)l/r) trap on unaligned
// memory access enabling the OS to handle them via trap-and-emulate. Note that
// simulator runs have the runtime system running directly on the host system
// and only generated code is executed in the simulator. Since the host is
// typically IA32 it will not trap on unaligned memory access. We assume that
// that executing correct generated code will not produce unaligned memory
// access, so we explicitly check for address alignment and trap. Note that
// trapping does not occur when executing wasm code, which requires that
// unaligned memory access provides correct result.

uint8_t Simulator::readBU(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 1)) {
    return 0xff;
  }

  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  return *ptr;
}

int8_t Simulator::readB(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 1)) {
    return -1;
  }

  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  return *ptr;
}

void Simulator::writeB(uint64_t addr, uint8_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 1)) {
    return;
  }

  uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
  *ptr = value;
}

void Simulator::writeB(uint64_t addr, int8_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 1)) {
    return;
  }

  int8_t* ptr = reinterpret_cast<int8_t*>(addr);
  *ptr = value;
}

uint16_t Simulator::readHU(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 2)) {
    return 0xffff;
  }

  if (AllowUnaligned() || (addr & 1) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    return *ptr;
  }
  printf("Unaligned unsigned halfword read at 0x%016" PRIx64
         ", pc=0x%016" PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

int16_t Simulator::readH(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 2)) {
    return -1;
  }

  if (AllowUnaligned() || (addr & 1) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    return *ptr;
  }
  printf("Unaligned signed halfword read at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR
         "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

void Simulator::writeH(uint64_t addr, uint16_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 2)) {
    return;
  }

  if (AllowUnaligned() || (addr & 1) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    uint16_t* ptr = reinterpret_cast<uint16_t*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned unsigned halfword write at 0x%016" PRIx64
         ", pc=0x%016" PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

void Simulator::writeH(uint64_t addr, int16_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 2)) {
    return;
  }

  if (AllowUnaligned() || (addr & 1) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    int16_t* ptr = reinterpret_cast<int16_t*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned halfword write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

uint32_t Simulator::readWU(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 4)) {
    return -1;
  }

  if (AllowUnaligned() || (addr & 3) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
    return *ptr;
  }
  printf("Unaligned read at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

int32_t Simulator::readW(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 4)) {
    return -1;
  }

  if (AllowUnaligned() || (addr & 3) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    int32_t* ptr = reinterpret_cast<int32_t*>(addr);
    return *ptr;
  }
  printf("Unaligned read at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

void Simulator::writeW(uint64_t addr, uint32_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 4)) {
    return;
  }

  if (AllowUnaligned() || (addr & 3) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

void Simulator::writeW(uint64_t addr, int32_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 4)) {
    return;
  }

  if (AllowUnaligned() || (addr & 3) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    int32_t* ptr = reinterpret_cast<int32_t*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

int64_t Simulator::readDW(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 8)) {
    return -1;
  }

  if (AllowUnaligned() || (addr & kPointerAlignmentMask) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    intptr_t* ptr = reinterpret_cast<intptr_t*>(addr);
    return *ptr;
  }
  printf("Unaligned read at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

void Simulator::writeDW(uint64_t addr, int64_t value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 8)) {
    return;
  }

  if (AllowUnaligned() || (addr & kPointerAlignmentMask) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    int64_t* ptr = reinterpret_cast<int64_t*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

double Simulator::readD(uint64_t addr, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 8)) {
    return NAN;
  }

  if (AllowUnaligned() || (addr & kDoubleAlignmentMask) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    double* ptr = reinterpret_cast<double*>(addr);
    return *ptr;
  }
  printf("Unaligned (double) read at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

void Simulator::writeD(uint64_t addr, double value, SimInstruction* instr) {
  if (handleWasmSegFault(addr, 8)) {
    return;
  }

  if (AllowUnaligned() || (addr & kDoubleAlignmentMask) == 0 ||
      wasm::InCompiledCode(reinterpret_cast<void*>(get_pc()))) {
    double* ptr = reinterpret_cast<double*>(addr);
    LLBit_ = false;
    *ptr = value;
    return;
  }
  printf("Unaligned (double) write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n",
         addr, reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
}

int Simulator::loadLinkedW(uint64_t addr, SimInstruction* instr) {
  if ((addr & 3) == 0) {
    if (handleWasmSegFault(addr, 4)) {
      return -1;
    }

    volatile int32_t* ptr = reinterpret_cast<volatile int32_t*>(addr);
    int32_t value = *ptr;
    lastLLValue_ = value;
    LLAddr_ = addr;
    // Note that any memory write or "external" interrupt should reset this
    // value to false.
    LLBit_ = true;
    return value;
  }
  printf("Unaligned write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

int Simulator::storeConditionalW(uint64_t addr, int value,
                                 SimInstruction* instr) {
  // Correct behavior in this case, as defined by architecture, is to just
  // return 0, but there is no point at allowing that. It is certainly an
  // indicator of a bug.
  if (addr != LLAddr_) {
    printf("SC to bad address: 0x%016" PRIx64 ", pc=0x%016" PRIx64
           ", expected: 0x%016" PRIx64 "\n",
           addr, reinterpret_cast<intptr_t>(instr), LLAddr_);
    MOZ_CRASH();
  }

  if ((addr & 3) == 0) {
    SharedMem<int32_t*> ptr =
        SharedMem<int32_t*>::shared(reinterpret_cast<int32_t*>(addr));

    if (!LLBit_) {
      return 0;
    }

    LLBit_ = false;
    LLAddr_ = 0;
    int32_t expected = int32_t(lastLLValue_);
    int32_t old =
        AtomicOperations::compareExchangeSeqCst(ptr, expected, int32_t(value));
    return (old == expected) ? 1 : 0;
  }
  printf("Unaligned SC at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

int64_t Simulator::loadLinkedD(uint64_t addr, SimInstruction* instr) {
  if ((addr & kPointerAlignmentMask) == 0) {
    if (handleWasmSegFault(addr, 8)) {
      return -1;
    }

    volatile int64_t* ptr = reinterpret_cast<volatile int64_t*>(addr);
    int64_t value = *ptr;
    lastLLValue_ = value;
    LLAddr_ = addr;
    // Note that any memory write or "external" interrupt should reset this
    // value to false.
    LLBit_ = true;
    return value;
  }
  printf("Unaligned write at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

int Simulator::storeConditionalD(uint64_t addr, int64_t value,
                                 SimInstruction* instr) {
  // Correct behavior in this case, as defined by architecture, is to just
  // return 0, but there is no point at allowing that. It is certainly an
  // indicator of a bug.
  if (addr != LLAddr_) {
    printf("SC to bad address: 0x%016" PRIx64 ", pc=0x%016" PRIx64
           ", expected: 0x%016" PRIx64 "\n",
           addr, reinterpret_cast<intptr_t>(instr), LLAddr_);
    MOZ_CRASH();
  }

  if ((addr & kPointerAlignmentMask) == 0) {
    SharedMem<int64_t*> ptr =
        SharedMem<int64_t*>::shared(reinterpret_cast<int64_t*>(addr));

    if (!LLBit_) {
      return 0;
    }

    LLBit_ = false;
    LLAddr_ = 0;
    int64_t expected = lastLLValue_;
    int64_t old =
        AtomicOperations::compareExchangeSeqCst(ptr, expected, int64_t(value));
    return (old == expected) ? 1 : 0;
  }
  printf("Unaligned SC at 0x%016" PRIx64 ", pc=0x%016" PRIxPTR "\n", addr,
         reinterpret_cast<intptr_t>(instr));
  MOZ_CRASH();
  return 0;
}

uintptr_t Simulator::stackLimit() const { return stackLimit_; }

uintptr_t* Simulator::addressOfStackLimit() { return &stackLimit_; }

bool Simulator::overRecursed(uintptr_t newsp) const {
  if (newsp == 0) {
    newsp = getRegister(sp);
  }
  return newsp <= stackLimit();
}

bool Simulator::overRecursedWithExtra(uint32_t extra) const {
  uintptr_t newsp = getRegister(sp) - extra;
  return newsp <= stackLimit();
}

// Unsupported instructions use format to print an error and stop execution.
void Simulator::format(SimInstruction* instr, const char* format) {
  printf("Simulator found unsupported instruction:\n 0x%016lx: %s\n",
         reinterpret_cast<intptr_t>(instr), format);
  MOZ_CRASH();
}

// Note: With the code below we assume that all runtime calls return a 64 bits
// result. If they don't, the v1 result register contains a bogus value, which
// is fine because it is caller-saved.
typedef int64_t (*Prototype_General0)();
typedef int64_t (*Prototype_General1)(int64_t arg0);
typedef int64_t (*Prototype_General2)(int64_t arg0, int64_t arg1);
typedef int64_t (*Prototype_General3)(int64_t arg0, int64_t arg1, int64_t arg2);
typedef int64_t (*Prototype_General4)(int64_t arg0, int64_t arg1, int64_t arg2,
                                      int64_t arg3);
typedef int64_t (*Prototype_General5)(int64_t arg0, int64_t arg1, int64_t arg2,
                                      int64_t arg3, int64_t arg4);
typedef int64_t (*Prototype_General6)(int64_t arg0, int64_t arg1, int64_t arg2,
                                      int64_t arg3, int64_t arg4, int64_t arg5);
typedef int64_t (*Prototype_General7)(int64_t arg0, int64_t arg1, int64_t arg2,
                                      int64_t arg3, int64_t arg4, int64_t arg5,
                                      int64_t arg6);
typedef int64_t (*Prototype_General8)(int64_t arg0, int64_t arg1, int64_t arg2,
                                      int64_t arg3, int64_t arg4, int64_t arg5,
                                      int64_t arg6, int64_t arg7);
typedef int64_t (*Prototype_GeneralGeneralGeneralInt64)(int64_t arg0,
                                                        int64_t arg1,
                                                        int64_t arg2,
                                                        int64_t arg3);
typedef int64_t (*Prototype_GeneralGeneralInt64Int64)(int64_t arg0,
                                                      int64_t arg1,
                                                      int64_t arg2,
                                                      int64_t arg3);

typedef int64_t (*Prototype_Int_Double)(double arg0);
typedef int64_t (*Prototype_Int_IntDouble)(int64_t arg0, double arg1);
typedef int64_t (*Prototype_Int_DoubleInt)(double arg0, int64_t arg1);
typedef int64_t (*Prototype_Int_DoubleIntInt)(double arg0, int64_t arg1,
                                              int64_t arg2);
typedef int64_t (*Prototype_Int_IntDoubleIntInt)(int64_t arg0, double arg1,
                                                 int64_t arg2, int64_t arg3);

typedef float (*Prototype_Float32_Float32)(float arg0);
typedef int64_t (*Prototype_Int_Float32)(float arg0);
typedef float (*Prototype_Float32_Float32Float32)(float arg0, float arg1);

typedef double (*Prototype_Double_None)();
typedef double (*Prototype_Double_Double)(double arg0);
typedef double (*Prototype_Double_Int)(int64_t arg0);
typedef double (*Prototype_Double_DoubleInt)(double arg0, int64_t arg1);
typedef double (*Prototype_Double_IntDouble)(int64_t arg0, double arg1);
typedef double (*Prototype_Double_DoubleDouble)(double arg0, double arg1);
typedef double (*Prototype_Double_DoubleDoubleDouble)(double arg0, double arg1,
                                                      double arg2);
typedef double (*Prototype_Double_DoubleDoubleDoubleDouble)(double arg0,
                                                            double arg1,
                                                            double arg2,
                                                            double arg3);

typedef int32_t (*Prototype_Int32_General)(int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32)(int64_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32)(int64_t, int32_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32Int32Int32)(int64_t, int32_t,
                                                               int32_t, int32_t,
                                                               int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32Int32Int32Int32)(
    int64_t, int32_t, int32_t, int32_t, int32_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32Int32Int32General)(
    int64_t, int32_t, int32_t, int32_t, int32_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32Int32General)(
    int64_t, int32_t, int32_t, int32_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32Int64)(int64_t, int32_t,
                                                          int32_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int32General)(int64_t, int32_t,
                                                            int32_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32Int64Int64)(int64_t, int32_t,
                                                          int64_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt32GeneralInt32)(int64_t, int32_t,
                                                            int64_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt32GeneralInt32Int32)(
    int64_t, int32_t, int64_t, int32_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralGeneral)(int64_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralGeneralGeneral)(int64_t, int64_t,
                                                         int64_t);
typedef int32_t (*Prototype_Int32_GeneralGeneralInt32Int32)(int64_t, int64_t,
                                                            int32_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int32Int32Int32)(int64_t, int64_t,
                                                               int32_t, int32_t,
                                                               int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int32)(int64_t, int64_t, int32_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int32Int64)(int64_t, int64_t,
                                                          int32_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int32Int64General)(
    int64_t, int64_t, int32_t, int64_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int64Int64)(int64_t, int64_t,
                                                          int64_t, int64_t);
typedef int32_t (*Prototype_Int32_GeneralInt64Int64Int64General)(
    int64_t, int64_t, int64_t, int64_t, int64_t);
typedef int64_t (*Prototype_General_GeneralInt32)(int64_t, int32_t);
typedef int64_t (*Prototype_General_GeneralInt32Int32)(int64_t, int32_t,
                                                       int32_t);
typedef int64_t (*Prototype_General_GeneralInt32General)(int64_t, int32_t,
                                                         int64_t);
typedef int64_t (*Prototype_Int64_General)(int64_t);
typedef int64_t (*Prototype_Int64_GeneralInt64)(int64_t, int64_t);

// Generated by Assembler::break_()/stop(), ebreak code is passed as immediate
// field of a subsequent LUI instruction; otherwise returns -1
static inline int32_t get_ebreak_code(SimInstruction* instr) {
  MOZ_ASSERT(instr->instructionBits() == kBreakInstr);
  uint8_t* cur = reinterpret_cast<uint8_t*>(instr);
  SimInstruction* next_instr = reinterpret_cast<SimInstruction*>(cur + kInstrSize);
  if (next_instr->BaseOpcodeFieldRaw() == LUI)
    return (next_instr->Imm20UValue());
  else
    return -1;
}

// Software interrupt instructions are used by the simulator to call into C++.
void Simulator::softwareInterrupt(SimInstruction* instr) {
  uint32_t code = (instr->instructionBits() == kBreakInstr) ? get_ebreak_code(instr) : -1;

  // We first check if we met a call_rt_redirected.
  if (instr->instructionBits() == kCallRedirInstr) {
#if !defined(USES_N64_ABI)
    MOZ_CRASH("Only N64 ABI supported.");
#else
    Redirection* redirection = Redirection::FromSwiInstruction(instr);
    uintptr_t nativeFn =
        reinterpret_cast<uintptr_t>(redirection->nativeFunction());

    int64_t arg0 = getRegister(a0);
    int64_t arg1 = getRegister(a1);
    int64_t arg2 = getRegister(a2);
    int64_t arg3 = getRegister(a3);
    int64_t arg4 = getRegister(a4);
    int64_t arg5 = getRegister(a5);

    // This is dodgy but it works because the C entry stubs are never moved.
    // See comment in codegen-arm.cc and bug 1242173.
    int64_t saved_ra = getRegister(ra);

    intptr_t external =
        reinterpret_cast<intptr_t>(redirection->nativeFunction());

    bool stack_aligned = (getRegister(sp) & (ABIStackAlignment - 1)) == 0;
    if (!stack_aligned) {
      fprintf(stderr, "Runtime call with unaligned stack!\n");
      MOZ_CRASH();
    }

    if (single_stepping_) {
      single_step_callback_(single_step_callback_arg_, this, nullptr);
    }

    switch (redirection->type()) {
      case Args_General0: {
        Prototype_General0 target =
            reinterpret_cast<Prototype_General0>(external);
        int64_t result = target();
        setCallResult(result);
        break;
      }
      case Args_General1: {
        Prototype_General1 target =
            reinterpret_cast<Prototype_General1>(external);
        int64_t result = target(arg0);
        setCallResult(result);
        break;
      }
      case Args_General2: {
        Prototype_General2 target =
            reinterpret_cast<Prototype_General2>(external);
        int64_t result = target(arg0, arg1);
        setCallResult(result);
        break;
      }
      case Args_General3: {
        Prototype_General3 target =
            reinterpret_cast<Prototype_General3>(external);
        int64_t result = target(arg0, arg1, arg2);
        if (external == intptr_t(&js::wasm::Instance::wake_m32)) {
          result = int32_t(result);
        }
        setCallResult(result);
        break;
      }
      case Args_General4: {
        Prototype_General4 target =
            reinterpret_cast<Prototype_General4>(external);
        int64_t result = target(arg0, arg1, arg2, arg3);
        setCallResult(result);
        break;
      }
      case Args_General5: {
        Prototype_General5 target =
            reinterpret_cast<Prototype_General5>(external);
        int64_t result = target(arg0, arg1, arg2, arg3, arg4);
        setCallResult(result);
        break;
      }
      case Args_General6: {
        Prototype_General6 target =
            reinterpret_cast<Prototype_General6>(external);
        int64_t result = target(arg0, arg1, arg2, arg3, arg4, arg5);
        setCallResult(result);
        break;
      }
      case Args_General7: {
        Prototype_General7 target =
            reinterpret_cast<Prototype_General7>(external);
        int64_t arg6 = getRegister(a6);
        int64_t result = target(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
        setCallResult(result);
        break;
      }
      case Args_General8: {
        Prototype_General8 target =
            reinterpret_cast<Prototype_General8>(external);
        int64_t arg6 = getRegister(a6);
        int64_t arg7 = getRegister(a7);
        int64_t result = target(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
        setCallResult(result);
        break;
      }
      case Args_Double_None: {
        Prototype_Double_None target =
            reinterpret_cast<Prototype_Double_None>(external);
        double dresult = target();
        setCallResultDouble(dresult);
        break;
      }
      case Args_Int_Double: {
        double dval0 = getFpuRegisterDouble(12);
        Prototype_Int_Double target =
            reinterpret_cast<Prototype_Int_Double>(external);
        int64_t result = target(dval0);
        if (external == intptr_t((int32_t(*)(double))JS::ToInt32)) {
          result = int32_t(result);
        }
        setRegister(v0, result);
        break;
      }
      case Args_Int_GeneralGeneralGeneralInt64: {
        Prototype_GeneralGeneralGeneralInt64 target =
            reinterpret_cast<Prototype_GeneralGeneralGeneralInt64>(external);
        int64_t result = target(arg0, arg1, arg2, arg3);
        if (external == intptr_t(&js::wasm::Instance::wait_i32_m32)) {
          result = int32_t(result);
        }
        setRegister(v0, result);
        break;
      }
      case Args_Int_GeneralGeneralInt64Int64: {
        Prototype_GeneralGeneralInt64Int64 target =
            reinterpret_cast<Prototype_GeneralGeneralInt64Int64>(external);
        int64_t result = target(arg0, arg1, arg2, arg3);
        if (external == intptr_t(&js::wasm::Instance::wait_i64_m32)) {
          result = int32_t(result);
        }
        setRegister(v0, result);
        break;
      }
      case Args_Int_DoubleInt: {
        double dval = getFpuRegisterDouble(12);
        Prototype_Int_DoubleInt target =
            reinterpret_cast<Prototype_Int_DoubleInt>(external);
        int64_t result = target(dval, arg1);
        setRegister(v0, result);
        break;
      }
      case Args_Int_DoubleIntInt: {
        double dval = getFpuRegisterDouble(12);
        Prototype_Int_DoubleIntInt target =
            reinterpret_cast<Prototype_Int_DoubleIntInt>(external);
        int64_t result = target(dval, arg1, arg2);
        setRegister(v0, result);
        break;
      }
      case Args_Int_IntDoubleIntInt: {
        double dval = getFpuRegisterDouble(13);
        Prototype_Int_IntDoubleIntInt target =
            reinterpret_cast<Prototype_Int_IntDoubleIntInt>(external);
        int64_t result = target(arg0, dval, arg2, arg3);
        setRegister(v0, result);
        break;
      }
      case Args_Double_Double: {
        double dval0 = getFpuRegisterDouble(12);
        Prototype_Double_Double target =
            reinterpret_cast<Prototype_Double_Double>(external);
        double dresult = target(dval0);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Float32_Float32: {
        float fval0;
        fval0 = getFpuRegisterFloat(12);
        Prototype_Float32_Float32 target =
            reinterpret_cast<Prototype_Float32_Float32>(external);
        float fresult = target(fval0);
        setCallResultFloat(fresult);
        break;
      }
      case Args_Int_Float32: {
        float fval0;
        fval0 = getFpuRegisterFloat(12);
        Prototype_Int_Float32 target =
            reinterpret_cast<Prototype_Int_Float32>(external);
        int64_t result = target(fval0);
        setRegister(v0, result);
        break;
      }
      case Args_Float32_Float32Float32: {
        float fval0;
        float fval1;
        fval0 = getFpuRegisterFloat(12);
        fval1 = getFpuRegisterFloat(13);
        Prototype_Float32_Float32Float32 target =
            reinterpret_cast<Prototype_Float32_Float32Float32>(external);
        float fresult = target(fval0, fval1);
        setCallResultFloat(fresult);
        break;
      }
      case Args_Double_Int: {
        Prototype_Double_Int target =
            reinterpret_cast<Prototype_Double_Int>(external);
        double dresult = target(arg0);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Double_DoubleInt: {
        double dval0 = getFpuRegisterDouble(12);
        Prototype_Double_DoubleInt target =
            reinterpret_cast<Prototype_Double_DoubleInt>(external);
        double dresult = target(dval0, arg1);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Double_DoubleDouble: {
        double dval0 = getFpuRegisterDouble(12);
        double dval1 = getFpuRegisterDouble(13);
        Prototype_Double_DoubleDouble target =
            reinterpret_cast<Prototype_Double_DoubleDouble>(external);
        double dresult = target(dval0, dval1);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Double_IntDouble: {
        double dval1 = getFpuRegisterDouble(13);
        Prototype_Double_IntDouble target =
            reinterpret_cast<Prototype_Double_IntDouble>(external);
        double dresult = target(arg0, dval1);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Int_IntDouble: {
        double dval1 = getFpuRegisterDouble(13);
        Prototype_Int_IntDouble target =
            reinterpret_cast<Prototype_Int_IntDouble>(external);
        int64_t result = target(arg0, dval1);
        setRegister(v0, result);
        break;
      }
      case Args_Double_DoubleDoubleDouble: {
        double dval0 = getFpuRegisterDouble(12);
        double dval1 = getFpuRegisterDouble(13);
        double dval2 = getFpuRegisterDouble(14);
        Prototype_Double_DoubleDoubleDouble target =
            reinterpret_cast<Prototype_Double_DoubleDoubleDouble>(external);
        double dresult = target(dval0, dval1, dval2);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Double_DoubleDoubleDoubleDouble: {
        double dval0 = getFpuRegisterDouble(12);
        double dval1 = getFpuRegisterDouble(13);
        double dval2 = getFpuRegisterDouble(14);
        double dval3 = getFpuRegisterDouble(15);
        Prototype_Double_DoubleDoubleDoubleDouble target =
            reinterpret_cast<Prototype_Double_DoubleDoubleDoubleDouble>(
                external);
        double dresult = target(dval0, dval1, dval2, dval3);
        setCallResultDouble(dresult);
        break;
      }
      case Args_Int32_General: {
        int32_t ret = reinterpret_cast<Prototype_Int32_General>(nativeFn)(arg0);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt32>(nativeFn)(
            arg0, I32(arg1));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt32Int32>(
            nativeFn)(arg0, I32(arg1), I32(arg2));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32Int32Int32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32Int32Int32Int32>(
                nativeFn)(arg0, I32(arg1), I32(arg2), I32(arg3), I32(arg4));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32Int32Int32Int32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32Int32Int32Int32Int32>(
                nativeFn)(arg0, I32(arg1), I32(arg2), I32(arg3), I32(arg4),
                          I32(arg5));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32Int32Int32General: {
        int32_t ret = reinterpret_cast<
            Prototype_Int32_GeneralInt32Int32Int32Int32General>(nativeFn)(
            arg0, I32(arg1), I32(arg2), I32(arg3), I32(arg4), arg5);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32Int32General: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32Int32Int32General>(
                nativeFn)(arg0, I32(arg1), I32(arg2), I32(arg3), arg4);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32Int64: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt32Int32Int64>(
            nativeFn)(arg0, I32(arg1), I32(arg2), arg3);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int32General: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32Int32General>(
                nativeFn)(arg0, I32(arg1), I32(arg2), arg3);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32Int64Int64: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt32Int64Int64>(
            nativeFn)(arg0, I32(arg1), arg2, arg3);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32GeneralInt32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32GeneralInt32>(
                nativeFn)(arg0, I32(arg1), arg2, I32(arg3));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralInt32GeneralInt32Int32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt32GeneralInt32Int32>(
                nativeFn)(arg0, I32(arg1), arg2, I32(arg3), I32(arg4));
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralGeneral: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralGeneral>(
            nativeFn)(arg0, arg1);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralGeneralGeneral: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralGeneralGeneral>(
            nativeFn)(arg0, arg1, arg2);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_Int32_GeneralGeneralInt32Int32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralGeneralInt32Int32>(
                nativeFn)(arg0, arg1, I32(arg2), I32(arg3));
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int32Int32Int32: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt64Int32Int32Int32>(
                nativeFn)(arg0, arg1, I32(arg2), I32(arg3), I32(arg4));
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int32: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt64Int32>(
            nativeFn)(arg0, arg1, I32(arg2));
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int32Int64: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt64Int32Int64>(
            nativeFn)(arg0, arg1, I32(arg2), arg3);
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int32Int64General: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt64Int32Int64General>(
                nativeFn)(arg0, arg1, I32(arg2), arg3, arg4);
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int64Int64: {
        int32_t ret = reinterpret_cast<Prototype_Int32_GeneralInt64Int64Int64>(
            nativeFn)(arg0, arg1, arg2, arg3);
        setRegister(v0, I64(ret));
        break;
      }
      case js::jit::Args_Int32_GeneralInt64Int64Int64General: {
        int32_t ret =
            reinterpret_cast<Prototype_Int32_GeneralInt64Int64Int64General>(
                nativeFn)(arg0, arg1, arg2, arg3, arg4);
        setRegister(v0, I64(ret));
        break;
      }
      case Args_General_GeneralInt32: {
        int64_t ret = reinterpret_cast<Prototype_General_GeneralInt32>(
            nativeFn)(arg0, I32(arg1));
        setRegister(v0, ret);
        break;
      }
      case Args_General_GeneralInt32Int32: {
        int64_t ret = reinterpret_cast<Prototype_General_GeneralInt32Int32>(
            nativeFn)(arg0, I32(arg1), I32(arg2));
        setRegister(v0, ret);
        break;
      }
      case Args_General_GeneralInt32General: {
        int64_t ret = reinterpret_cast<Prototype_General_GeneralInt32General>(
            nativeFn)(arg0, I32(arg1), arg2);
        setRegister(v0, ret);
        break;
      }
      case js::jit::Args_Int64_General: {
        int64_t ret = reinterpret_cast<Prototype_Int64_General>(nativeFn)(arg0);
        setRegister(v0, ret);
        break;
      }
      case js::jit::Args_Int64_GeneralInt64: {
        int64_t ret = reinterpret_cast<Prototype_Int64_GeneralInt64>(nativeFn)(
            arg0, arg1);
        setRegister(v0, ret);
        break;
      }
      default:
        MOZ_CRASH("Unknown function type.");
    }

    if (single_stepping_) {
      single_step_callback_(single_step_callback_arg_, this, nullptr);
    }

    setRegister(ra, saved_ra);
    set_pc(getRegister(ra));
#endif
  } else if (instr->instructionBits() == kBreakInstr && code <= kMaxStopCode) {
    if (isWatchpoint(code)) {
      printWatchpoint(code);
    } else {
      increaseStopCounter(code);
      handleStop(code, instr);
    }
  } else {
    switch (instr->instructionBits() & kBaseOpcodeMask) {
       UNSUPPORTED();
    };
    // All remaining break_ codes, and all traps are handled here.
    RiscvDebugger dbg(this);
    dbg.Debug();
  }
}

// Stop helper functions.
bool Simulator::isWatchpoint(uint32_t code) {
  return (code <= kMaxWatchpointCode);
}

void Simulator::printWatchpoint(uint32_t code) {
  RiscvDebugger dbg(this);
  ++break_count_;
  printf("\n---- break %d marker: %20" PRIi64 "  (instr count: %20" PRIi64
         ") ----\n",
         code, break_count_, icount_);
  dbg.printAllRegs();  // Print registers and continue running.
}

void Simulator::handleStop(uint32_t code, SimInstruction* instr) {
  // Stop if it is enabled, otherwise go on jumping over the stop
  // and the message address.
  if (isEnabledStop(code)) {
    RiscvDebugger dbg(this);
    dbg.Debug();
  } else {
    set_pc(get_pc() + 2 * SimInstruction::kInstrSize);
  }
}

bool Simulator::isStopInstruction(SimInstruction* instr) {
  if (instr->instructionBits() != kBreakInstr) return false;
  int32_t code = get_ebreak_code(instr);
  return code != -1 && static_cast<uint32_t>(code) > kMaxWatchpointCode &&
         static_cast<uint32_t>(code) <= kMaxStopCode;
}

bool Simulator::isEnabledStop(uint32_t code) {
  MOZ_ASSERT(code <= kMaxStopCode);
  MOZ_ASSERT(code > kMaxWatchpointCode);
  return !(watchedStops_[code].count_ & kStopDisabledBit);
}

void Simulator::enableStop(uint32_t code) {
  if (!isEnabledStop(code)) {
    watchedStops_[code].count_ &= ~kStopDisabledBit;
  }
}

void Simulator::disableStop(uint32_t code) {
  if (isEnabledStop(code)) {
    watchedStops_[code].count_ |= kStopDisabledBit;
  }
}

void Simulator::increaseStopCounter(uint32_t code) {
  MOZ_ASSERT(code <= kMaxStopCode);
  if ((watchedStops_[code].count_ & ~(1 << 31)) == 0x7fffffff) {
    printf(
        "Stop counter for code %i has overflowed.\n"
        "Enabling this code and reseting the counter to 0.\n",
        code);
    watchedStops_[code].count_ = 0;
    enableStop(code);
  } else {
    watchedStops_[code].count_++;
  }
}

// Print a stop status.
void Simulator::printStopInfo(uint32_t code) {
  if (code <= kMaxWatchpointCode) {
    printf("That is a watchpoint, not a stop.\n");
    return;
  } else if (code > kMaxStopCode) {
    printf("Code too large, only %u stops can be used\n", kMaxStopCode + 1);
    return;
  }
  const char* state = isEnabledStop(code) ? "Enabled" : "Disabled";
  int32_t count = watchedStops_[code].count_ & ~kStopDisabledBit;
  // Don't print the state of unused breakpoints.
  if (count != 0) {
    if (watchedStops_[code].desc_) {
      printf("stop %i - 0x%x: \t%s, \tcounter = %i, \t%s\n", code, code, state,
             count, watchedStops_[code].desc_);
    } else {
      printf("stop %i - 0x%x: \t%s, \tcounter = %i\n", code, code, state,
             count);
    }
  }
}

void Simulator::signalExceptions() {
  for (int i = 1; i < kNumExceptions; i++) {
    if (exceptions[i] != 0) {
      MOZ_CRASH("Error: Exception raised.");
    }
  }
}


// Executes the current instruction.
void Simulator::instructionDecode(SimInstruction* instr) {
  if (!SimulatorProcess::ICacheCheckingDisableCount) {
    AutoLockSimulatorCache als;
    SimulatorProcess::checkICacheLocked(instr);
  }
  pc_modified_ = false;
  UNSUPPORTED();
  //   switch (instr->instructionType()) {
  //     case SimInstruction::kRegisterType:
  //       decodeTypeRegister(instr);
  //       break;
  //     case SimInstruction::kImmediateType:
  //       decodeTypeImmediate(instr);
  //       break;
  //     case SimInstruction::kJumpType:
  //       decodeTypeJump(instr);
  //       break;
  //     default:
  //       UNSUPPORTED();
  //   }
  if (!pc_modified_) {
    setRegister(pc,
                reinterpret_cast<int64_t>(instr) + SimInstruction::kInstrSize);
  }
}


void Simulator::enable_single_stepping(SingleStepCallback cb, void* arg) {
  single_stepping_ = true;
  single_step_callback_ = cb;
  single_step_callback_arg_ = arg;
  single_step_callback_(single_step_callback_arg_, this, (void*)get_pc());
}

void Simulator::disable_single_stepping() {
  if (!single_stepping_) {
    return;
  }
  single_step_callback_(single_step_callback_arg_, this, (void*)get_pc());
  single_stepping_ = false;
  single_step_callback_ = nullptr;
  single_step_callback_arg_ = nullptr;
}

template <bool enableStopSimAt>
void Simulator::execute() {
  if (single_stepping_) {
    single_step_callback_(single_step_callback_arg_, this, nullptr);
  }

  // Get the PC to simulate. Cannot use the accessor here as we need the
  // raw PC value and not the one used as input to arithmetic instructions.
  int64_t program_counter = get_pc();

  while (program_counter != end_sim_pc) {
    if (enableStopSimAt && (icount_ == Simulator::StopSimAt)) {
      RiscvDebugger dbg(this);
      dbg.Debug();
    } else {
      if (single_stepping_) {
        single_step_callback_(single_step_callback_arg_, this,
                              (void*)program_counter);
      }
      SimInstruction* instr =
          reinterpret_cast<SimInstruction*>(program_counter);
      instructionDecode(instr);
      icount_++;
    }
    program_counter = get_pc();
  }

  if (single_stepping_) {
    single_step_callback_(single_step_callback_arg_, this, nullptr);
  }
}

void Simulator::callInternal(uint8_t* entry) {
  // Prepare to execute the code at entry.
  setRegister(pc, reinterpret_cast<int64_t>(entry));
  // Put down marker for end of simulation. The simulator will stop simulation
  // when the PC reaches this value. By saving the "end simulation" value into
  // the LR the simulation stops when returning to this call point.
  setRegister(ra, end_sim_pc);
  // Remember the values of callee-saved registers.
  intptr_t s0_val = getRegister(Simulator::Register::fp);
  intptr_t s1_val = getRegister(Simulator::Register::s1);
  intptr_t s2_val = getRegister(Simulator::Register::s2);
  intptr_t s3_val = getRegister(Simulator::Register::s3);
  intptr_t s4_val = getRegister(Simulator::Register::s4);
  intptr_t s5_val = getRegister(Simulator::Register::s5);
  intptr_t s6_val = getRegister(Simulator::Register::s6);
  intptr_t s7_val = getRegister(Simulator::Register::s7);
  intptr_t s8_val = getRegister(Simulator::Register::s8);
  intptr_t s9_val = getRegister(Simulator::Register::s9);
  intptr_t s10_val = getRegister(Simulator::Register::s10);
  intptr_t s11_val = getRegister(Simulator::Register::s11);
  intptr_t gp_val = getRegister(Simulator::Register::gp);
  intptr_t sp_val = getRegister(Simulator::Register::sp);

  // Set up the callee-saved registers with a known value. To be able to check
  // that they are preserved properly across JS execution. If this value is
  // small int, it should be SMI.
  intptr_t callee_saved_value = icount_;
  setRegister(Simulator::Register::fp, callee_saved_value);
  setRegister(Simulator::Register::s1, callee_saved_value);
  setRegister(Simulator::Register::s2, callee_saved_value);
  setRegister(Simulator::Register::s3, callee_saved_value);
  setRegister(Simulator::Register::s4, callee_saved_value);
  setRegister(Simulator::Register::s5, callee_saved_value);
  setRegister(Simulator::Register::s6, callee_saved_value);
  setRegister(Simulator::Register::s7, callee_saved_value);
  setRegister(Simulator::Register::s8, callee_saved_value);
  setRegister(Simulator::Register::s9, callee_saved_value);
  setRegister(Simulator::Register::s10, callee_saved_value);
  setRegister(Simulator::Register::s11, callee_saved_value);
  setRegister(Simulator::Register::gp, callee_saved_value);

  // Start the simulation.
  if (Simulator::StopSimAt != -1) {
    execute<true>();
  } else {
    execute<false>();
  }

  // Check that the callee-saved registers have been preserved.
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::fp));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s1));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s2));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s3));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s4));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s5));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s6));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s7));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s8));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s9));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s10));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::s11));
  MOZ_ASSERT(callee_saved_value, getRegister(Simulator::Register::gp));

  // Restore callee-saved registers with the original value.
  setRegister(Simulator::Register::fp, s0_val);
  setRegister(Simulator::Register::s1, s1_val);
  setRegister(Simulator::Register::s2, s2_val);
  setRegister(Simulator::Register::s3, s3_val);
  setRegister(Simulator::Register::s4, s4_val);
  setRegister(Simulator::Register::s5, s5_val);
  setRegister(Simulator::Register::s6, s6_val);
  setRegister(Simulator::Register::s7, s7_val);
  setRegister(Simulator::Register::s8, s8_val);
  setRegister(Simulator::Register::s9, s9_val);
  setRegister(Simulator::Register::s10, s10_val);
  setRegister(Simulator::Register::s11, s11_val);
  setRegister(Simulator::Register::gp, gp_val);
  setRegister(Simulator::Register::sp, sp_val);
}

int64_t Simulator::call(uint8_t* entry, int argument_count, ...) {
  va_list parameters;
  va_start(parameters, argument_count);

  int64_t original_stack = getRegister(sp);
  // Compute position of stack on entry to generated code.
  int64_t entry_stack = original_stack;
  if (argument_count > kCArgSlotCount) {
    entry_stack = entry_stack - argument_count * sizeof(int64_t);
  } else {
    entry_stack = entry_stack - kCArgsSlotsSize;
  }

  entry_stack &= ~U64(ABIStackAlignment - 1);

  intptr_t* stack_argument = reinterpret_cast<intptr_t*>(entry_stack);

  // Setup the arguments.
  for (int i = 0; i < argument_count; i++) {
    js::jit::Register argReg;
    if (GetIntArgReg(i, argReg)) {
      setRegister(argReg.code(), va_arg(parameters, int64_t));
    } else {
      stack_argument[i] = va_arg(parameters, int64_t);
    }
  }

  va_end(parameters);
  setRegister(sp, entry_stack);

  callInternal(entry);

  // Pop stack passed arguments.
  MOZ_ASSERT(entry_stack == getRegister(sp));
  setRegister(sp, original_stack);

  int64_t result = getRegister(a0);
  return result;
}

uintptr_t Simulator::pushAddress(uintptr_t address) {
  int new_sp = getRegister(sp) - sizeof(uintptr_t);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(new_sp);
  *stack_slot = address;
  setRegister(sp, new_sp);
  return new_sp;
}

uintptr_t Simulator::popAddress() {
  int current_sp = getRegister(sp);
  uintptr_t* stack_slot = reinterpret_cast<uintptr_t*>(current_sp);
  uintptr_t address = *stack_slot;
  setRegister(sp, current_sp + sizeof(uintptr_t));
  return address;
}

}  // namespace jit
}  // namespace js

js::jit::Simulator* JSContext::simulator() const { return simulator_; }
#endif // JS_SIMULATOR_RISCV64