// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using CapabilityHandlesTest = TestWithIsolate;

// MARK CHERI CHANGE there should be similar tests checking that
// CapabilityHandles are _not_ equal to Handles if the CapabilityHandle points
// to something for which it does not have legal access
// e.g. it refers to something outwith its own bounds or is otherwise invalid
TEST_F(CapabilityHandlesTest, CreateCapabilityHandleFromLocal) {
  HandleScope scope(isolate());
  Local<String> foo = String::NewFromUtf8Literal(isolate(), "foo");

  i::CapabilityHandle<i::String> capability = Utils::OpenCapabilityHandle(*foo);
  i::Handle<i::String> handle = Utils::OpenHandle(*foo);

  EXPECT_EQ(*capability, *handle);
}

TEST_F(CapabilityHandlesTest, CreateLocalFromCapabilityHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::CapabilityHandle<i::String> capability = handle; // TODO what's going on here? Maybe I'm too tired right now...

  Local<String> l1 = Utils::ToLocal(capability, i_isolate());
  Local<String> l2 = Utils::ToLocal(handle);

  EXPECT_EQ(l1, l2);
}

TEST_F(CapabilityHandlesTest, CreateMaybeCapabilityHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::CapabilityHandle<i::String> capability = handle;

  i::MaybeCapabilityHandle<i::String> maybe_capability(capability);
  i::MaybeHandle<i::String> maybe_handle(handle);

  EXPECT_EQ(*maybe_capability.ToHandleChecked(), *maybe_handle.ToHandleChecked());
}

TEST_F(CapabilityHandlesTest, CreateMaybeCapabilityObjectHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::CapabilityHandle<i::String> capability = handle;

  i::MaybeObjectCapabilityHandle maybe_capability(capability);
  i::MaybeObjectHandle maybe_handle(handle);

  EXPECT_EQ(*maybe_capability, *maybe_handle);
}

TEST_F(CapabilityHandlesTest, IsIdenticalTo) {
  i::CapabilityHandle<i::String> d1 =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::CapabilityHandle<i::String> d2(d1);

  i::CapabilityHandle<i::String> d3 =
      i_isolate()->factory()->NewStringFromAsciiChecked("bar");
  i::CapabilityHandle<i::String> d4;
  i::CapabilityHandle<i::String> d5;

  EXPECT_TRUE(d1.is_identical_to(d2));
  EXPECT_TRUE(d2.is_identical_to(d1));
  EXPECT_FALSE(d1.is_identical_to(d3));
  EXPECT_FALSE(d1.is_identical_to(d4));
  EXPECT_FALSE(d4.is_identical_to(d1));
  EXPECT_TRUE(d4.is_identical_to(d5));
}

TEST_F(CapabilityHandlesTest, MaybeObjectCapabilityHandleIsIdenticalTo) {
  i::CapabilityHandle<i::String> foo =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::CapabilityHandle<i::String> bar =
      i_isolate()->factory()->NewStringFromAsciiChecked("bar");

  i::MaybeObjectCapabilityHandle d1(foo);
  i::MaybeObjectCapabilityHandle d2(foo);
  i::MaybeObjectCapabilityHandle d3(bar);
  i::MaybeObjectCapabilityHandle d4;
  i::MaybeObjectCapabilityHandle d5;

  EXPECT_TRUE(d1.is_identical_to(d2));
  EXPECT_TRUE(d2.is_identical_to(d1));
  EXPECT_FALSE(d1.is_identical_to(d3));
  EXPECT_FALSE(d1.is_identical_to(d4));
  EXPECT_FALSE(d4.is_identical_to(d1));
  EXPECT_TRUE(d4.is_identical_to(d5));
}

// Tests to check CapabilityHandle usage. Such usage violations are only
// detected in debug builds, with the compile-time flag for enabling capability
// handles (V8_ENABLE_CAPABILITY_HANDLE).

#if defined(DEBUG) && defined(V8_ENABLE_CAPABILITY_HANDLE)

namespace {
template <typename Callback>
void CheckCapabilityHandleUsage(Callback callback) {
  EXPECT_DEATH_IF_SUPPORTED(callback(), "");
}
}  // anonymous namespace

TEST_F(CapabilityHandlesTest, CapabilityHandleOutOfStackFails) {
  // Out-of-stack allocation of capability handles should fail.
  CheckCapabilityHandleUsage([]() {
    auto ptr = std::make_unique<i::CapabilityHandle<i::String>>();
    USE(ptr);
  });
}

namespace {
class BackgroundThread final : public v8::base::Thread {
 public:
  explicit BackgroundThread(i::Heap* heap)
      : v8::base::Thread(base::Thread::Options("BackgroundThread")),
        heap_(heap) {}

  void Run() override {
    i::LocalHeap lh(heap_, i::ThreadKind::kBackground);
    // Usage of capability handles in background threads should fail.
    CheckCapabilityHandleUsage([]() {
      i::CapabilityHandle<i::String> capability;
      USE(capability);
    });
  }

  i::Heap* heap_;
};
}  // anonymous namespace

TEST_F(CapabilityHandlesTest, CapabilityHandleInBackgroundThreadFails) {
  i::Heap* heap = i_isolate()->heap();
  i::LocalHeap lh(heap, i::ThreadKind::kMain);
  lh.SetUpMainThreadForTesting();
  auto thread = std::make_unique<BackgroundThread>(heap);
  CHECK(thread->Start());
  thread->Join();
}

#if V8_CAN_CREATE_SHARED_HEAP_BOOL

using CapabilityHandlesSharedTest = i::TestJSSharedMemoryWithIsolate;

namespace {
class ClientThread final : public i::ParkingThread {
 public:
  ClientThread() : ParkingThread(base::Thread::Options("ClientThread")) {}

  void Run() override {
    IsolateWrapper isolate_wrapper(kNoCounters);
    // Capability handles can be used in the main thread of client isolates.
    i::CapabilityHandle<i::String> capability;
    USE(capability);
  }
};
}  // anonymous namespace

TEST_F(CapabilityHandlesSharedTest, CapabilityHandleInClient) {
  auto thread = std::make_unique<ClientThread>();
  CHECK(thread->Start());
  thread->ParkedJoin(i_isolate()->main_thread_local_isolate());
}

namespace {
class ClientMainThread final : public i::ParkingThread {
 public:
  ClientMainThread()
      : ParkingThread(base::Thread::Options("ClientMainThread")) {}

  void Run() override {
    IsolateWrapper isolate_wrapper(kNoCounters);
    Isolate* client_isolate = isolate_wrapper.isolate();
    i::Isolate* i_client_isolate =
        reinterpret_cast<i::Isolate*>(client_isolate);

    i::Heap* heap = i_client_isolate->heap();
    i::LocalHeap lh(heap, i::ThreadKind::kMain);
    lh.SetUpMainThreadForTesting();
    auto thread = std::make_unique<BackgroundThread>(heap);
    CHECK(thread->Start());
    thread->Join();
  }
};
}  // anonymous namespace

TEST_F(CapabilityHandlesSharedTest, CapabilityHandleInClientBackgroundThreadFails) {
  auto thread = std::make_unique<ClientMainThread>();
  CHECK(thread->Start());
  thread->ParkedJoin(i_isolate()->main_thread_local_isolate());
}

#endif  // V8_CAN_CREATE_SHARED_HEAP_BOOL
#endif  // DEBUG && V8_ENABLE_CAPABILITY_HANDLE

}  // namespace v8

