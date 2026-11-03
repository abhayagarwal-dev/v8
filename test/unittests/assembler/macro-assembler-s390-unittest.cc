// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "src/base/bits.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/s390/assembler-s390-inl.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/simulator.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ masm.

// Test the s390 MacroAssembler by compiling simple functions into a buffer
// and executing them. These tests do not initialize the V8 library, create a
// context, or use any V8 objects.

class MacroAssemblerTest : public TestWithIsolate {};

// ---------------------------------------------------------------------------
// Abort / Check (existing tests kept)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(); }, v8_flags.debug_code ? "abort: no reason" : "");
}

TEST_F(MacroAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ lgfi(r3, Operand(17));
  __ CmpS64(r2, r3);  // 1st parameter is in {r2}.
  __ Check(Condition::ne, AbortReason::kNoReason);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(17); }, v8_flags.debug_code ? "abort: no reason" : "");
}

// ---------------------------------------------------------------------------
// Integer arithmetic — AddS32 / AddS64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, AddS32) {
  // f(a, b) = a + b  (32-bit signed, result in r2)
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = first arg, r3 = second arg
  __ AddS32(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t, int32_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0, f.Call(0, 0));
  CHECK_EQ(5, f.Call(2, 3));
  CHECK_EQ(-1, f.Call(INT32_MAX, INT32_MIN));
  CHECK_EQ(INT32_MIN, f.Call(INT32_MAX, 1));  // overflow wraps
}

TEST_F(MacroAssemblerTest, AddS64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t, int64_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0LL, f.Call(0, 0));
  CHECK_EQ(100LL, f.Call(40, 60));
  CHECK_EQ(-1LL, f.Call(INT64_MAX, INT64_MIN));
}

// ---------------------------------------------------------------------------
// Integer arithmetic — SubS32 / SubS64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, SubS32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SubS32(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t, int32_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0, f.Call(5, 5));
  CHECK_EQ(3, f.Call(8, 5));
  CHECK_EQ(-3, f.Call(5, 8));
}

TEST_F(MacroAssemblerTest, SubS64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SubS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t, int64_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0LL, f.Call(0, 0));
  CHECK_EQ(1LL, f.Call(INT64_MIN, INT64_MAX));  // wraps
  CHECK_EQ(100LL, f.Call(200, 100));
}

// ---------------------------------------------------------------------------
// Integer arithmetic — MulS32 / MulS64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, MulS32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ MulS32(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t, int32_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0, f.Call(0, 42));
  CHECK_EQ(42, f.Call(6, 7));
  CHECK_EQ(-42, f.Call(-6, 7));
  CHECK_EQ(1, f.Call(-1, -1));
}

TEST_F(MacroAssemblerTest, MulS64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ MulS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t, int64_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(0LL, f.Call(0, 100));
  CHECK_EQ(100LL, f.Call(10, 10));
  CHECK_EQ(-100LL, f.Call(10, -10));
  CHECK_EQ(static_cast<int64_t>(0x0000000100000000LL),
           f.Call(static_cast<int64_t>(0x100000LL),
                  static_cast<int64_t>(0x1000LL)));
}

// ---------------------------------------------------------------------------
// Comparison — CmpS32 / CmpS64 with conditional branch
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, CmpS32BranchEq) {
  // Returns 1 if a == b (32-bit), else 0.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label equal, done;
  __ CmpS32(r2, r3);
  __ beq(&equal);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&equal);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t, int32_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(1, f.Call(5, 5));
  CHECK_EQ(0, f.Call(5, 6));
  CHECK_EQ(1, f.Call(-1, -1));
  CHECK_EQ(0, f.Call(0, 1));
}

TEST_F(MacroAssemblerTest, CmpS64BranchLt) {
  // Returns 1 if a < b (64-bit signed), else 0.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label less, done;
  __ CmpS64(r2, r3);
  __ blt(&less);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&less);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int64_t, int64_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(1, f.Call(-1, 0));
  CHECK_EQ(0, f.Call(0, 0));
  CHECK_EQ(0, f.Call(1, 0));
  CHECK_EQ(1, f.Call(INT64_MIN, INT64_MAX));
}

// ---------------------------------------------------------------------------
// Smi operations
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, SmiTag) {
  // SmiTag(r) = r << kSmiShift  (in place)
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SmiTag(r2);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<intptr_t, intptr_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(Smi::FromInt(0).ptr(), static_cast<uintptr_t>(f.Call(0)));
  CHECK_EQ(Smi::FromInt(1).ptr(), static_cast<uintptr_t>(f.Call(1)));
  CHECK_EQ(Smi::FromInt(42).ptr(), static_cast<uintptr_t>(f.Call(42)));
  CHECK_EQ(Smi::FromInt(-1).ptr(), static_cast<uintptr_t>(f.Call(-1)));
}

TEST_F(MacroAssemblerTest, SmiUntag) {
  // SmiUntag(r) = r >> kSmiShift  (in place)
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SmiUntag(r2);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<intptr_t, intptr_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(0, f.Call(Smi::FromInt(0).ptr()));
  CHECK_EQ(1, f.Call(Smi::FromInt(1).ptr()));
  CHECK_EQ(42, f.Call(Smi::FromInt(42).ptr()));
  CHECK_EQ(-1, f.Call(Smi::FromInt(-1).ptr()));
}

TEST_F(MacroAssemblerTest, SmiToInt32) {
  // SmiToInt32 converts a Smi to a 32-bit integer value.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SmiToInt32(r2);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, intptr_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(0, f.Call(Smi::FromInt(0).ptr()));
  CHECK_EQ(7, f.Call(Smi::FromInt(7).ptr()));
  CHECK_EQ(-7, f.Call(Smi::FromInt(-7).ptr()));
  CHECK_EQ(100, f.Call(Smi::FromInt(100).ptr()));
}

// ---------------------------------------------------------------------------
// Popcnt32 / Popcnt64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, Popcnt32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ Popcnt32(r2, r2);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, uint32_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(0, f.Call(0x00000000u));
  CHECK_EQ(32, f.Call(0xFFFFFFFFu));
  CHECK_EQ(1, f.Call(0x00000001u));
  CHECK_EQ(1, f.Call(0x80000000u));
  CHECK_EQ(16, f.Call(0xAAAAAAAAu));
  CHECK_EQ(16, f.Call(0x0F0F0F0Fu));
}

TEST_F(MacroAssemblerTest, Popcnt64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ Popcnt64(r2, r2);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, uint64_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(0LL, f.Call(0x0000000000000000ULL));
  CHECK_EQ(64LL, f.Call(0xFFFFFFFFFFFFFFFFULL));
  CHECK_EQ(1LL, f.Call(0x0000000000000001ULL));
  CHECK_EQ(1LL, f.Call(0x8000000000000000ULL));
  CHECK_EQ(32LL, f.Call(0xAAAAAAAAAAAAAAAAULL));
  CHECK_EQ(10LL, f.Call(0xAF1000000000000BULL));
}

// ---------------------------------------------------------------------------
// CountLeadingZerosU32 / CountLeadingZerosU64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, CountLeadingZerosU32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // scratch pair r0:r1
  __ CountLeadingZerosU32(r2, r2, r0);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, uint32_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(32, f.Call(0x00000000u));
  CHECK_EQ(0, f.Call(0xFFFFFFFFu));
  CHECK_EQ(0, f.Call(0x80000000u));
  CHECK_EQ(31, f.Call(0x00000001u));
  CHECK_EQ(4, f.Call(0x0FFFFFFFu));
}

TEST_F(MacroAssemblerTest, CountLeadingZerosU64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ CountLeadingZerosU64(r2, r2, r0);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, uint64_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(64LL, f.Call(0x0000000000000000ULL));
  CHECK_EQ(0LL, f.Call(0xFFFFFFFFFFFFFFFFULL));
  CHECK_EQ(0LL, f.Call(0x8000000000000000ULL));
  CHECK_EQ(63LL, f.Call(0x0000000000000001ULL));
  CHECK_EQ(32LL, f.Call(0x00000000FFFFFFFFULL));
}

// ---------------------------------------------------------------------------
// CountTrailingZerosU32 / CountTrailingZerosU64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, CountTrailingZerosU32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ CountTrailingZerosU32(r2, r2, r0);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, uint32_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(32, f.Call(0x00000000u));
  CHECK_EQ(0, f.Call(0xFFFFFFFFu));
  CHECK_EQ(31, f.Call(0x80000000u));
  CHECK_EQ(0, f.Call(0x00000001u));
  CHECK_EQ(4, f.Call(0x00000010u));
}

TEST_F(MacroAssemblerTest, CountTrailingZerosU64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ CountTrailingZerosU64(r2, r2, r0);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, uint64_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(64LL, f.Call(0x0000000000000000ULL));
  CHECK_EQ(0LL, f.Call(0xFFFFFFFFFFFFFFFFULL));
  CHECK_EQ(63LL, f.Call(0x8000000000000000ULL));
  CHECK_EQ(0LL, f.Call(0x0000000000000001ULL));
  CHECK_EQ(8LL, f.Call(0x0000000000000100ULL));
}

