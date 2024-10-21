// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_ARM64_BASELINE_COMPILER_ARM64_INL_H_
#define V8_BASELINE_ARM64_BASELINE_COMPILER_ARM64_INL_H_

#include "src/baseline/baseline-compiler.h"

namespace v8 {
namespace internal {
namespace baseline {

#define __ basm_.

// A builtin call/jump mode that is used then short builtin calls feature is
// not enabled.
constexpr BuiltinCallJumpMode kFallbackBuiltinCallJumpModeForBaseline =
    BuiltinCallJumpMode::kIndirect;

void BaselineCompiler::Prologue() {
  ASM_CODE_COMMENT(&masm_);
  // Enter the frame here, since CallBuiltin will override lr.
  __ masm()->EnterFrame(StackFrame::BASELINE, MacroAssembler::builtinCall);
  DCHECK_EQ(kJSFunctionRegister, kJavaScriptCallTargetRegister);
  int max_frame_size =
      bytecode_->frame_size() + max_call_args_ * kSystemPointerSize;
  CallBuiltin<Builtin::kBaselineOutOfLinePrologue>(
      kContextRegister, kJSFunctionRegister, kJavaScriptCallArgCountRegister,
      max_frame_size, kJavaScriptCallNewTargetRegister, bytecode_);

  __ masm()->AssertSpAligned();
  PrologueFillFrame();
  __ masm()->AssertSpAligned();
}

void BaselineCompiler::PrologueFillFrame() {
  ASM_CODE_COMMENT(&masm_);
  // Inlined register frame fill
  interpreter::Register new_target_or_generator_register =
      bytecode_->incoming_new_target_or_generator_register();
  if (v8_flags.debug_code) {
    __ masm()->CompareRoot(kInterpreterAccumulatorRegister,
                           RootIndex::kUndefinedValue);
    __ masm()->Assert(eq, AbortReason::kUnexpectedValue);
  }
  int register_count = bytecode_->register_count();
  // Magic value
  const int kLoopUnrollSize = 8;
  const int new_target_index = new_target_or_generator_register.index();
  const bool has_new_target = new_target_index != kMaxInt;
  if (has_new_target) {
      DCHECK_LE(new_target_index, register_count);
      int before_new_target_count = 0;
      for (; before_new_target_count + 2 <= new_target_index;
           before_new_target_count += 2) {
        __ masm()->Push(kInterpreterAccumulatorRegister,
                        kInterpreterAccumulatorRegister);
      }
      if (before_new_target_count == new_target_index) {
        __ masm()->Push(kJavaScriptCallNewTargetRegister,
                        kInterpreterAccumulatorRegister);
      } else {
        DCHECK_EQ(before_new_target_count + 1, new_target_index);
        __ masm()->Push(kInterpreterAccumulatorRegister,
                        kJavaScriptCallNewTargetRegister);
      }
      // We pushed before_new_target_count registers, plus the two registers
      // that included new_target.
      register_count -= (before_new_target_count + 2);
  }
  if (register_count < 2 * kLoopUnrollSize) {
    // If the frame is small enough, just unroll the frame fill completely.
    for (int i = 0; i < register_count; i += 2) {
      __ masm()->Push(kInterpreterAccumulatorRegister,
                      kInterpreterAccumulatorRegister);
    }
  } else {
    BaselineAssembler::ScratchRegisterScope temps(&basm_);
    Register scratch = temps.AcquireScratch();

    // Extract the first few registers to round to the unroll size.
    int first_registers = register_count % kLoopUnrollSize;
    for (int i = 0; i < first_registers; i += 2) {
      __ masm()->Push(kInterpreterAccumulatorRegister,
                      kInterpreterAccumulatorRegister);
    }
    __ Move(scratch, register_count / kLoopUnrollSize);
    // We enter the loop unconditionally, so make sure we need to loop at least
    // once.
    DCHECK_GT(register_count / kLoopUnrollSize, 0);
    Label loop;
    __ Bind(&loop);
    for (int i = 0; i < kLoopUnrollSize; i += 2) {
      __ masm()->Push(kInterpreterAccumulatorRegister,
                      kInterpreterAccumulatorRegister);
    }
    __ masm()->Subs(scratch, scratch, 1);
    __ masm()->B(gt, &loop);
  }
}

void BaselineCompiler::VerifyFrameSize() {
  ASM_CODE_COMMENT(&masm_);
  __ masm()->Add(x15, sp,
                 RoundUp(InterpreterFrameConstants::kFixedFrameSizeFromFp +
                             bytecode_->frame_size(),
                         2 * kSystemPointerSize));

  // // TODELETE check for which invocation of VerifyFrameSize gives us the issue we're seeing...it's not the first one!
  // Label EQ;
  // Label NEQ;
  // Label ENDCONDITIONAL;
  // __ masm()->Cmp(x15, fp);
  // __ masm()->B(eq, &EQ);
  // // NEQ case --- trap so we can inspect.
  // __ masm()->Bind(&NEQ);
  // __ masm()->Brk(0xf016);
  // // EQ case --- add 1 to CID so we know how many invocations of VerifyFrameSize we see before the discrepancy occurs.
  // __ masm()->Bind(&EQ);
  // __ masm()->Push(x14, x13);
  // __ masm()->Mrs(x13.C(), CID);
  // __ masm()->Add(x13, x13, 1);
  // __ masm()->Msr(CID, x13.C());
  // __ masm()->Pop(x13, x14);
  // // end of conditional for debugging
  // __ masm()->Bind(&ENDCONDITIONAL);
  // // END TODELETE

  __ masm()->Cmp(x15, fp);
  __ masm()->Assert(eq, AbortReason::kUnexpectedStackPointer);
}

#undef __

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_ARM64_BASELINE_COMPILER_ARM64_INL_H_
