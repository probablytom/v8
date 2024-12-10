// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/pointer-authentication.h"

namespace v8 {
namespace internal {

#ifdef CHERI_HYBRID
const int Deoptimizer::kCheriCompartmentExitSize = 8 * kInstrSize;
#endif

#ifdef CHERI_HYBRID
// const int Deoptimizer::kEagerDeoptExitSize = kInstrSize + kCheriCompartmentExitSize;
const int Deoptimizer::kEagerDeoptExitSize = kInstrSize;
#else
const int Deoptimizer::kEagerDeoptExitSize = kInstrSize;
#endif


#ifdef CHERI_HYBRID

// #ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
// const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize + kCheriCompartmentExitSize;
// #else
// const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize + kCheriCompartmentExitSize;
// #endif // V8_ENABLE_CONTROL_FLOW_INTEGRITY
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize;
#else
const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize;
#endif // V8_ENABLE_CONTROL_FLOW_INTEGRITY

#else // CHERI_HYBRID

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize;
#else
const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize;
#endif // V8_ENABLE_CONTROL_FLOW_INTEGRITY

#endif // CHERI_HYBRID

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  V8_ASSUME(n < arraysize(simd128_registers_));
  return base::ReadUnalignedValue<Float32>(
      reinterpret_cast<Address>(simd128_registers_ + n));
}

Float64 RegisterValues::GetDoubleRegister(unsigned n) const {
  V8_ASSUME(n < arraysize(simd128_registers_));
  return base::ReadUnalignedValue<Float64>(
      reinterpret_cast<Address>(simd128_registers_ + n));
}

void RegisterValues::SetDoubleRegister(unsigned n, Float64 value) {
  V8_ASSUME(n < arraysize(simd128_registers_));
  base::WriteUnalignedValue(reinterpret_cast<Address>(simd128_registers_ + n),
                            value);
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  Address new_context =
      static_cast<Address>(GetTop()) + offset + kPCOnStackSize;
  value = PointerAuthentication::SignAndCheckPC(isolate_, value, new_context);
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}

void FrameDescription::SetPc(intptr_t pc, bool skip_validity_check) {
  // TODO(v8:10026): We need to sign pointers to the embedded blob, which are
  // stored in the isolate and code range objects.
  if (ENABLE_CONTROL_FLOW_INTEGRITY_BOOL && !skip_validity_check) {
    CHECK(Deoptimizer::IsValidReturnAddress(PointerAuthentication::StripPAC(pc),
                                            isolate_));
  }
  pc_ = pc;
}

}  // namespace internal
}  // namespace v8