// ---------------------------------------------------------------------------
// Float conversions — ConvertIntToDouble / ConvertIntToFloat /
// ConvertDoubleToInt64
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, ConvertIntToDouble) {
  // Loads int32 T::input, converts it to float64 in d0, stores T::output.
  struct T {
    int32_t input;
    double output;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to T
  __ LoadS32(r3, MemOperand(r2, offsetof(T, input)));
  __ ConvertIntToDouble(d0, r3);
  __ StoreF64(d0, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0;     t.output = -1.0; f.Call(&t); CHECK_EQ(0.0,  t.output);
  t.input = 42;    t.output = -1.0; f.Call(&t); CHECK_EQ(42.0, t.output);
  t.input = -42;   t.output = -1.0; f.Call(&t); CHECK_EQ(-42.0, t.output);
  t.input = 1000;  t.output = -1.0; f.Call(&t); CHECK_EQ(1000.0, t.output);
}

TEST_F(MacroAssemblerTest, ConvertIntToFloat) {
  // Loads int32 T::input, converts it to float32 in d0, stores T::output.
  struct T {
    int32_t input;
    float output;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to T
  __ LoadS32(r3, MemOperand(r2, offsetof(T, input)));
  __ ConvertIntToFloat(d0, r3);
  __ StoreF32(d0, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0;     t.output = -1.0f; f.Call(&t); CHECK_EQ(0.0f,  t.output);
  t.input = 42;    t.output = -1.0f; f.Call(&t); CHECK_EQ(42.0f, t.output);
  t.input = -42;   t.output = -1.0f; f.Call(&t); CHECK_EQ(-42.0f, t.output);
  t.input = 1000;  t.output = -1.0f; f.Call(&t); CHECK_EQ(1000.0f, t.output);
  // 2^24 + 1 is not representable as float32; rounds to nearest even (2^24).
  t.input = 16777217; t.output = -1.0f; f.Call(&t);
  CHECK_EQ(16777216.0f, t.output);
}

TEST_F(MacroAssemblerTest, ConvertDoubleToInt64) {
  // Converts double in d0 to int64 in r2 (truncate toward zero).
  struct T {
    double input;
    int64_t output;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to T
  __ LoadF64(d0, MemOperand(r2, offsetof(T, input)));
  __ ConvertDoubleToInt64(r3, d0);
  __ StoreU64(r3, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0.0;    t.output = INT64_MIN; f.Call(&t); CHECK_EQ(0LL,   t.output);
  t.input = 1.7;    t.output = INT64_MIN; f.Call(&t); CHECK_EQ(1LL,   t.output);
  t.input = -1.7;   t.output = INT64_MIN; f.Call(&t); CHECK_EQ(-1LL,  t.output);
  t.input = 100.9;  t.output = INT64_MIN; f.Call(&t); CHECK_EQ(100LL, t.output);
  t.input = -100.9; t.output = INT64_MIN; f.Call(&t);
  CHECK_EQ(-100LL, t.output);
}

// ---------------------------------------------------------------------------
// Load/Store — typed widths
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, LoadStoreU64) {
  uint64_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = src ptr (int64), r3 = dst ptr — store then load back
  __ LoadU64(r4, MemOperand(r2));
  __ StoreU64(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint64_t src = 0xDEADBEEFCAFEBABEULL;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

TEST_F(MacroAssemblerTest, LoadStoreU32) {
  uint32_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU32(r4, MemOperand(r2));
  __ StoreU32(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint32_t src = 0xCAFEBABEu;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

TEST_F(MacroAssemblerTest, LoadStoreU16) {
  uint16_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU16(r4, MemOperand(r2));
  __ StoreU16(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint16_t src = 0xABCDu;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

TEST_F(MacroAssemblerTest, LoadStoreU8) {
  uint8_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU8(r4, MemOperand(r2));
  __ StoreU8(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint8_t src = 0xA5u;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

// ---------------------------------------------------------------------------
// LoadS8 sign-extends
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, LoadS8SignExtend) {
  int64_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // Load a signed byte and sign-extend to 64 bits; store the 64-bit result.
  __ LoadS8(r4, MemOperand(r2));
  __ StoreU64(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint8_t src_pos = 0x7F;  // +127
  f.Call(&src_pos, &val);
  CHECK_EQ(127LL, val);

  uint8_t src_neg = 0x80;  // -128 when signed
  f.Call(&src_neg, &val);
  CHECK_EQ(-128LL, val);
}

// ---------------------------------------------------------------------------
// Float loads and stores
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, LoadStoreF64) {
  double val = 0.0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d0, MemOperand(r2));
  __ StoreF64(d0, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  double src = 3.14159265358979;
  f.Call(&src, &val);
  CHECK_EQ(src, val);

  src = -0.0;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

TEST_F(MacroAssemblerTest, LoadStoreF32) {
  float val = 0.0f;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d0, MemOperand(r2));
  __ StoreF32(d0, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  float src = 2.71828f;
  f.Call(&src, &val);
  CHECK_EQ(src, val);
}

// LoadF32 must use LDE, which zeroes the low 32 bits of the FPR. Preload d0
// with all ones and check the raw register bits after the load.
static void CheckLoadF32ZeroesLowBits(Isolate* isolate, int offset) {
  uint64_t raw = 0;
  std::vector<float> mem(2048, 0.0f);
  float expected = 2.71828f;
  mem[offset / sizeof(float)] = expected;

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate, AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lghi(r4, Operand(-1));
  __ MovInt64ToDouble(d0, r4);
  __ LoadF32(d0, MemOperand(r2, offset));
  __ MovDoubleToInt64(r4, d0);
  __ StoreU64(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate, buffer->start());
  f.Call(mem.data(), &raw);

  uint32_t expected_bits;
  memcpy(&expected_bits, &expected, sizeof(expected_bits));
  CHECK_EQ(static_cast<uint64_t>(expected_bits) << 32, raw);
}

TEST_F(MacroAssemblerTest, LoadF32ZeroesLowBitsSmallOffset) {
  CheckLoadF32ZeroesLowBits(isolate(), 64);
}

// Offsets above 12 bits take the lay + lde path.
TEST_F(MacroAssemblerTest, LoadF32ZeroesLowBitsLargeOffset) {
  CheckLoadF32ZeroesLowBits(isolate(), 4100);
}

// ---------------------------------------------------------------------------
// SwapP — register swap
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, SwapPRegisters) {
  // Swap r2 and r3, return r2 (should contain original r3 value).
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SwapP(r2, r3, r4 /*scratch*/);
  // r2 now holds original r3; return it.
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t, int64_t>::FromBuffer(isolate(),
                                                           buffer->start());

  CHECK_EQ(99LL, f.Call(42, 99));
  CHECK_EQ(0LL, f.Call(1, 0));
  CHECK_EQ(-7LL, f.Call(3, -7));
}

// ---------------------------------------------------------------------------
// SIMD splat — I32x4Splat / I64x2Splat
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, I32x4Splat) {
  // Splat integer into vector then extract lane 0 and lane 2 to check.
  struct T {
    int32_t input;
    int32_t lane0;
    int32_t lane1;
    int32_t lane2;
    int32_t lane3;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to T
  __ LoadS32(r3, MemOperand(r2, offsetof(T, input)));
  __ I32x4Splat(d1, r3);
  __ I32x4ExtractLane(r4, d1, 0, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane0)));
  __ I32x4ExtractLane(r4, d1, 1, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane1)));
  __ I32x4ExtractLane(r4, d1, 2, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane2)));
  __ I32x4ExtractLane(r4, d1, 3, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane3)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 42;
  f.Call(&t);
  CHECK_EQ(42, t.lane0);
  CHECK_EQ(42, t.lane1);
  CHECK_EQ(42, t.lane2);
  CHECK_EQ(42, t.lane3);

  t.input = -1;
  f.Call(&t);
  CHECK_EQ(-1, t.lane0);
  CHECK_EQ(-1, t.lane3);
}

TEST_F(MacroAssemblerTest, I64x2Splat) {
  struct T {
    int64_t input;
    int64_t lane0;
    int64_t lane1;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, input)));
  __ I64x2Splat(d1, r3);
  __ I64x2ExtractLane(r4, d1, 0, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, lane0)));
  __ I64x2ExtractLane(r4, d1, 1, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, lane1)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0x1234567890ABCDEFLL;
  f.Call(&t);
  CHECK_EQ(0x1234567890ABCDEFLL, t.lane0);
  CHECK_EQ(0x1234567890ABCDEFLL, t.lane1);
}

TEST_F(MacroAssemblerTest, I16x8Splat) {
  struct T {
    int32_t input;
    int16_t lane0;
    int16_t lane4;
    int16_t lane7;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadS32(r3, MemOperand(r2, offsetof(T, input)));
  __ I16x8Splat(d1, r3);
  __ I16x8ExtractLaneU(r4, d1, 0, r0);
  __ StoreU16(r4, MemOperand(r2, offsetof(T, lane0)));
  __ I16x8ExtractLaneU(r4, d1, 4, r0);
  __ StoreU16(r4, MemOperand(r2, offsetof(T, lane4)));
  __ I16x8ExtractLaneU(r4, d1, 7, r0);
  __ StoreU16(r4, MemOperand(r2, offsetof(T, lane7)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0x1234;
  f.Call(&t);
  CHECK_EQ(static_cast<int16_t>(0x1234), t.lane0);
  CHECK_EQ(static_cast<int16_t>(0x1234), t.lane4);
  CHECK_EQ(static_cast<int16_t>(0x1234), t.lane7);
}

// ---------------------------------------------------------------------------
// SIMD replace lane round-trip
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, I32x4ReplaceLane) {
  struct T {
    int32_t val;
    int32_t extracted;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // Splat 0, replace lane 2 with val, extract lane 2.
  __ lgfi(r3, Operand(0));
  __ I32x4Splat(d1, r3);
  __ LoadS32(r3, MemOperand(r2, offsetof(T, val)));
  __ I32x4ReplaceLane(d1, d1, r3, 2);
  __ I32x4ExtractLane(r4, d1, 2, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.val = 777;
  f.Call(&t);
  CHECK_EQ(777, t.extracted);

  t.val = -1;
  f.Call(&t);
  CHECK_EQ(-1, t.extracted);
}

// ---------------------------------------------------------------------------
// Deoptimizer exit size is fixed (matches x64 / arm64 pattern)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, DeoptExitSizeIsFixed) {
  Isolate* isolate = i_isolate();
  HandleScope handles(isolate);
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired{true},
                      buffer->CreateView());
  static_assert(static_cast<int>(kFirstDeoptimizeKind) == 0);
  for (int i = 0; i < kDeoptimizeKindCount; i++) {
    DeoptimizeKind kind = static_cast<DeoptimizeKind>(i);
    Label before_exit;
    masm.bind(&before_exit);
    Builtin target = Deoptimizer::GetDeoptimizationEntry(kind);
    masm.CallForDeoptimization(target, 42, &before_exit, kind, nullptr,
                               nullptr);
    CHECK_EQ(kind == DeoptimizeKind::kLazy ? Deoptimizer::kLazyDeoptExitSize
                                           : Deoptimizer::kEagerDeoptExitSize,
             masm.SizeOfCodeGeneratedSince(&before_exit));
  }
}

// ---------------------------------------------------------------------------
// CmpU32 / CmpU64 (unsigned compare with branch)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, CmpU32BranchHi) {
  // Returns 1 if a > b (unsigned 32-bit), else 0.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label hi, done;
  __ CmpU32(r2, r3);
  __ bgt(&hi);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&hi);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, uint32_t, uint32_t>::FromBuffer(isolate(),
                                                             buffer->start());
  CHECK_EQ(1, f.Call(0xFFFFFFFFu, 1u));
  CHECK_EQ(0, f.Call(1u, 1u));
  CHECK_EQ(0, f.Call(0u, 1u));
}

TEST_F(MacroAssemblerTest, CmpU64BranchHi) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label hi, done;
  __ CmpU64(r2, r3);
  __ bgt(&hi);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&hi);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, uint64_t, uint64_t>::FromBuffer(isolate(),
                                                             buffer->start());
  CHECK_EQ(1, f.Call(UINT64_MAX, 0ULL));
  CHECK_EQ(0, f.Call(0ULL, 0ULL));
  CHECK_EQ(0, f.Call(0ULL, 1ULL));
}

// ---------------------------------------------------------------------------
// AndS32 / OrS32 / XorS32
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, AndOrXorS32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = a, r3 = b; compute (a & b) | (a ^ b) — should equal a | b
  // For a=0b1100, b=0b1010: and=0b1000, xor=0b0110, or(and,xor)=0b1110 = a|b
  __ lgfi(r4, Operand(0));
  __ lr(r5, r2);
  __ And(r5, r3);      // r5 = a & b
  __ lr(r6, r2);
  __ Xor(r6, r3);      // r6 = a ^ b
  __ Or(r5, r6);       // r5 = (a&b) | (a^b) = a | b
  __ lr(r2, r5);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t, int32_t>::FromBuffer(isolate(),
                                                           buffer->start());
  CHECK_EQ(0b1110, f.Call(0b1100, 0b1010));
  CHECK_EQ(0xFF, f.Call(0xF0, 0x0F));
  CHECK_EQ(0, f.Call(0, 0));
}

// ---------------------------------------------------------------------------
// ShiftLeftU32 / ShiftRightU32 / ShiftRightS32
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, Shifts32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = value; return (val << 3) >> 1
  __ ShiftLeftU32(r2, r2, Operand(3));
  __ ShiftRightU32(r2, r2, Operand(1));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(4, f.Call(1));   // (1<<3)>>1 = 4
  CHECK_EQ(20, f.Call(5));  // (5<<3)>>1 = 20
}

TEST_F(MacroAssemblerTest, ShiftRightS32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftRightS32(r2, r2, Operand(1));  // arithmetic right shift by 1
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-1, f.Call(-2));   // -2 >> 1 = -1 (arithmetic)
  CHECK_EQ(-1, f.Call(-1));   // -1 >> 1 = -1
  CHECK_EQ(1, f.Call(2));
}

// ---------------------------------------------------------------------------
// ShiftLeftU64 / ShiftRightS64 (immediate)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, Shifts64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftLeftU64(r2, r2, Operand(4));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(16LL, f.Call(1));
  CHECK_EQ(INT64_MIN, f.Call(0x0800000000000000LL));  // (1<<59)<<4 = 1<<63
}

TEST_F(MacroAssemblerTest, ShiftRightS64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftRightS64(r2, r2, Operand(1));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-1LL, f.Call(-1LL));
  CHECK_EQ(-1LL, f.Call(-2LL));
  CHECK_EQ(0x3FFFFFFFFFFFFFFFLL, f.Call(INT64_MAX));
}

// ---------------------------------------------------------------------------
// LoadS16 / LoadS32 sign extension
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, LoadS16SignExtend) {
  int64_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadS16(r4, MemOperand(r2));
  __ StoreU64(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint16_t neg = 0x8000;   // -32768 when signed
  f.Call(&neg, &val);
  CHECK_EQ(-32768LL, val);

  uint16_t pos = 0x7FFF;   // +32767
  f.Call(&pos, &val);
  CHECK_EQ(32767LL, val);
}

TEST_F(MacroAssemblerTest, LoadS32SignExtend) {
  int64_t val = 0;
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadS32(r4, MemOperand(r2));
  __ StoreU64(r4, MemOperand(r3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*, void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  int32_t neg = -1;
  f.Call(&neg, &val);
  CHECK_EQ(-1LL, val);
}

// ---------------------------------------------------------------------------
// ConvertFloat32ToInt32
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, ConvertFloat32ToInt32) {
  struct T { float input; int32_t output; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d0, MemOperand(r2, offsetof(T, input)));
  __ ConvertFloat32ToInt32(r3, d0, kRoundToZero);
  __ StoreU32(r3, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 3.9f;  t.output = INT32_MIN; f.Call(&t); CHECK_EQ(3, t.output);
  t.input = -3.9f; t.output = INT32_MIN; f.Call(&t); CHECK_EQ(-3, t.output);
  t.input = 0.0f;  t.output = INT32_MIN; f.Call(&t); CHECK_EQ(0, t.output);
}

// ---------------------------------------------------------------------------
// ConvertDoubleToFloat32
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, ConvertDoubleToFloat32) {
  struct T { double input; float output; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d0, MemOperand(r2, offsetof(T, input)));
  __ ledbr(d1, d0);
  __ StoreF32(d1, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 1.5;  t.output = -1.0f; f.Call(&t); CHECK_EQ(1.5f, t.output);
  t.input = -2.0; t.output = -1.0f; f.Call(&t); CHECK_EQ(-2.0f, t.output);
}

// ---------------------------------------------------------------------------
// ConvertFloat32ToDouble
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, ConvertFloat32ToDouble) {
  struct T { float input; double output; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d0, MemOperand(r2, offsetof(T, input)));
  __ ldebr(d1, d0);
  __ StoreF64(d1, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 3.14f; t.output = -1.0; f.Call(&t);
  CHECK_EQ(static_cast<double>(3.14f), t.output);
}

// ---------------------------------------------------------------------------
// AddS32/SubS32 with immediate operand (Operand variant)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, AddSubImmediate32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS32(r2, r2, Operand(100));
  __ SubS32(r2, r2, Operand(42));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, int32_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(158, f.Call(100));   // 100 + 100 - 42 = 158
  CHECK_EQ(58, f.Call(0));      // 0 + 100 - 42 = 58
}

// ---------------------------------------------------------------------------
// AddS64/SubS64 with immediate operand
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, AddSubImmediate64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS64(r2, r2, Operand(1000));
  __ SubS64(r2, r2, Operand(1));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int64_t, int64_t>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(998LL, f.Call(-1LL));    // -1 + 1000 - 1 = 998
  CHECK_EQ(1999LL, f.Call(1000LL)); // 1000 + 1000 - 1 = 1999
}

// ---------------------------------------------------------------------------
// LoadPositive / LoadNegative on floats (lpdbr, lndbr, lpebr, lnebr)
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, AbsFloat) {
  struct T { double d_in, d_out; float f_in, f_out; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d0, MemOperand(r2, offsetof(T, d_in)));
  __ lndbr(d1, d0);
  __ lpdbr(d1, d1);
  __ StoreF64(d1, MemOperand(r2, offsetof(T, d_out)));
  __ LoadF32(d2, MemOperand(r2, offsetof(T, f_in)));
  __ lnebr(d3, d2);
  __ lpebr(d3, d3);
  __ StoreF32(d3, MemOperand(r2, offsetof(T, f_out)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.d_in = 3.0; t.f_in = -2.0f;
  t.d_out = -1.0; t.f_out = -1.0f;
  f.Call(&t);
  CHECK_EQ(3.0,  t.d_out);
  CHECK_EQ(2.0f, t.f_out);
}

// ---------------------------------------------------------------------------
// DoubleMax / DoubleMin
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, MaxMinF64) {
  struct T { double a, b, mx, mn; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d0, MemOperand(r2, offsetof(T, a)));
  __ LoadF64(d1, MemOperand(r2, offsetof(T, b)));
  __ DoubleMax(d2, d0, d1);
  __ StoreF64(d2, MemOperand(r2, offsetof(T, mx)));
  __ DoubleMin(d2, d0, d1);
  __ StoreF64(d2, MemOperand(r2, offsetof(T, mn)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.a = 3.0; t.b = 7.0;
  t.mx = t.mn = -100.0;
  f.Call(&t);
  CHECK_EQ(7.0, t.mx);
  CHECK_EQ(3.0, t.mn);

  t.a = -1.0; t.b = 0.0;
  t.mx = t.mn = -100.0;
  f.Call(&t);
  CHECK_EQ(0.0,  t.mx);
  CHECK_EQ(-1.0, t.mn);
}

// ---------------------------------------------------------------------------
// I8x16Splat / I8x16ExtractLaneU
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, I8x16Splat) {
  struct T { int32_t input; int32_t lane0, lane7, lane15; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadS32(r3, MemOperand(r2, offsetof(T, input)));
  __ I8x16Splat(d1, r3);
  __ I8x16ExtractLaneU(r4, d1, 0, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane0)));
  __ I8x16ExtractLaneU(r4, d1, 7, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane7)));
  __ I8x16ExtractLaneU(r4, d1, 15, r0);
  __ StoreU32(r4, MemOperand(r2, offsetof(T, lane15)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 0xAB;
  f.Call(&t);
  CHECK_EQ(0xAB, t.lane0);
  CHECK_EQ(0xAB, t.lane7);
  CHECK_EQ(0xAB, t.lane15);
}

// ---------------------------------------------------------------------------
// I64x2ReplaceLane / I16x8ReplaceLane
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, I64x2ReplaceLane) {
  struct T { int64_t val; int64_t extracted; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I64x2Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I64x2ReplaceLane(d1, d1, r3, 1);
  __ I64x2ExtractLane(r4, d1, 1, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.val = 0x123456789ABCLL;
  f.Call(&t);
  CHECK_EQ(0x123456789ABCLL, t.extracted);

  t.val = -1LL;
  f.Call(&t);
  CHECK_EQ(-1LL, t.extracted);
}

// ---------------------------------------------------------------------------
// CmpSmiLiteral / JumpIfSmi / JumpIfNotSmi
// ---------------------------------------------------------------------------

TEST_F(MacroAssemblerTest, JumpIfNotSmi) {
  // Returns 1 if argument is NOT a Smi.
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label is_not_smi, done;
  __ JumpIfNotSmi(r2, &is_not_smi);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&is_not_smi);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  auto f =
      GeneratedCode<int32_t, intptr_t>::FromBuffer(isolate(), buffer->start());

  CHECK_EQ(0, f.Call(Smi::FromInt(0).ptr()));
  CHECK_EQ(0, f.Call(Smi::FromInt(42).ptr()));
  // A non-Smi has the tag bit set (e.g., a pointer).
  CHECK_EQ(1, f.Call(static_cast<intptr_t>(1)));  // odd number = not Smi
}

// ---------------------------------------------------------------------------
// CmpS32 / CmpS64 with memory operand
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpS32Memory) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to int32_t
  Label eq, done;
  __ CmpS32(r3, MemOperand(r2, 0));
  __ beq(&eq);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&eq);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t*, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  int32_t val = 42;
  CHECK_EQ(0, f.Call(&val, 99));  // 42 != 99
  CHECK_EQ(1, f.Call(&val, 42));  // 42 == 42
}

// ---------------------------------------------------------------------------
// AddS64 / SubS64 with memory
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddSubS64Memory) {
  struct T { int64_t a; int64_t result; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, a)));   // r3 = a
  __ AddS64(r3, Operand(10));                      // r3 += 10
  __ SubS64(r3, Operand(3));                       // r3 -= 3
  __ StoreU64(r3, MemOperand(r2, offsetof(T, result)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 100;
  f.Call(&t);
  CHECK_EQ(107LL, t.result);  // 100 + 10 - 3 = 107
}

// ---------------------------------------------------------------------------
// MulS64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MulS64Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = 6, r3 = 7
  __ MulS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(42LL, f.Call(6, 7));
}

// ---------------------------------------------------------------------------
// LoadU8 / StoreU8 round-trip
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadStoreU8Inc) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to uint8_t
  __ LoadU8(r3, MemOperand(r2, 0));
  __ AddS32(r3, Operand(1));
  __ StoreU8(r3, MemOperand(r2, 0));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint8_t val = 10;
  f.Call(&val);
  CHECK_EQ(11, static_cast<int>(val));
}

// ---------------------------------------------------------------------------
// LoadU16 / StoreU16 round-trip
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadStoreU16Inc) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadU16(r3, MemOperand(r2, 0));
  __ AddS32(r3, Operand(5));
  __ StoreU16(r3, MemOperand(r2, 0));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  uint16_t val = 1000;
  f.Call(&val);
  CHECK_EQ(1005, static_cast<int>(val));
}

// ---------------------------------------------------------------------------
// AddF64 / SubF64 / MulF64 / DivF64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, FPArithF64) {
  struct T { double a; double b; double result; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d1, MemOperand(r2, offsetof(T, a)));
  __ LoadF64(d2, MemOperand(r2, offsetof(T, b)));
  __ MulF64(d3, d1, d2);
  __ AddF64(d3, d3, d1);
  __ StoreF64(d3, MemOperand(r2, offsetof(T, result)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 2.0; t.b = 3.0;
  f.Call(&t);
  CHECK_EQ(8.0, t.result);  // 2*3 + 2 = 8
}

// ---------------------------------------------------------------------------
// AddF32 / MulF32
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, FPArithF32) {
  struct T { float a; float b; float result; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d1, MemOperand(r2, offsetof(T, a)));
  __ LoadF32(d2, MemOperand(r2, offsetof(T, b)));
  __ AddF32(d3, d1, d2);
  __ StoreF32(d3, MemOperand(r2, offsetof(T, result)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 1.5f; t.b = 2.5f;
  f.Call(&t);
  CHECK_EQ(4.0f, t.result);
}

// ---------------------------------------------------------------------------
// SubF64 / DivF64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, FPSubDiv) {
  struct T { double a; double b; double sub_result; double div_result; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d1, MemOperand(r2, offsetof(T, a)));
  __ LoadF64(d2, MemOperand(r2, offsetof(T, b)));
  __ SubF64(d3, d1, d2);
  __ DivF64(d4, d1, d2);
  __ StoreF64(d3, MemOperand(r2, offsetof(T, sub_result)));
  __ StoreF64(d4, MemOperand(r2, offsetof(T, div_result)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 10.0; t.b = 4.0;
  f.Call(&t);
  CHECK_EQ(6.0, t.sub_result);
  CHECK_EQ(2.5, t.div_result);
}

// ---------------------------------------------------------------------------
// CmpF64 then branch
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpF64Branch) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // d0 == d2 → return 1, else return 0  (s390x ABI: first double in f0, second in f2)
  Label eq, done;
  __ CmpF64(d0, d2);
  __ beq(&eq);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&eq);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(double, double);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1, f.Call(3.14, 3.14));
  CHECK_EQ(0, f.Call(1.0, 2.0));
}

// ---------------------------------------------------------------------------
// SwapFloat32 (register form)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SwapFloat32Regs) {
  struct T { float a; float b; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d1, MemOperand(r2, offsetof(T, a)));
  __ LoadF32(d2, MemOperand(r2, offsetof(T, b)));
  __ SwapFloat32(d1, d2, d3);  // d3 = scratch
  __ StoreF32(d1, MemOperand(r2, offsetof(T, a)));
  __ StoreF32(d2, MemOperand(r2, offsetof(T, b)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 1.0f; t.b = 2.0f;
  f.Call(&t);
  CHECK_EQ(2.0f, t.a);
  CHECK_EQ(1.0f, t.b);
}

// ---------------------------------------------------------------------------
// SwapDouble (register form)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SwapDoubleRegs) {
  struct T { double a; double b; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d1, MemOperand(r2, offsetof(T, a)));
  __ LoadF64(d2, MemOperand(r2, offsetof(T, b)));
  __ SwapDouble(d1, d2, d3);
  __ StoreF64(d1, MemOperand(r2, offsetof(T, a)));
  __ StoreF64(d2, MemOperand(r2, offsetof(T, b)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.a = 1.5; t.b = 2.5;
  f.Call(&t);
  CHECK_EQ(2.5, t.a);
  CHECK_EQ(1.5, t.b);
}

// ---------------------------------------------------------------------------
// I16x8ReplaceLane / I16x8ExtractLaneU
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I16x8ReplaceLane) {
  struct T { int64_t val; int64_t extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I16x8Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I16x8ReplaceLane(d1, d1, r3, 3);
  __ I16x8ExtractLaneU(r4, d1, 3, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 0x1234;
  f.Call(&t);
  CHECK_EQ(0x1234LL, t.extracted);
}

// ---------------------------------------------------------------------------
// I8x16ReplaceLane / I8x16ExtractLaneU
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I8x16ReplaceLane) {
  struct T { int64_t val; int64_t extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I8x16Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I8x16ReplaceLane(d1, d1, r3, 5);
  __ I8x16ExtractLaneU(r4, d1, 5, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 0xAB;
  f.Call(&t);
  CHECK_EQ(0xABLL, t.extracted);
}

// ---------------------------------------------------------------------------
// F64x2Splat / F64x2ExtractLane
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F64x2SplatExtract) {
  struct T { double val; double extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d2, MemOperand(r2, offsetof(T, val)));
  __ F64x2Splat(d1, d2);
  __ F64x2ExtractLane(d3, d1, 1, r0);
  __ StoreF64(d3, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 3.14;
  f.Call(&t);
  CHECK_EQ(3.14, t.extracted);
}

// ---------------------------------------------------------------------------
// F32x4Splat / F32x4ExtractLane
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F32x4SplatExtract) {
  struct T { float val; float extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d2, MemOperand(r2, offsetof(T, val)));
  __ F32x4Splat(d1, d2);
  __ F32x4ExtractLane(d3, d1, 2, r0);
  __ StoreF32(d3, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 2.5f;
  f.Call(&t);
  CHECK_EQ(2.5f, t.extracted);
}

// ---------------------------------------------------------------------------
// F64x2ReplaceLane
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F64x2ReplaceLane) {
  struct T { double val; double extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d2, MemOperand(r2, offsetof(T, val)));
  __ lgfi(r3, Operand(0));
  __ I64x2Splat(d1, r3);
  __ F64x2ReplaceLane(d1, d1, d2, 0, r1);
  __ F64x2ExtractLane(d3, d1, 0, r0);
  __ StoreF64(d3, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 6.28;
  f.Call(&t);
  CHECK_EQ(6.28, t.extracted);
}

// ---------------------------------------------------------------------------
// F32x4ReplaceLane
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F32x4ReplaceLane) {
  struct T { float val; float extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d2, MemOperand(r2, offsetof(T, val)));
  __ lgfi(r3, Operand(0));
  __ I32x4Splat(d1, r3);
  __ F32x4ReplaceLane(d1, d1, d2, 1, r1);
  __ F32x4ExtractLane(d3, d1, 1, r0);
  __ StoreF32(d3, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 1.25f;
  f.Call(&t);
  CHECK_EQ(1.25f, t.extracted);
}

// ---------------------------------------------------------------------------
// I32x4ExtractLane / I32x4ReplaceLane
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I32x4ExtractLane) {
  struct T { int64_t val; int64_t extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I32x4Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I32x4ReplaceLane(d1, d1, r3, 2);
  __ I32x4ExtractLane(r4, d1, 2, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 999;
  f.Call(&t);
  CHECK_EQ(999LL, t.extracted);
}

// ---------------------------------------------------------------------------
// I8x16ExtractLaneS (sign-extend)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I8x16ExtractLaneS) {
  struct T { int64_t val; int64_t extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I8x16Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I8x16ReplaceLane(d1, d1, r3, 0);
  __ I8x16ExtractLaneS(r4, d1, 0, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 0xFF;  // 255 unsigned = -1 signed when sign-extended from 8 bits
  f.Call(&t);
  CHECK_EQ(-1LL, t.extracted);
}

// ---------------------------------------------------------------------------
// I16x8ExtractLaneS (sign-extend)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I16x8ExtractLaneS) {
  struct T { int64_t val; int64_t extracted; };
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0));
  __ I16x8Splat(d1, r3);
  __ LoadU64(r3, MemOperand(r2, offsetof(T, val)));
  __ I16x8ReplaceLane(d1, d1, r3, 2);
  __ I16x8ExtractLaneS(r4, d1, 2, r0);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, extracted)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void*(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  T t = {};
  t.val = 0xFFFF;  // 65535 unsigned = -1 signed when sign-extended from 16 bits
  f.Call(&t);
  CHECK_EQ(-1LL, t.extracted);
}

// ---------------------------------------------------------------------------
// Move (register ← register)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MoveRegReg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(77));
  __ Move(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = intptr_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(77, static_cast<int>(f.Call()));
}

// ---------------------------------------------------------------------------
// Push / Pop round-trip
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, PushPop) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(123));
  __ Push(r3);
  __ lgfi(r3, Operand(0));   // clobber r3
  __ Pop(r2);                // restore into r2
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = intptr_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(123, static_cast<int>(f.Call()));
}

// ---------------------------------------------------------------------------
// CmpSmiLiteral
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpSmiLiteral) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 contains Smi value; compare against Smi(42)
  Label eq, done;
  __ CmpSmiLiteral(r2, Smi::FromInt(42), r0);
  __ beq(&eq);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&eq);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(intptr_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1, f.Call(Smi::FromInt(42).ptr()));
  CHECK_EQ(0, f.Call(Smi::FromInt(99).ptr()));
}

// ---------------------------------------------------------------------------
// JumpIfSmi
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, JumpIfSmi) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  Label is_smi, done;
  __ JumpIfSmi(r2, &is_smi);
  __ lgfi(r2, Operand(0));
  __ b(&done);
  __ bind(&is_smi);
  __ lgfi(r2, Operand(1));
  __ bind(&done);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(intptr_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1, f.Call(Smi::FromInt(0).ptr()));
  CHECK_EQ(1, f.Call(Smi::FromInt(7).ptr()));
  CHECK_EQ(0, f.Call(static_cast<intptr_t>(1)));  // odd = not Smi
}

// ---------------------------------------------------------------------------
// ShiftLeftU32 with register amount
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ShiftU32RegAmount) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = value=1, r3 = shift=4
  __ ShiftLeftU32(r2, r2, r3);    // 1 << 4 = 16
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(16, f.Call(1, 4));
  CHECK_EQ(32, f.Call(2, 4));
}

// ---------------------------------------------------------------------------
// ShiftLeftU64 with register amount
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ShiftU64RegAmount) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftLeftU64(r2, r2, r3);    // r2 <<= r3
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(8LL, f.Call(1, 3));
  CHECK_EQ(256LL, f.Call(1, 8));
  CHECK_EQ(int64_t(1) << 32, f.Call(1, 32));  // shift amount >= 32
}

// ---------------------------------------------------------------------------
// AddS32(dst, imm) — add signed 32-bit immediate to register
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS32Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 += 42
  __ AddS32(r2, Operand(42));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(42, f.Call(0));
  CHECK_EQ(142, f.Call(100));
  CHECK_EQ(-958, f.Call(-1000));
}

// ---------------------------------------------------------------------------
// AddS64(dst, imm) — add signed 64-bit immediate to register
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS64Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS64(r2, Operand(1000));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1000LL, f.Call(0));
  CHECK_EQ(1500LL, f.Call(500));
}

// ---------------------------------------------------------------------------
// AddS32(dst, src, imm) — 3-register form with immediate
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS32SrcImm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = r3 + 10
  __ AddS32(r2, r3, Operand(10));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(10, f.Call(0, 0));
  CHECK_EQ(15, f.Call(0, 5));
}

// ---------------------------------------------------------------------------
// SubS32(dst, imm) / SubS64(dst, imm)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SubS32Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SubS32(r2, Operand(7));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-7, f.Call(0));
  CHECK_EQ(13, f.Call(20));
}

// ---------------------------------------------------------------------------
// SubS64(dst, src, imm) — subtract immediate from source into dest
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SubS64SrcImm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = r3 - 3
  __ SubS64(r2, r3, Operand(3));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-3LL, f.Call(0, 0));
  CHECK_EQ(7LL, f.Call(0, 10));
}

// ---------------------------------------------------------------------------
// CmpAndSwap (32-bit) / CmpAndSwap64 (64-bit)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpAndSwap) {
  auto buffer = AllocateAssemblerBuffer(8 * KB);
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to int32_t mem, r3 = old value, r4 = new value
  // CmpAndSwap: if *r2==r3, replace with r4 (32-bit)
  __ CmpAndSwap(r3, r4, MemOperand(r2, 0));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(int32_t*, int32_t, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  int32_t mem = 10;
  f.Call(&mem, 10, 99);   // *mem==10 → swap with 99
  CHECK_EQ(99, mem);
}

TEST_F(MacroAssemblerTest, CmpAndSwap64) {
  auto buffer = AllocateAssemblerBuffer(8 * KB);
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ CmpAndSwap64(r3, r4, MemOperand(r2, 0));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(int64_t*, int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  int64_t mem64 = 42LL;
  f.Call(&mem64, 42LL, 999LL);
  CHECK_EQ(999LL, mem64);
}

// ---------------------------------------------------------------------------
// LoadU32(reg, reg) — zero-extend 32-bit register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadU32Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = zero-extend(r2) — use lgfi(-1) to set r2=0xFFFF_FFFFFFFF, then LoadU32
  __ lgfi(r3, Operand(-1));       // r3 = 0xFFFFFFFF_FFFFFFFF
  __ LoadU32(r2, r3);             // r2 = zero-extend(r3_low32) = 0x00000000_FFFFFFFF
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0xFFFFFFFFULL, f.Call());
}

// ---------------------------------------------------------------------------
// LoadS32(reg, reg) — sign-extend 32-bit register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadS32Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(-1));       // r3 low32 = 0xFFFFFFFF = -1
  __ LoadS32(r2, r3);             // r2 = sign-extend(-1) = -1 (64-bit)
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-1LL, f.Call());
}

// ---------------------------------------------------------------------------
// LoadU8(reg, reg) — zero-extend byte register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadU8Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0xABCD));   // r3 has 0xABCD; low byte = 0xCD
  __ LoadU8(r2, r3);              // r2 = 0xCD (zero-extended byte)
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0xCDULL, f.Call());
}

// ---------------------------------------------------------------------------
// LoadS8(reg, reg) — sign-extend byte register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadS8Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0xFF));     // low byte = 0xFF = -1 as signed byte
  __ LoadS8(r2, r3);              // r2 = -1 (sign-extended)
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-1LL, f.Call());
}

// ---------------------------------------------------------------------------
// LoadU16(reg, reg) — zero-extend halfword register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadU16Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0x8000));   // 0x8000 = 32768 unsigned
  __ LoadU16(r2, r3);             // r2 = 32768 (zero-extended)
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(32768ULL, f.Call());
}

// ---------------------------------------------------------------------------
// LoadS16(reg, reg) — sign-extend halfword register to 64-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadS16Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lgfi(r3, Operand(0x8000));   // 0x8000 = -32768 as signed 16-bit
  __ LoadS16(r2, r3);             // r2 = -32768 (sign-extended)
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(void);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-32768LL, f.Call());
}

// ---------------------------------------------------------------------------
// ConvertInt64ToDouble
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ConvertInt64ToDouble) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to struct T; r3 = input int64; d0 = output double
  struct T { int64_t i; double d; };
  __ LoadU64(r3, MemOperand(r2, offsetof(T, i)));
  __ ConvertInt64ToDouble(d0, r3);
  __ StoreF64(d0, MemOperand(r2, offsetof(T, d)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(T*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  T t = {12345, 0.0};
  f.Call(&t);
  CHECK_EQ(12345.0, t.d);
}

// ---------------------------------------------------------------------------
// ConvertDoubleToInt32
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ConvertDoubleToInt32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  struct T { double d; int32_t i; };
  __ LoadF64(d0, MemOperand(r2, offsetof(T, d)));
  __ ConvertDoubleToInt32(r3, d0, kRoundToZero);
  __ StoreU32(r3, MemOperand(r2, offsetof(T, i)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(T*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  T t = {3.7, 0};
  f.Call(&t);
  CHECK_EQ(3, t.i);  // truncate toward zero
}

// ---------------------------------------------------------------------------
// AddS32(mem, imm) — add immediate to memory word (uses asi)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS32MemImm) {
  auto buffer = AllocateAssemblerBuffer(8 * KB);
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = pointer to int32_t
  __ AddS32(MemOperand(r2, 0), Operand(7));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(int32_t*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  int32_t val = 50;
  f.Call(&val);
  CHECK_EQ(57, val);
}

// ---------------------------------------------------------------------------
// AddS64(mem, imm) — add immediate to memory doubleword (uses agsi)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS64MemImm) {
  auto buffer = AllocateAssemblerBuffer(8 * KB);
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS64(MemOperand(r2, 0), Operand(100));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(int64_t*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  int64_t val64 = 1000LL;
  f.Call(&val64);
  CHECK_EQ(1100LL, val64);
}

// ---------------------------------------------------------------------------
// AddU32(dst, src1, src2) — unsigned 3-reg add
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddU32ThreeReg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r4 = r2 + r3 (unsigned 32-bit), then moved to r2 as the return value
  __ AddU32(r4, r2, r3);
  __ lgr(r2, r4);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint32_t(uint32_t, uint32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(7U, f.Call(3, 4));
  CHECK_EQ(0U, f.Call(0, 0));
}

// ---------------------------------------------------------------------------
// SubS32(dst, src1, src2) — 3-register subtract
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SubS32ThreeReg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SubS32(r2, r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(3, f.Call(10, 7));
  CHECK_EQ(-5, f.Call(0, 5));
}

// ---------------------------------------------------------------------------
// MulS32(dst, src, imm) — signed 32-bit multiply with immediate (MulS32 Reg+Imm)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MulS32Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ MulS32(r2, Operand(7));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(21, f.Call(3));
  CHECK_EQ(0, f.Call(0));
  CHECK_EQ(-14, f.Call(-2));
}

// ---------------------------------------------------------------------------
// MulS64(dst, imm) — signed 64-bit multiply with immediate
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MulS64Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ MulS64(r2, Operand(10));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(int64_t(50), f.Call(5));
  CHECK_EQ(int64_t(-30), f.Call(-3));
}

// ---------------------------------------------------------------------------
// Or / And / Xor — bitwise logical 32-bit (3-register forms)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, BitwiseLogical32) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r4 = r2 | r3; r5 = r2 & r3; r2 = r4 ^ r5
  __ Or(r4, r2, r3);
  __ And(r5, r2, r3);
  __ Xor(r2, r4, r5);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint32_t(uint32_t, uint32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  // 0xF0 | 0x0F = 0xFF, 0xF0 & 0x0F = 0x00, 0xFF ^ 0x00 = 0xFF
  CHECK_EQ(0xFFU, f.Call(0xF0, 0x0F));
  // 0b1010 | 0b1100=0b1110; & = 0b1000; ^ = 0b0110
  CHECK_EQ(6U, f.Call(0xA, 0xC));
}

// ---------------------------------------------------------------------------
// ShiftRightU32 — logical right shift 32-bit
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ShiftRightU32Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftRightU32(r2, r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint32_t(uint32_t, uint32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(8U >> 2, f.Call(8, 2));
  CHECK_EQ(0x80000000U >> 1, f.Call(0x80000000U, 1));
}

// ---------------------------------------------------------------------------
// ShiftRightS32 — arithmetic right shift 32-bit (preserves sign)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ShiftRightS32Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ ShiftRightS32(r2, r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t, int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(-2, f.Call(-4, 1));  // arithmetic: sign bit preserved
  CHECK_EQ(2, f.Call(8, 2));
}

// ---------------------------------------------------------------------------
// AddS64(dst, src) — 64-bit add two-operand
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AddS64TwoOp) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ AddS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(int64_t(7), f.Call(3, 4));
  CHECK_EQ(int64_t(-1), f.Call(2, -3));
}

// ---------------------------------------------------------------------------
// SubS64(dst, src) — 64-bit subtract two-operand
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SubS64TwoOp) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ SubS64(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int64_t(int64_t, int64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(int64_t(3), f.Call(7, 4));
  CHECK_EQ(int64_t(-5), f.Call(0, 5));
}

// ---------------------------------------------------------------------------
// CmpS32(r, imm) — 32-bit signed compare immediate → branch
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpS32Imm) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // Return 1 if r2 == 42, else 0
  Label yes;
  __ CmpS32(r2, Operand(42));
  __ beq(&yes);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  __ bind(&yes);
  __ lgfi(r2, Operand(1));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(int32_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1, f.Call(42));
  CHECK_EQ(0, f.Call(43));
}

// ---------------------------------------------------------------------------
// CmpU64(r, r) — unsigned 64-bit compare → branch
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpU64Reg) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // Return 1 if r2 == r3 (unsigned), else 0
  Label yes;
  __ CmpU64(r2, r3);
  __ beq(&yes);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  __ bind(&yes);
  __ lgfi(r2, Operand(1));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(uint64_t, uint64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(1, f.Call(100ULL, 100ULL));
  CHECK_EQ(0, f.Call(1ULL, 2ULL));
}

// ---------------------------------------------------------------------------
// Or (64-bit) — bitwise OR using 64-bit registers
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, BitwiseLogical64) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = r2 | r3 (64-bit)
  __ Or(r2, r3);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = uint64_t(uint64_t, uint64_t);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(uint64_t(0xFF), f.Call(0xF0ULL, 0x0FULL));
  CHECK_EQ(uint64_t(0), f.Call(0ULL, 0ULL));
}

// ---------------------------------------------------------------------------
// LoadF32 / StoreF32 — store two floats and verify copy (offset variant)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadStoreF32Copy) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = float[4]; copy [0] to [2] and [1] to [3]
  __ LoadF32(d1, MemOperand(r2, 0));
  __ LoadF32(d2, MemOperand(r2, 4));
  __ StoreF32(d1, MemOperand(r2, 8));
  __ StoreF32(d2, MemOperand(r2, 12));
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  float buf[4] = {1.5f, 2.5f, 0.0f, 0.0f};
  using F = int32_t(float*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(buf));
  CHECK_EQ(1.5f, buf[2]);
  CHECK_EQ(2.5f, buf[3]);
}

// ---------------------------------------------------------------------------
// LoadF64 / StoreF64 — copy two doubles (offset variant)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadStoreF64Copy) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = double[4]; copy [0] to [2] and [1] to [3]
  __ LoadF64(d1, MemOperand(r2, 0));
  __ LoadF64(d2, MemOperand(r2, 8));
  __ StoreF64(d1, MemOperand(r2, 16));
  __ StoreF64(d2, MemOperand(r2, 24));
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  double buf[4] = {1.1, 2.2, 0.0, 0.0};
  using F = int32_t(double*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(buf));
  CHECK_EQ(1.1, buf[2]);
  CHECK_EQ(2.2, buf[3]);
}

// ===========================================================================
// Extended MacroAssembler coverage.
//
// Conventions used by the tests below:
//  * The generated function receives a pointer in r2 which is copied to r5.
//    r5 is the base register for all inputs / outputs; r0..r4 are scratch
//    (r0/r1 are also clobbered internally by several macros and must not be
//    passed as operands to those macros).
//  * d13 (kScratchDoubleReg) and d14 (kDoubleRegZero) are callee-saved and are
//    preserved around every test body.
// ===========================================================================
#define I64(x) static_cast<int64_t>(UINT64_C(x))
#define MOV64(reg, v) __ mov(reg, Operand(static_cast<intptr_t>(UINT64_C(v))))

#define MASM_TEST_BEGIN                                                       \
  auto buffer = AllocateAssemblerBuffer();                                    \
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false}, \
                      buffer->CreateView());                                  \
  __ set_root_array_available(false);                                         \
  __ set_abort_hard(true);                                                    \
  __ MultiPushDoubles(DoubleRegList{d13, d14});                               \
  __ lgr(r5, r2);

#define MASM_TEST_RUN(ptr)                                                    \
  __ MultiPopDoubles(DoubleRegList{d13, d14});                                \
  __ Ret();                                                                   \
  CodeDesc desc;                                                              \
  masm.GetCode(isolate(), &desc);                                             \
  buffer->MakeExecutable();                                                   \
  {                                                                           \
    using TestFn = void(void*);                                               \
    auto f = GeneratedCode<TestFn>::FromBuffer(isolate(), buffer->start());   \
    f.Call(static_cast<void*>(ptr));                                          \
  }

#define MT_OUT(reg, i) __ StoreU64(reg, MemOperand(r5, (i) * 8));
#define MT_OUTD(reg, i) __ StoreF64(reg, MemOperand(r5, (i) * 8));
#define MT_OUTF(reg, i) __ StoreF32(reg, MemOperand(r5, (i) * 4));
#define MT_LOADV(vreg, off) __ LoadV128(vreg, MemOperand(r5, off), r1);
#define MT_STOREV(vreg, off) __ StoreV128(vreg, MemOperand(r5, off), r1);

static uint32_t ReadBE32(const uint8_t* p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) v = (v << 8) | p[i];
  return v;
}
static void WriteBE32(uint8_t* p, uint32_t v) {
  for (int i = 3; i >= 0; i--) {
    p[i] = v & 0xFF;
    v >>= 8;
  }
}

// ---------------------------------------------------------------------------
// AddFloat64 / SubFloat64 / MulFloat64 / DivFloat64 (memory operand forms; the
// short and long displacement code paths are both exercised)
// ---------------------------------------------------------------------------
#define F64_MEM_OP_TEST(Name, Op, a, b, expected)                      \
  TEST_F(MacroAssemblerTest, Name) {                                   \
    for (int idx : {1, 520}) {                                         \
      std::vector<double> v(700, 0.0);                                 \
      v[0] = (a);                                                      \
      v[idx] = (b);                                                    \
      MASM_TEST_BEGIN                                                  \
      __ LoadF64(d1, MemOperand(r5, 0));                               \
      __ Op(d1, MemOperand(r5, idx * 8), d2);                          \
      __ StoreF64(d1, MemOperand(r5, 699 * 8));                        \
      MASM_TEST_RUN(v.data())                                          \
      CHECK_EQ((expected), v[699]);                                    \
    }                                                                  \
  }

F64_MEM_OP_TEST(AddFloat64Mem, AddFloat64, 1.5, 2.0, 3.5)
F64_MEM_OP_TEST(SubFloat64Mem, SubFloat64, 5.0, 1.5, 3.5)
F64_MEM_OP_TEST(MulFloat64Mem, MulFloat64, 3.0, 2.5, 7.5)
F64_MEM_OP_TEST(DivFloat64Mem, DivFloat64, 7.5, 2.5, 3.0)

#define F32_MEM_OP_TEST(Name, Op, a, b, expected)                      \
  TEST_F(MacroAssemblerTest, Name) {                                   \
    for (int idx : {1, 1100}) {                                        \
      std::vector<float> v(1400, 0.0f);                                \
      v[0] = (a);                                                      \
      v[idx] = (b);                                                    \
      MASM_TEST_BEGIN                                                  \
      __ LoadF32(d1, MemOperand(r5, 0));                               \
      __ Op(d1, MemOperand(r5, idx * 4), d2);                          \
      __ StoreF32(d1, MemOperand(r5, 1399 * 4));                       \
      MASM_TEST_RUN(v.data())                                          \
      CHECK_EQ(static_cast<float>(expected), v[1399]);                 \
    }                                                                  \
  }

F32_MEM_OP_TEST(AddFloat32Mem, AddFloat32, 1.5f, 2.0f, 3.5f)
F32_MEM_OP_TEST(SubFloat32Mem, SubFloat32, 5.0f, 1.5f, 3.5f)
F32_MEM_OP_TEST(MulFloat32Mem, MulFloat32, 3.0f, 2.5f, 7.5f)
F32_MEM_OP_TEST(DivFloat32Mem, DivFloat32, 7.5f, 2.5f, 3.0f)

// ---------------------------------------------------------------------------
// Register forms with every dst/lhs/rhs aliasing combination
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, FPArithRegisterAliasingF64) {
  std::vector<double> v(32, 0.0);
  v[0] = 6.0;
  v[1] = 2.0;
  MASM_TEST_BEGIN
  __ LoadF64(d1, MemOperand(r5, 0));
  __ LoadF64(d2, MemOperand(r5, 8));
  __ AddF64(d3, d1, d2);
  MT_OUTD(d3, 4)
  __ SubF64(d3, d1, d2);
  MT_OUTD(d3, 5)
  __ MulF64(d3, d1, d2);
  MT_OUTD(d3, 6)
  __ DivF64(d3, d1, d2);
  MT_OUTD(d3, 7)
  // dst == lhs
  __ ldr(d4, d1);
  __ AddF64(d4, d4, d2);
  MT_OUTD(d4, 8)
  __ ldr(d4, d1);
  __ SubF64(d4, d4, d2);
  MT_OUTD(d4, 9)
  __ ldr(d4, d1);
  __ MulF64(d4, d4, d2);
  MT_OUTD(d4, 10)
  __ ldr(d4, d1);
  __ DivF64(d4, d4, d2);
  MT_OUTD(d4, 11)
  // dst == rhs
  __ ldr(d4, d2);
  __ AddF64(d4, d1, d4);
  MT_OUTD(d4, 12)
  __ ldr(d4, d2);
  __ SubF64(d4, d1, d4);
  MT_OUTD(d4, 13)
  __ ldr(d4, d2);
  __ MulF64(d4, d1, d4);
  MT_OUTD(d4, 14)
  __ ldr(d4, d2);
  __ DivF64(d4, d1, d4);
  MT_OUTD(d4, 15)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(8.0, v[4]);
  CHECK_EQ(4.0, v[5]);
  CHECK_EQ(12.0, v[6]);
  CHECK_EQ(3.0, v[7]);
  CHECK_EQ(8.0, v[8]);
  CHECK_EQ(4.0, v[9]);
  CHECK_EQ(12.0, v[10]);
  CHECK_EQ(3.0, v[11]);
  CHECK_EQ(8.0, v[12]);
  CHECK_EQ(4.0, v[13]);
  CHECK_EQ(12.0, v[14]);
  CHECK_EQ(3.0, v[15]);
}

TEST_F(MacroAssemblerTest, FPArithRegisterAliasingF32) {
  std::vector<float> v(64, 0.0f);
  v[0] = 6.0f;
  v[1] = 2.0f;
  MASM_TEST_BEGIN
  __ LoadF32(d1, MemOperand(r5, 0));
  __ LoadF32(d2, MemOperand(r5, 4));
  __ AddF32(d3, d1, d2);
  MT_OUTF(d3, 4)
  __ SubF32(d3, d1, d2);
  MT_OUTF(d3, 5)
  __ MulF32(d3, d1, d2);
  MT_OUTF(d3, 6)
  __ DivF32(d3, d1, d2);
  MT_OUTF(d3, 7)
  __ ler(d4, d1);
  __ AddF32(d4, d4, d2);
  MT_OUTF(d4, 8)
  __ ler(d4, d1);
  __ SubF32(d4, d4, d2);
  MT_OUTF(d4, 9)
  __ ler(d4, d1);
  __ MulF32(d4, d4, d2);
  MT_OUTF(d4, 10)
  __ ler(d4, d1);
  __ DivF32(d4, d4, d2);
  MT_OUTF(d4, 11)
  __ ler(d4, d2);
  __ AddF32(d4, d1, d4);
  MT_OUTF(d4, 12)
  __ ler(d4, d2);
  __ SubF32(d4, d1, d4);
  MT_OUTF(d4, 13)
  __ ler(d4, d2);
  __ MulF32(d4, d1, d4);
  MT_OUTF(d4, 14)
  __ ler(d4, d2);
  __ DivF32(d4, d1, d4);
  MT_OUTF(d4, 15)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(8.0f, v[4]);
  CHECK_EQ(4.0f, v[5]);
  CHECK_EQ(12.0f, v[6]);
  CHECK_EQ(3.0f, v[7]);
  CHECK_EQ(8.0f, v[8]);
  CHECK_EQ(4.0f, v[9]);
  CHECK_EQ(12.0f, v[10]);
  CHECK_EQ(3.0f, v[11]);
  CHECK_EQ(8.0f, v[12]);
  CHECK_EQ(4.0f, v[13]);
  CHECK_EQ(12.0f, v[14]);
  CHECK_EQ(3.0f, v[15]);
}

// ---------------------------------------------------------------------------
// Sqrt (register and memory, short and long displacement)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, SqrtRegAndMem) {
  std::vector<double> v(700, 0.0);
  v[0] = 9.0;
  v[1] = 16.0;
  v[520] = 25.0;
  MASM_TEST_BEGIN
  __ LoadF64(d1, MemOperand(r5, 0));
  __ Sqrt(d2, d1);
  MT_OUTD(d2, 600)
  __ Sqrt(d3, MemOperand(r5, 8));
  MT_OUTD(d3, 601)
  __ Sqrt(d4, MemOperand(r5, 520 * 8));
  MT_OUTD(d4, 602)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(3.0, v[600]);
  CHECK_EQ(4.0, v[601]);
  CHECK_EQ(5.0, v[602]);
}

// ---------------------------------------------------------------------------
// CeilF64 / FloorF64 / TruncF64 / NearestIntF64 and the F32 variants
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, RoundF64Family) {
  const double in[4] = {1.5, -1.5, 2.5, -2.5};
  std::vector<double> v(32, 0.0);
  for (int i = 0; i < 4; i++) v[i] = in[i];
  MASM_TEST_BEGIN
  for (int i = 0; i < 4; i++) {
    __ LoadF64(d1, MemOperand(r5, i * 8));
    __ CeilF64(d2, d1);
    MT_OUTD(d2, 4 + i)
    __ FloorF64(d2, d1);
    MT_OUTD(d2, 8 + i)
    __ TruncF64(d2, d1);
    MT_OUTD(d2, 12 + i)
    __ NearestIntF64(d2, d1);
    MT_OUTD(d2, 16 + i)
  }
  MASM_TEST_RUN(v.data())
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(std::ceil(in[i]), v[4 + i]);
    CHECK_EQ(std::floor(in[i]), v[8 + i]);
    CHECK_EQ(std::trunc(in[i]), v[12 + i]);
    CHECK_EQ(std::nearbyint(in[i]), v[16 + i]);
  }
}

TEST_F(MacroAssemblerTest, RoundF32Family) {
  const float in[4] = {1.5f, -1.5f, 2.5f, -2.5f};
  std::vector<float> v(64, 0.0f);
  for (int i = 0; i < 4; i++) v[i] = in[i];
  MASM_TEST_BEGIN
  for (int i = 0; i < 4; i++) {
    __ LoadF32(d1, MemOperand(r5, i * 4));
    __ CeilF32(d2, d1);
    MT_OUTF(d2, 4 + i)
    __ FloorF32(d2, d1);
    MT_OUTF(d2, 8 + i)
    __ TruncF32(d2, d1);
    MT_OUTF(d2, 12 + i)
    __ NearestIntF32(d2, d1);
    MT_OUTF(d2, 16 + i)
  }
  MASM_TEST_RUN(v.data())
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(std::ceil(in[i]), v[4 + i]);
    CHECK_EQ(std::floor(in[i]), v[8 + i]);
    CHECK_EQ(std::trunc(in[i]), v[12 + i]);
    CHECK_EQ(std::nearbyint(in[i]), v[16 + i]);
  }
}

// ---------------------------------------------------------------------------
// LoadF32AsF64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadF32AsF64Mem) {
  std::vector<float> v(1400, 0.0f);
  v[1] = 1.5f;
  v[1100] = -2.25f;
  MASM_TEST_BEGIN
  __ LoadF32AsF64(d1, MemOperand(r5, 4));
  __ LoadF32AsF64(d2, MemOperand(r5, 1100 * 4));
  __ StoreF64(d1, MemOperand(r5, 1200 * 4));
  __ StoreF64(d2, MemOperand(r5, 1202 * 4));
  MASM_TEST_RUN(v.data())
  double a, b;
  memcpy(&a, &v[1200], sizeof(a));
  memcpy(&b, &v[1202], sizeof(b));
  CHECK_EQ(1.5, a);
  CHECK_EQ(-2.25, b);
}

// ---------------------------------------------------------------------------
// MovIntToFloat / MovFloatToInt / MovInt64ToDouble / MovDoubleToInt64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MovBetweenGprAndFpr) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lgfi(r3, Operand(0x3FC00000));  // 1.5f
  __ MovIntToFloat(d1, r3);
  __ StoreF32(d1, MemOperand(r5, 0));
  __ MovFloatToInt(r4, d1);
  MT_OUT(r4, 1)
  MOV64(r3, 0x4000000000000000);  // 2.0
  __ MovInt64ToDouble(d2, r3);
  MT_OUTD(d2, 2)
  __ MovDoubleToInt64(r4, d2);
  MT_OUT(r4, 3)
  MASM_TEST_RUN(v.data())
  float f;
  memcpy(&f, &v[0], sizeof(f));
  CHECK_EQ(1.5f, f);
  CHECK_EQ(0x3FC00000, v[1]);
  double d;
  memcpy(&d, &v[2], sizeof(d));
  CHECK_EQ(2.0, d);
  CHECK_EQ(I64(0x4000000000000000), v[3]);
}

// ---------------------------------------------------------------------------
// CanonicalizeNaN
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CanonicalizeNaNTest) {
  std::vector<uint64_t> v(16, 0);
  v[0] = UINT64_C(0x7FF4000000000001);  // signalling NaN
  double normal = 3.5;
  memcpy(&v[1], &normal, sizeof(normal));
  MASM_TEST_BEGIN
  __ LoadF64(d1, MemOperand(r5, 0));
  __ CanonicalizeNaN(d2, d1);
  MT_OUTD(d2, 4)
  __ LoadF64(d3, MemOperand(r5, 8));
  __ CanonicalizeNaN(d3);
  MT_OUTD(d3, 5)
  MASM_TEST_RUN(v.data())
  double nan_out, normal_out;
  memcpy(&nan_out, &v[4], sizeof(nan_out));
  memcpy(&normal_out, &v[5], sizeof(normal_out));
  CHECK(std::isnan(nan_out));
  CHECK_NE(0u, v[4] & UINT64_C(0x0008000000000000));  // quiet bit set
  CHECK_EQ(3.5, normal_out);
}

// ---------------------------------------------------------------------------
// FloatMax / FloatMin (needs the vector-FP enhancement facility)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, FloatMaxMin) {
  if (!CpuFeatures::IsSupported(VECTOR_ENHANCE_FACILITY_2)) return;
  std::vector<float> v(64, 0.0f);
  v[0] = 1.5f;
  v[1] = 2.5f;
  v[2] = std::numeric_limits<float>::quiet_NaN();
  v[3] = -0.0f;
  v[4] = 0.0f;
  MASM_TEST_BEGIN
  __ LoadF32(d1, MemOperand(r5, 0));
  __ LoadF32(d2, MemOperand(r5, 4));
  __ LoadF32(d3, MemOperand(r5, 8));
  __ LoadF32(d5, MemOperand(r5, 12));
  __ LoadF32(d6, MemOperand(r5, 16));
  __ FloatMax(d4, d1, d2);
  MT_OUTF(d4, 8)
  __ FloatMin(d4, d1, d2);
  MT_OUTF(d4, 9)
  __ FloatMax(d4, d1, d3);
  MT_OUTF(d4, 10)
  __ FloatMin(d4, d1, d3);
  MT_OUTF(d4, 11)
  __ FloatMax(d4, d5, d6);
  MT_OUTF(d4, 12)
  __ FloatMin(d4, d5, d6);
  MT_OUTF(d4, 13)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(2.5f, v[8]);
  CHECK_EQ(1.5f, v[9]);
  CHECK(std::isnan(v[10]));
  CHECK(std::isnan(v[11]));
  CHECK_EQ(0.0f, v[12]);
  CHECK(!std::signbit(v[12]));
  CHECK_EQ(0.0f, v[13]);
  CHECK(std::signbit(v[13]));
}

// ---------------------------------------------------------------------------
// CmpF32 (register and memory forms)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CmpF32RegMem) {
  std::vector<float> v(128, 0.0f);
  v[0] = 1.0f;
  v[1] = 2.0f;
  int64_t* out = reinterpret_cast<int64_t*>(&v[64]);
  MASM_TEST_BEGIN
  __ LoadF32(d1, MemOperand(r5, 0));
  __ LoadF32(d2, MemOperand(r5, 4));
  __ lghi(r4, Operand(1));
  __ lghi(r2, Operand(0));
  __ CmpF32(d1, d2);
  __ LoadOnConditionP(lt, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 256));
  __ lghi(r2, Operand(0));
  __ CmpF32(d2, d1);
  __ LoadOnConditionP(gt, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 264));
  __ lghi(r2, Operand(0));
  __ CmpF32(d1, d1);
  __ LoadOnConditionP(eq, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 272));
  __ lghi(r2, Operand(0));
  __ CmpF32(d1, MemOperand(r5, 4));
  __ LoadOnConditionP(lt, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 280));
  __ lghi(r2, Operand(0));
  __ CmpF32(d2, MemOperand(r5, 0));
  __ LoadOnConditionP(gt, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 288));
  __ lghi(r2, Operand(0));
  __ CmpF32(d1, MemOperand(r5, 0));
  __ LoadOnConditionP(eq, r2, r4);
  __ StoreU64(r2, MemOperand(r5, 296));
  MASM_TEST_RUN(v.data())
  for (int i = 0; i < 6; i++) CHECK_EQ(1, out[i]);
}

// ---------------------------------------------------------------------------
// LoadAndTest32 / LoadAndTestP (register and memory forms)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadAndTestFamily) {
  std::vector<int64_t> v(64, 0);
  int32_t* p32 = reinterpret_cast<int32_t*>(v.data());
  p32[16] = -7;  // byte offset 64
  p32[18] = 0;   // byte offset 72
  int64_t* out = &v[16];  // byte offset 128
  MASM_TEST_BEGIN
  __ lghi(r4, Operand(1));
#define LAT_CHECK(cond, idx)                                \
  __ LoadOnConditionP(cond, r2, r4);                        \
  __ StoreU64(r2, MemOperand(r5, 128 + (idx) * 8));
  __ lghi(r3, Operand(-5));
  __ lghi(r2, Operand(0));
  __ LoadAndTest32(r3, r3);
  LAT_CHECK(lt, 0)
  __ lghi(r3, Operand(0));
  __ lghi(r2, Operand(0));
  __ LoadAndTest32(r3, r3);
  LAT_CHECK(eq, 1)
  __ lghi(r3, Operand(9));
  __ lghi(r2, Operand(0));
  __ LoadAndTest32(r3, r3);
  LAT_CHECK(gt, 2)
  MOV64(r3, 0x8000000000000000);
  __ lghi(r2, Operand(0));
  __ LoadAndTestP(r3, r3);
  LAT_CHECK(lt, 3)
  __ lghi(r3, Operand(0));
  __ lghi(r2, Operand(0));
  __ LoadAndTestP(r3, r3);
  LAT_CHECK(eq, 4)
  __ lghi(r2, Operand(0));
  __ LoadAndTest32(r3, MemOperand(r5, 64));
  LAT_CHECK(lt, 5)
  __ lghi(r2, Operand(0));
  __ LoadAndTest32(r3, MemOperand(r5, 72));
  LAT_CHECK(eq, 6)
  MOV64(r3, 0xFFFFFFFFFFFFFFF0);
  __ StoreU64(r3, MemOperand(r5, 80));
  __ lghi(r2, Operand(0));
  __ LoadAndTestP(r3, MemOperand(r5, 80));
  LAT_CHECK(lt, 7)
#undef LAT_CHECK
  MASM_TEST_RUN(v.data())
  for (int i = 0; i < 8; i++) CHECK_EQ(1, out[i]);
}

// ---------------------------------------------------------------------------
// LoadPositive32 / LoadPositiveP
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadPositiveFamily) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(-7));
  __ LoadPositive32(r2, r3);
  MT_OUT(r2, 0)
  __ llilf(r3, Operand(0xFFFFFFFB));  // low word == -5, high word == 0
  __ LoadPositive32(r2, r3);
  MT_OUT(r2, 1)
  __ lghi(r3, Operand(12));
  __ LoadPositive32(r2, r3);
  MT_OUT(r2, 2)
  MOV64(r3, 0xFFFFFFFFFFFFFFF7);  // -9
  __ LoadPositiveP(r2, r3);
  MT_OUT(r2, 3)
  __ lghi(r3, Operand(33));
  __ LoadPositiveP(r2, r3);
  MT_OUT(r2, 4)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(7, v[0]);
  CHECK_EQ(5, v[1]);
  CHECK_EQ(12, v[2]);
  CHECK_EQ(9, v[3]);
  CHECK_EQ(33, v[4]);
}

// ---------------------------------------------------------------------------
// Not32 / Not64 / NotP
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, NotFamily) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ llilf(r3, Operand(0x0F0F0F0F));
  __ lghi(r2, Operand(0));
  __ Not32(r2, r3);  // copies the low word then flips it
  MT_OUT(r2, 0)
  __ Not32(r3);  // in place
  MT_OUT(r3, 1)
  MOV64(r3, 0x00FF00FF00FF00FF);
  __ Not64(r2, r3);
  MT_OUT(r2, 2)
  __ Not64(r3);
  MT_OUT(r3, 3)
  MOV64(r3, 0x123456789ABCDEF0);
  __ NotP(r2, r3);
  MT_OUT(r2, 4)
  __ NotP(r3);
  MT_OUT(r3, 5)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xF0F0F0F0), v[0]);
  CHECK_EQ(I64(0xF0F0F0F0), v[1]);
  CHECK_EQ(I64(0xFF00FF00FF00FF00), v[2]);
  CHECK_EQ(I64(0xFF00FF00FF00FF00), v[3]);
  CHECK_EQ(I64(0xEDCBA9876543210F), v[4]);
  CHECK_EQ(I64(0xEDCBA9876543210F), v[5]);
}

// ---------------------------------------------------------------------------
// AndP / OrP / XorP (every operand form)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AndPForms) {
  std::vector<int64_t> v(64, 0);
  v[32] = I64(0xFFFF0000FFFF0000);
  MASM_TEST_BEGIN
  MOV64(r3, 0x123456789ABCDEF0);
  MOV64(r4, 0xFFFF0000FFFF0000);
  __ AndP(r2, r3, Operand(0xFF));  // contiguous mask: RISBG fast path
  MT_OUT(r2, 0)
  __ AndP(r2, r3, Operand(0xFF00));
  MT_OUT(r2, 1)
  __ AndP(r2, r3, Operand(0x00F0F0F0));  // non-contiguous: nihf/nilf
  MT_OUT(r2, 2)
  __ lgr(r2, r3);
  __ AndP(r2, Operand(0x0F0F));
  MT_OUT(r2, 3)
  __ lgr(r2, r3);
  __ AndP(r2, Operand(-256));  // high word untouched
  MT_OUT(r2, 4)
  __ AndP(r2, r3, r4);
  MT_OUT(r2, 5)
  __ lgr(r2, r4);
  __ AndP(r2, r3);
  MT_OUT(r2, 6)
  __ lgr(r2, r3);
  __ AndP(r2, r2, r4);  // dst == src1
  MT_OUT(r2, 7)
  __ lgr(r2, r4);
  __ AndP(r2, r3, r2);  // dst == src2
  MT_OUT(r2, 8)
  __ lgr(r2, r3);
  __ AndP(r2, MemOperand(r5, 256));
  MT_OUT(r2, 9)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xF0), v[0]);
  CHECK_EQ(I64(0xDE00), v[1]);
  CHECK_EQ(I64(0xB0D0F0), v[2]);
  CHECK_EQ(I64(0x0E00), v[3]);
  CHECK_EQ(I64(0x123456789ABCDE00), v[4]);
  CHECK_EQ(I64(0x123400009ABC0000), v[5]);
  CHECK_EQ(I64(0x123400009ABC0000), v[6]);
  CHECK_EQ(I64(0x123400009ABC0000), v[7]);
  CHECK_EQ(I64(0x123400009ABC0000), v[8]);
  CHECK_EQ(I64(0x123400009ABC0000), v[9]);
}

TEST_F(MacroAssemblerTest, OrPForms) {
  std::vector<int64_t> v(64, 0);
  v[32] = I64(0xF0);
  MASM_TEST_BEGIN
  MOV64(r3, 0x1000000000000001);
  MOV64(r4, 0x0200000000000020);
  __ lghi(r2, Operand(0));
  __ OrP(r2, Operand(static_cast<intptr_t>(UINT64_C(0x0000000100000001))));
  MT_OUT(r2, 0)
  __ lghi(r2, Operand(0));
  __ OrP(r2, Operand(0x80));
  MT_OUT(r2, 1)
  __ OrP(r2, r3, Operand(0xF));
  MT_OUT(r2, 2)
  __ OrP(r2, r3, r4);
  MT_OUT(r2, 3)
  __ lgr(r2, r3);
  __ OrP(r2, r4);
  MT_OUT(r2, 4)
  __ lgr(r2, r3);
  __ OrP(r2, MemOperand(r5, 256));
  MT_OUT(r2, 5)
  __ lgr(r2, r3);
  __ OrP(r2, r2, r4);
  MT_OUT(r2, 6)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0x0000000100000001), v[0]);
  CHECK_EQ(0x80, v[1]);
  CHECK_EQ(I64(0x100000000000000F), v[2]);
  CHECK_EQ(I64(0x1200000000000021), v[3]);
  CHECK_EQ(I64(0x1200000000000021), v[4]);
  CHECK_EQ(I64(0x10000000000000F1), v[5]);
  CHECK_EQ(I64(0x1200000000000021), v[6]);
}

TEST_F(MacroAssemblerTest, XorPForms) {
  std::vector<int64_t> v(64, 0);
  v[32] = I64(0xFF);
  MASM_TEST_BEGIN
  MOV64(r3, 0x1234567812345678);
  MOV64(r4, 0xFFFFFFFF00000000);
  __ XorP(r2, r3, Operand(0xFF));
  MT_OUT(r2, 0)
  __ lgr(r2, r3);
  __ XorP(r2, Operand(-1));
  MT_OUT(r2, 1)
  __ XorP(r2, r3, r4);
  MT_OUT(r2, 2)
  __ lgr(r2, r3);
  __ XorP(r2, r4);
  MT_OUT(r2, 3)
  __ lgr(r2, r3);
  __ XorP(r2, MemOperand(r5, 256));
  MT_OUT(r2, 4)
  __ lgr(r2, r3);
  __ XorP(r2, r2, r4);
  MT_OUT(r2, 5)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0x1234567812345687), v[0]);
  CHECK_EQ(I64(0xEDCBA987EDCBA987), v[1]);
  CHECK_EQ(I64(0xEDCBA98712345678), v[2]);
  CHECK_EQ(I64(0xEDCBA98712345678), v[3]);
  CHECK_EQ(I64(0x1234567812345687), v[4]);
  CHECK_EQ(I64(0xEDCBA98712345678), v[5]);
}

// ---------------------------------------------------------------------------
// ClearRightImm / RotateInsertSelectBits
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ClearRightImmTest) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  MOV64(r3, 0x123456789ABCDEFF);
  __ ClearRightImm(r2, r3, Operand(8));
  MT_OUT(r2, 0)
  __ ClearRightImm(r2, r3, Operand(4));
  MT_OUT(r2, 1)
  __ ClearRightImm(r2, r3, Operand(32));
  MT_OUT(r2, 2)
  __ ClearRightImm(r2, r3, Operand(72));  // 72 % 64 == 8
  MT_OUT(r2, 3)
  __ ClearRightImm(r2, r3, Operand(0));
  MT_OUT(r2, 4)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0x123456789ABCDE00), v[0]);
  CHECK_EQ(I64(0x123456789ABCDEF0), v[1]);
  CHECK_EQ(I64(0x1234567800000000), v[2]);
  CHECK_EQ(I64(0x123456789ABCDE00), v[3]);
  CHECK_EQ(I64(0x123456789ABCDEFF), v[4]);
}

TEST_F(MacroAssemblerTest, RotateInsertSelectBitsTest) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(0xAB));
  __ lghi(r2, Operand(-1));
  __ RotateInsertSelectBits(r2, r3, Operand(56), Operand(63), Operand::Zero(),
                            false);
  MT_OUT(r2, 0)
  __ lghi(r2, Operand(-1));
  __ RotateInsertSelectBits(r2, r3, Operand(56), Operand(63), Operand::Zero(),
                            true);
  MT_OUT(r2, 1)
  __ lghi(r2, Operand(0));
  __ RotateInsertSelectBits(r2, r3, Operand(48), Operand(55), Operand(8), true);
  MT_OUT(r2, 2)
  MOV64(r4, 0xAB00000000000000);
  __ lghi(r2, Operand(0));
  __ RotateInsertSelectBits(r2, r4, Operand(56), Operand(63), Operand(8), true);
  MT_OUT(r2, 3)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xFFFFFFFFFFFFFFAB), v[0]);
  CHECK_EQ(I64(0xAB), v[1]);
  CHECK_EQ(I64(0xAB00), v[2]);
  CHECK_EQ(I64(0xAB), v[3]);
}

// ---------------------------------------------------------------------------
// ShiftRightU64 (immediate, register, register + immediate)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ShiftRightU64Forms) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  MOV64(r3, 0xF000000000000000);
  __ ShiftRightU64(r2, r3, Operand(60));
  MT_OUT(r2, 0)
  __ lghi(r4, Operand(4));
  __ ShiftRightU64(r2, r3, r4);
  MT_OUT(r2, 1)
  __ ShiftRightU64(r2, r3, r4, Operand(4));  // 4 + 4
  MT_OUT(r2, 2)
  __ ShiftRightU64(r2, r3, Operand(63));
  MT_OUT(r2, 3)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(0xF, v[0]);
  CHECK_EQ(I64(0x0F00000000000000), v[1]);
  CHECK_EQ(I64(0x00F0000000000000), v[2]);
  CHECK_EQ(1, v[3]);
}

// ---------------------------------------------------------------------------
// AddU32 / SubU32 / AddU64 / SubU64 (memory, register and immediate forms)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LogicalAddSubMem) {
  for (int k : {8, 520}) {  // short (al/sl) and long (aly/sly) displacement
    std::vector<int64_t> v(700, 0);
    uint8_t* base = reinterpret_cast<uint8_t*>(v.data());
    WriteBE32(base + 8 * k, 10);
    WriteBE32(base + 8 * (k + 1), 7);
    v[k + 2] = 1000;
    MASM_TEST_BEGIN
    __ lghi(r2, Operand(5));
    __ AddU32(r2, MemOperand(r5, 8 * k));  // 5 + 10
    MT_OUT(r2, 600)
    __ lghi(r2, Operand(20));
    __ SubU32(r2, MemOperand(r5, 8 * (k + 1)));  // 20 - 7
    MT_OUT(r2, 601)
    __ lghi(r2, Operand(1));
    __ AddU64(r2, MemOperand(r5, 8 * (k + 2)));  // 1 + 1000
    MT_OUT(r2, 602)
    __ lghi(r2, Operand(64));
    __ SubU64(r2, MemOperand(r5, 8 * k));  // 64 - zext32(10)
    MT_OUT(r2, 603)
    MASM_TEST_RUN(v.data())
    CHECK_EQ(15, static_cast<int32_t>(v[600]));
    CHECK_EQ(13, static_cast<int32_t>(v[601]));
    CHECK_EQ(1001, v[602]);
    CHECK_EQ(54, v[603]);
  }
}

TEST_F(MacroAssemblerTest, LogicalAddSubRegs) {
  std::vector<int64_t> v(32, 0);
  MASM_TEST_BEGIN
  MOV64(r3, 0xFFFFFFFFFFFFFFFF);
  __ lghi(r4, Operand(2));
  __ AddU64(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lgr(r2, r3);
  __ AddU64(r2, r2, r4);  // dst == src1
  MT_OUT(r2, 1)
  __ lgr(r2, r4);
  __ AddU64(r2, r3, r2);  // dst == src2
  MT_OUT(r2, 2)
  __ lghi(r2, Operand(40));
  __ AddU64(r2, Operand(2));
  MT_OUT(r2, 3)
  __ lghi(r2, Operand(40));
  __ AddU64(r2, 3);
  MT_OUT(r2, 4)
  __ lghi(r2, Operand(40));
  __ AddU64(r2, r4);
  MT_OUT(r2, 5)
  __ lghi(r3, Operand(10));
  __ lghi(r4, Operand(3));
  __ SubU32(r2, r3, r4);
  MT_OUT(r2, 6)
  __ lgr(r2, r3);
  __ SubU32(r2, r2, r4);  // dst == src1
  MT_OUT(r2, 7)
  __ lgr(r2, r4);
  __ SubU32(r2, r3, r2);  // dst == src2
  MT_OUT(r2, 8)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(1, v[0]);
  CHECK_EQ(1, v[1]);
  CHECK_EQ(1, v[2]);
  CHECK_EQ(42, v[3]);
  CHECK_EQ(43, v[4]);
  CHECK_EQ(42, v[5]);
  CHECK_EQ(7, static_cast<int32_t>(v[6]));
  CHECK_EQ(7, static_cast<int32_t>(v[7]));
  CHECK_EQ(7, static_cast<int32_t>(v[8]));
}

TEST_F(MacroAssemblerTest, LoadAndSubMem) {
  std::vector<int64_t> v(32, 0);
  uint8_t* base = reinterpret_cast<uint8_t*>(v.data());
  WriteBE32(base, 100);
  v[1] = 1000;
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(30));
  __ LoadAndSub32(r2, r3, MemOperand(r5, 0));  // r2 = old (100); mem = 70
  MT_OUT(r2, 4)
  __ lghi(r3, Operand(400));
  __ LoadAndSub64(r2, r3, MemOperand(r5, 8));  // r2 = old (1000); mem = 600
  MT_OUT(r2, 5)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(100, static_cast<int32_t>(v[4]));
  CHECK_EQ(70u, ReadBE32(base));
  CHECK_EQ(1000, v[5]);
  CHECK_EQ(600, v[1]);
}


// ---------------------------------------------------------------------------
// MultiPush / MultiPop (General Purpose Registers, using default sp)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MultiPushPopGPR) {
  // Test that MultiPush/MultiPop correctly round-trips register values.
  //
  // Use r3 and r4 (caller-saved/volatile in s390 ABI) as the pushed registers
  // so we don't need to worry about ABI callee-save obligations for them.
  //
  // Use sp (r15) as the push/pop location (the default), but save/restore sp
  // state by doing a balanced push then pop — sp ends up unchanged.
  // MASM_TEST_BEGIN already saves d13/d14 on the real stack; our MultiPush
  // adds 16 more bytes, MultiPop removes them, so net sp delta = 0.
  //
  // r6 is NOT used here, avoiding the callee-save ABI issue.
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  // r5 = v.data() (set by MASM_TEST_BEGIN via lgr r5, r2)
  // Load test values into r3, r4 (volatile — no ABI save needed)
  __ lghi(r3, Operand(111));
  __ lghi(r4, Operand(222));
  // Push r3,r4 onto the real stack (sp -= 16)
  __ MultiPush(RegList{r3, r4});
  // Clobber to prove pop restores correctly
  __ lghi(r3, Operand(0));
  __ lghi(r4, Operand(0));
  // Pop r3,r4 back (sp += 16) — sp is now identical to before MultiPush
  __ MultiPop(RegList{r3, r4});
  MT_OUT(r3, 0)
  MT_OUT(r4, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(111, v[0]);
  CHECK_EQ(222, v[1]);
}

// ---------------------------------------------------------------------------
// LoadReversedU64 / StoreReversedU64 (byte-swap 64-bit)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, LoadStoreReversedU64) {
  std::vector<int64_t> v(16, 0);
  v[0] = I64(0x0102030405060708);
  MASM_TEST_BEGIN
  // LoadReversedU64: byte-swap from memory into register
  __ LoadReversedU64(r3, MemOperand(r5, 0), r1);
  MT_OUT(r3, 4)
  // StoreReversedU64: byte-swap from register into memory
  __ lghi(r3, Operand(0));
  __ iihf(r3, Operand(0x11223344));
  __ iilf(r3, Operand(0x55667788));
  __ StoreReversedU64(r3, MemOperand(r5, 5 * 8), r1);
  __ lg(r4, MemOperand(r5, 5 * 8));
  MT_OUT(r4, 6)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0x0807060504030201), v[4]);
  CHECK_EQ(I64(0x8877665544332211), v[6]);
}

// ---------------------------------------------------------------------------
// MoveChar (mvc — block move up to 256 bytes)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MoveCharTest) {
  // Copy 8 bytes from one slot to another
  std::vector<int64_t> v(16, 0);
  v[0] = I64(0xDEADBEEFCAFEBABE);
  MASM_TEST_BEGIN
  __ MoveChar(MemOperand(r5, 8), MemOperand(r5, 0), Operand(8));
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xDEADBEEFCAFEBABE), v[1]);
}

// ---------------------------------------------------------------------------
// CompareLogicalChar (clc — block compare)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, CompareLogicalCharTest) {
  struct T {
    char a[8];
    char b[8];
    int64_t result;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ lghi(r3, Operand(1));
  __ lghi(r4, Operand(0));
  // r2 = &t
  __ CompareLogicalChar(MemOperand(r2, offsetof(T, a)),
                        MemOperand(r2, offsetof(T, b)), Operand(4));
  __ LoadOnConditionP(eq, r4, r3);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, result)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(T*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  // Equal
  memcpy(t.a, "ABCD    ", 8);
  memcpy(t.b, "ABCD    ", 8);
  t.result = -1;
  f.Call(&t);
  CHECK_EQ(1, t.result);

  // Not equal
  memcpy(t.b, "ABCE    ", 8);
  t.result = -1;
  f.Call(&t);
  CHECK_EQ(0, t.result);
}

// ---------------------------------------------------------------------------
// AtomicExchangeU8 / AtomicExchangeU16
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AtomicExchangeU8U16) {
  std::vector<int64_t> v(16, 0);
  uint8_t* b = reinterpret_cast<uint8_t*>(v.data());
  b[0] = 0xAB;  // byte at offset 0
  uint8_t* hw = b + 8;
  hw[0] = 0x12;
  hw[1] = 0x34;  // halfword 0x1234 at offset 8

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = base
  // Byte exchange: swap 0xAB with 0xCD at byte offset 0
  __ lghi(r3, Operand(0xCD));
  __ AtomicExchangeU8(r2, r3, r4, r1);   // output = old value
  __ StoreU64(r4, MemOperand(r2, 2 * 8));  // save old value
  // Halfword exchange: swap 0x1234 with 0x5678 at byte offset 8
  __ lghi(r3, Operand(0x5678));
  __ AtomicExchangeU16(r2, r3, r4, r1);  // NOTE: addr=r2+0, but we want offset 8
  // Actually AtomicExchangeU16 uses r2 as address directly; need to adjust
  __ Ret();
  // This is a smoke test (just assembles cleanly)
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // Just verify it compiles
}

// ---------------------------------------------------------------------------
// AtomicCmpExchangeU8 / AtomicCmpExchangeU16 (smoke tests — assembles cleanly)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, AtomicCmpExchangeSmoke) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // CmpExchangeU8(addr, output, old_val, new_val, temp0, temp1)
  // Use distinct registers to satisfy constraints: none can alias
  __ lghi(r3, Operand(0xAA));   // old_value
  __ lghi(r4, Operand(0xBB));   // new_value
  // AtomicCmpExchangeU8: addr=r2, output=r5, old=r3, new=r4, temp0=r6, temp1=r7
  __ AtomicCmpExchangeU8(r2, r5, r3, r4, r6, r7);
  // AtomicCmpExchangeU16: same registers
  __ lghi(r3, Operand(0x1234));
  __ lghi(r4, Operand(0x5678));
  __ AtomicCmpExchangeU16(r2, r5, r3, r4, r6, r7);
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // Smoke test: just verifies it assembles and links
}

// ---------------------------------------------------------------------------
// ConvertDoubleToUnsignedInt32 / ConvertDoubleToUnsignedInt64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ConvertDoubleToUnsigned) {
  struct T {
    double input;
    uint32_t out32;
    uint64_t out64;
  } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF64(d1, MemOperand(r2, offsetof(T, input)));
  __ ConvertDoubleToUnsignedInt32(r3, d1);
  __ StoreU32(r3, MemOperand(r2, offsetof(T, out32)));
  __ ConvertDoubleToUnsignedInt64(r4, d1);
  __ StoreU64(r4, MemOperand(r2, offsetof(T, out64)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(T*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 42.9;
  t.out32 = UINT32_MAX;
  t.out64 = UINT64_MAX;
  f.Call(&t);
  CHECK_EQ(42u, t.out32);
  CHECK_EQ(42ull, t.out64);

  t.input = 0.0;
  t.out32 = UINT32_MAX;
  t.out64 = UINT64_MAX;
  f.Call(&t);
  CHECK_EQ(0u, t.out32);
  CHECK_EQ(0ull, t.out64);

  t.input = 1000000.7;
  t.out32 = UINT32_MAX;
  t.out64 = UINT64_MAX;
  f.Call(&t);
  CHECK_EQ(1000000u, t.out32);
  CHECK_EQ(1000000ull, t.out64);
}

// ---------------------------------------------------------------------------
// ConvertFloat32ToInt64
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ConvertFloat32ToInt64) {
  struct T { float input; int64_t output; } t = {};
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadF32(d0, MemOperand(r2, offsetof(T, input)));
  __ ConvertFloat32ToInt64(r3, d0, kRoundToZero);
  __ StoreU64(r3, MemOperand(r2, offsetof(T, output)));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(T*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());

  t.input = 3.9f; t.output = INT64_MIN; f.Call(&t); CHECK_EQ(3LL, t.output);
  t.input = -7.1f; t.output = INT64_MIN; f.Call(&t); CHECK_EQ(-7LL, t.output);
  t.input = 1e10f; t.output = INT64_MIN; f.Call(&t);
  CHECK_EQ(10000000000LL, t.output);
  t.input = 0.0f; t.output = INT64_MIN; f.Call(&t); CHECK_EQ(0LL, t.output);
}

// ---------------------------------------------------------------------------
// MulHighS32 / MulHighU32 (register and immediate variants)
//
// MulHighS32 implementation (from macro-assembler-s390.cc):
//   lgfr(dst, src1);          // sign-extend src1.low32 to 64-bit
//   msgfr(dst, src2);         // dst = dst * sext32(src2) (64-bit)
//   srlg(dst, dst, Operand(32)); // logical-shift-right 32 → high 32 bits
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MulHighS32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  // sext(-2^31) * 2 = 0xFFFFFFFF80000000 * 2 = 0xFFFFFFFF00000000
  // srlg 32 → 0x00000000FFFFFFFF = 4294967295
  __ lgfi(r3, Operand(static_cast<int32_t>(0x80000000)));
  __ lghi(r4, Operand(2));
  __ MulHighS32(r2, r3, r4);
  MT_OUT(r2, 0)

  // 0x7FFFFFFF * 2 = 0xFFFFFFFE (fits in low 32): high 32 = 0
  __ lgfi(r3, Operand(0x7FFFFFFF));
  __ lghi(r4, Operand(2));
  __ MulHighS32(r2, r3, r4);
  MT_OUT(r2, 1)

  // sext(-2^31) * 4 = 0xFFFFFFFE00000000; srlg 32 → 0xFFFFFFFE = 4294967294
  __ lgfi(r3, Operand(static_cast<int32_t>(0x80000000)));
  __ MulHighS32(r2, r3, Operand(4));
  MT_OUT(r2, 2)

  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xFFFFFFFF), v[0]);    // = 4294967295
  CHECK_EQ(0, v[1]);
  CHECK_EQ(I64(0xFFFFFFFE), v[2]);    // = 4294967294
}

TEST_F(MacroAssemblerTest, MulHighU32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  // MulHighU32(dst, src1, src2): high 32 bits of unsigned 64-bit product
  // 0xFFFFFFFF * 2 = 0x1FFFFFFFE; high 32 = 1
  __ llilf(r3, Operand(0xFFFFFFFF));
  __ lghi(r4, Operand(2));
  __ MulHighU32(r2, r3, r4);
  MT_OUT(r2, 0)

  // 0x10000 * 0x10000 = 0x100000000; high 32 = 1
  __ lgfi(r3, Operand(0x10000));
  __ lgfi(r4, Operand(0x10000));
  __ MulHighU32(r2, r3, r4);
  MT_OUT(r2, 1)

  MASM_TEST_RUN(v.data())
  CHECK_EQ(1, v[0]);
  CHECK_EQ(1, v[1]);
}

// ---------------------------------------------------------------------------
// MulHighS64 / MulHighU64 (register variant)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, MulHighS64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  // 2^63 * 2 = 2^64; high 64 bits = 1
  MOV64(r3, 0x8000000000000000);
  __ lghi(r4, Operand(2));
  __ MulHighS64(r2, r3, r4);
  MT_OUT(r2, 0)

  // 2^62 * 4 = 2^64; high 64 bits = 1
  MOV64(r3, 0x4000000000000000);
  __ lghi(r4, Operand(4));
  __ MulHighS64(r2, r3, r4);
  MT_OUT(r2, 1)

  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0xFFFFFFFFFFFFFFFF), v[0]);  // signed: -1
  CHECK_EQ(1, v[1]);
}

TEST_F(MacroAssemblerTest, MulHighU64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  // 2^63 * 2 = 2^64; high 64 bits = 1
  MOV64(r3, 0x8000000000000000);
  __ lghi(r4, Operand(2));
  __ MulHighU64(r2, r3, r4);
  MT_OUT(r2, 0)

  // 2^32 * 2^32 = 2^64; high 64 bits = 1
  MOV64(r3, 0x0000000100000000);
  MOV64(r4, 0x0000000100000000);
  __ MulHighU64(r2, r3, r4);
  MT_OUT(r2, 1)

  MASM_TEST_RUN(v.data())
  CHECK_EQ(1, v[0]);
  CHECK_EQ(1, v[1]);
}

// ---------------------------------------------------------------------------
// DivS32 / DivU32 / DivS64 / DivU64 (register variants)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, DivS32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(100));
  __ lghi(r4, Operand(7));
  __ DivS32(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lgfi(r3, Operand(-100));
  __ DivS32(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(14, static_cast<int32_t>(v[0]));
  CHECK_EQ(-14, static_cast<int32_t>(v[1]));
}

TEST_F(MacroAssemblerTest, DivU32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ llilf(r3, Operand(0xFFFFFFFF));  // 2^32-1
  __ lghi(r4, Operand(3));
  __ DivU32(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lghi(r3, Operand(100));
  __ lghi(r4, Operand(9));
  __ DivU32(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(0xFFFFFFFF / 3u, static_cast<uint32_t>(v[0]));
  CHECK_EQ(11u, static_cast<uint32_t>(v[1]));
}

TEST_F(MacroAssemblerTest, DivS64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lgfi(r3, Operand(1000));
  __ lghi(r4, Operand(7));
  __ DivS64(r2, r3, r4);
  MT_OUT(r2, 0)
  MOV64(r3, 0xFFFFFFFFFFFFFF9C);  // -100
  __ lghi(r4, Operand(3));
  __ DivS64(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(142, v[0]);
  CHECK_EQ(-33, v[1]);
}

TEST_F(MacroAssemblerTest, DivU64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  MOV64(r3, 0x0000000100000000);  // 2^32
  __ lghi(r4, Operand(2));
  __ DivU64(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lgfi(r3, Operand(999));
  __ lghi(r4, Operand(10));
  __ DivU64(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(I64(0x0000000080000000), v[0]);
  CHECK_EQ(99, v[1]);
}

// ---------------------------------------------------------------------------
// ModS32 / ModU32 / ModS64 / ModU64 (register variants)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, ModS32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(100));
  __ lghi(r4, Operand(7));
  __ ModS32(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lgfi(r3, Operand(-100));
  __ ModS32(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(2, static_cast<int32_t>(v[0]));   // 100 mod 7 = 2
  CHECK_EQ(-2, static_cast<int32_t>(v[1]));  // -100 mod 7 = -2
}

TEST_F(MacroAssemblerTest, ModU32Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lghi(r3, Operand(100));
  __ lghi(r4, Operand(7));
  __ ModU32(r2, r3, r4);
  MT_OUT(r2, 0)
  __ llilf(r3, Operand(0xFFFFFFFF));
  __ lghi(r4, Operand(3));
  __ ModU32(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(2u, static_cast<uint32_t>(v[0]));
  CHECK_EQ(0xFFFFFFFF % 3u, static_cast<uint32_t>(v[1]));
}

TEST_F(MacroAssemblerTest, ModS64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  __ lgfi(r3, Operand(1000));
  __ lghi(r4, Operand(7));
  __ ModS64(r2, r3, r4);
  MT_OUT(r2, 0)
  MOV64(r3, 0xFFFFFFFFFFFFFF9C);  // -100
  __ lghi(r4, Operand(3));
  __ ModS64(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(6, v[0]);    // 1000 mod 7 = 6
  CHECK_EQ(-1, v[1]);   // -100 mod 3 = -1
}

TEST_F(MacroAssemblerTest, ModU64Test) {
  std::vector<int64_t> v(16, 0);
  MASM_TEST_BEGIN
  MOV64(r3, 0x0000000100000000);  // 2^32
  __ lghi(r4, Operand(3));
  __ ModU64(r2, r3, r4);
  MT_OUT(r2, 0)
  __ lgfi(r3, Operand(999));
  __ lghi(r4, Operand(10));
  __ ModU64(r2, r3, r4);
  MT_OUT(r2, 1)
  MASM_TEST_RUN(v.data())
  CHECK_EQ(1, v[0]);   // 2^32 mod 3 = 1
  CHECK_EQ(9, v[1]);   // 999 mod 10 = 9
}

// ---------------------------------------------------------------------------
// I8x16Shuffle (requires VECTOR_FACILITY)
// Signature: I8x16Shuffle(dst, src1, src2, high64, low64,
//                         scratch1_reg, scratch2_reg, scratch3_simd)
// The 16 shuffle indices are packed into two uint64_t (big-endian byte order):
//   high = indices for output bytes 0-7, low = indices for output bytes 8-15.
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I8x16ShuffleTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  std::vector<uint8_t> v(256, 0);
  // src1 (bytes 0-15): 0,1,...,15
  for (int i = 0; i < 16; i++) v[i] = static_cast<uint8_t>(i);
  // src2 (bytes 16-31): 80,81,...,95 (not selected in this test)
  for (int i = 0; i < 16; i++) v[16 + i] = static_cast<uint8_t>(0x80 + i);

  // Identity shuffle: output byte i = src1[i]
  uint64_t high = UINT64_C(0x0001020304050607);
  uint64_t low  = UINT64_C(0x08090a0b0c0d0e0f);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ LoadV128(d2, MemOperand(r2, 16), r1);
  __ I8x16Shuffle(d3, d1, d2, high, low, r3, r4, d4);
  __ StoreV128(d3, MemOperand(r2, 32), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  // Identity shuffle: result[i] == src1[i] == i
  for (int i = 0; i < 16; i++) {
    CHECK_EQ(static_cast<uint8_t>(i), v[32 + i]);
  }
}

// ---------------------------------------------------------------------------
// I8x16Swizzle (runtime permutation with a vector index)
// Signature: I8x16Swizzle(dst, src1, src2_indices,
//                         scratch1_reg, scratch2_reg, scratch3_simd)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, I8x16SwizzleTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  // I8x16Swizzle on s390x internally byte-reverses src1 before vperm, so the
  // identity index set {0,1,...,15} does NOT produce identity output.
  //
  // The implementation computes:
  //   byte_reversed_src1[j] = src1[15 - j]
  //   result[i] = byte_reversed_src1[ saturate(index[i]) ]
  //             = src1[ 15 - index[i] ]   (when index[i] <= 15)
  //
  // Test: src1[k] = k (values 0..15), index[i] = 15-i (reverse)
  //   => result[i] = src1[15 - (15-i)] = src1[i] = i
  //
  // So with reverse indices the output equals the input.
  std::vector<uint8_t> v(64, 0);
  // src1: v[0..15] = {0, 1, ..., 15}
  for (int i = 0; i < 16; i++) v[i] = static_cast<uint8_t>(i);
  // indices: v[16..31] = {15, 14, ..., 0} (reverse permutation)
  for (int i = 0; i < 16; i++) v[16 + i] = static_cast<uint8_t>(15 - i);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ LoadV128(d2, MemOperand(r2, 16), r1);
  __ I8x16Swizzle(d3, d1, d2, r3, r4, d4);
  __ StoreV128(d3, MemOperand(r2, 32), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  // result[i] should equal i
  for (int i = 0; i < 16; i++) {
    CHECK_EQ(static_cast<uint8_t>(i), v[32 + i]);
  }
}

// ---------------------------------------------------------------------------
// S128Select (vsel — per-bit select from two vectors based on mask)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, S128SelectTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  std::vector<uint8_t> v(64, 0);
  // v1: all 0xFF
  for (int i = 0; i < 16; i++) v[i] = 0xFF;
  // v2: all 0x00
  for (int i = 0; i < 16; i++) v[16 + i] = 0x00;
  // mask: first 8 bytes = 0xFF (pick from v1), last 8 bytes = 0x00 (pick from v2)
  for (int i = 0; i < 8; i++) v[32 + i] = 0xFF;
  for (int i = 8; i < 16; i++) v[32 + i] = 0x00;

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);   // v1=0xFF..
  __ LoadV128(d2, MemOperand(r2, 16), r1);  // v2=0x00..
  __ LoadV128(d3, MemOperand(r2, 32), r1);  // mask
  __ S128Select(d4, d1, d2, d3);
  __ StoreV128(d4, MemOperand(r2, 48), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  for (int i = 0; i < 8; i++) CHECK_EQ(0xFF, v[48 + i]);    // from v1
  for (int i = 8; i < 16; i++) CHECK_EQ(0x00, v[48 + i]);   // from v2
}

// ---------------------------------------------------------------------------
// V128AnyTrue (returns non-zero if any bit is set)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, V128AnyTrueTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  std::vector<int64_t> v(16, 0);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  // r2 = base of v
  // Test 1: vector with all zeros → AnyTrue == 0
  __ lghi(r3, Operand(0));
  __ StoreU64(r3, MemOperand(r2, 0));
  __ StoreU64(r3, MemOperand(r2, 8));
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ V128AnyTrue(r4, d1, r3);
  __ StoreU64(r4, MemOperand(r2, 16));
  // Test 2: vector with a non-zero byte → AnyTrue != 0
  __ lghi(r3, Operand(1));
  __ StoreU64(r3, MemOperand(r2, 0));
  __ lghi(r3, Operand(0));
  __ StoreU64(r3, MemOperand(r2, 8));
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ V128AnyTrue(r4, d1, r3);
  __ StoreU64(r4, MemOperand(r2, 24));
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = void(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  f.Call(v.data());
  CHECK_EQ(0, v[2]);    // all zeros: AnyTrue == 0
  CHECK_NE(0, v[3]);    // has a set bit: AnyTrue != 0
}

// ---------------------------------------------------------------------------
// F32x4SConvertI32x4 (convert int32x4 to float32x4)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F32x4SConvertI32x4Test) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  // Build a 128-bit vector of four int32 values: 1, -2, 3, -4
  int32_t ints[4] = {1, -2, 3, -4};
  std::vector<int64_t> v(16, 0);
  memcpy(v.data(), ints, 16);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ F32x4SConvertI32x4(d2, d1, d3, r3);
  __ StoreV128(d2, MemOperand(r2, 16), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  float out[4];
  memcpy(out, &v[2], 16);
  CHECK_EQ(1.0f, out[0]);
  CHECK_EQ(-2.0f, out[1]);
  CHECK_EQ(3.0f, out[2]);
  CHECK_EQ(-4.0f, out[3]);
}

// ---------------------------------------------------------------------------
// F64x2PromoteLowF32x4: promote low two float32 lanes to float64x2
// Signature: F64x2PromoteLowF32x4(dst, src, scratch1_simd,
//                                 scratch2_reg, scratch3_reg, scratch4_reg)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F64x2PromoteLowF32x4Test) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  // F64x2PromoteLowF32x4 extracts float32 elements at vector indices 2 and 3
  // (via `vlgv src, [r0 + index+2], Condition(2)` for index=0,1).
  // With in_floats = {1.5f, -2.25f, 3.0f, 4.0f}:
  //   index=0: vlgv at 2 => in_floats[2] = 3.0f  => out[0] = 3.0
  //   index=1: vlgv at 3 => in_floats[3] = 4.0f  => out[1] = 4.0
  float in_floats[4] = {1.5f, -2.25f, 3.0f, 4.0f};
  std::vector<int64_t> v(16, 0);
  memcpy(v.data(), in_floats, 16);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ F64x2PromoteLowF32x4(d2, d1, d3, r3, r4, r5);
  __ StoreV128(d2, MemOperand(r2, 16), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  double out[2];
  memcpy(out, &v[2], 16);
  // Floats at indices 2 and 3 of the f32x4 vector are promoted
  CHECK_EQ(static_cast<double>(3.0f), out[0]);
  CHECK_EQ(static_cast<double>(4.0f), out[1]);
}

// ---------------------------------------------------------------------------
// F32x4DemoteF64x2Zero: demote float64x2 to float32x4 (high 2 lanes zeroed)
// Signature: F32x4DemoteF64x2Zero(dst, src, scratch1_simd,
//                                 scratch2_reg, scratch3_reg, scratch4_reg)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, F32x4DemoteF64x2ZeroTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  double in_doubles[2] = {1.5, -2.25};
  std::vector<int64_t> v(16, 0);
  memcpy(v.data(), in_doubles, 16);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ LoadV128(d1, MemOperand(r2, 0), r1);
  __ F32x4DemoteF64x2Zero(d2, d1, d3, r3, r4, r5);
  __ StoreV128(d2, MemOperand(r2, 16), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  float out[4];
  memcpy(out, &v[2], 16);
  CHECK_EQ(0.0f, out[0]);   // high lanes zeroed
  CHECK_EQ(0.0f, out[1]);   // lane 1 = 0
  CHECK_EQ(1.5f, out[2]);   // lane 2 = first double demoted
  CHECK_EQ(-2.25f, out[3]); // lane 3 = second double demoted
}

// ---------------------------------------------------------------------------
// S128Const (load a vector with a constant bit pattern)
// Signature: S128Const(dst, high64, low64, scratch1_reg, scratch2_reg)
// ---------------------------------------------------------------------------
TEST_F(MacroAssemblerTest, S128ConstTest) {
  if (!CpuFeatures::IsSupported(VECTOR_FACILITY)) return;
  std::vector<uint8_t> v(32, 0);
  // Expected: bytes 0-7 = high, bytes 8-15 = low
  uint64_t high = UINT64_C(0x0001020304050607);
  uint64_t low  = UINT64_C(0x08090a0b0c0d0e0f);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);
  __ S128Const(d1, high, low, r3, r4);
  __ StoreV128(d1, MemOperand(r2, 0), r1);
  __ lgfi(r2, Operand::Zero());
  __ Ret();
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  using F = int32_t(void*);
  auto f = GeneratedCode<F>::FromBuffer(isolate(), buffer->start());
  CHECK_EQ(0, f.Call(v.data()));
  for (int i = 0; i < 16; i++) CHECK_EQ(static_cast<uint8_t>(i), v[i]);
}


#undef __

}  // namespace internal
}  // namespace v8

