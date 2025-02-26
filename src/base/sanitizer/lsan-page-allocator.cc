// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/sanitizer/lsan-page-allocator.h"

#include "include/v8-platform.h"
#include "src/base/logging.h"

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

namespace v8 {
namespace base {

LsanPageAllocator::LsanPageAllocator(v8::PageAllocator* page_allocator)
    : page_allocator_(page_allocator),
      allocate_page_size_(page_allocator_->AllocatePageSize()),
      commit_page_size_(page_allocator_->CommitPageSize()) {
  DCHECK_NOT_NULL(page_allocator);
}

void* LsanPageAllocator::AllocatePages(void* hint, size_t size,
                                       size_t alignment,
#if defined(CHERI_HYBRID)
                                       PageAllocator::Permission access,
                                       PageAllocator::Permission max_access) {
  void* result = page_allocator_->AllocatePages(hint, size, alignment, access,
		                                max_access);
#else
                                       PageAllocator::Permission access) {
  void* result = page_allocator_->AllocatePages(hint, size, alignment, access);
#endif // CHERI_HYBRID
#if defined(LEAK_SANITIZER)
  if (result != nullptr) {
    if (access != PageAllocator::Permission::kNoAccessWillJitLater) {
      __lsan_register_root_region(result, size);
    } else {
      // We allocate the JIT cage as RWX from the beginning und use Discard to
      // mark the memory as unused. This makes tests with LSAN enabled 2-3x
      // slower since it will always try to scan the area for pointers. So skip
      // registering the JIT regions with LSAN.
      base::MutexGuard lock(&not_registered_regions_mutex_);
      not_registered_regions_.insert(result);
    }
  }
#endif
  return result;
}

std::unique_ptr<v8::PageAllocator::SharedMemory>
LsanPageAllocator::AllocateSharedPages(size_t size,
                                       const void* original_address) {
  auto result = page_allocator_->AllocateSharedPages(size, original_address);
#if defined(LEAK_SANITIZER)
  if (result != nullptr) {
    __lsan_register_root_region(result->GetMemory(), size);
  }
#endif
  return result;
}

bool LsanPageAllocator::CanAllocateSharedPages() {
  return page_allocator_->CanAllocateSharedPages();
}

bool LsanPageAllocator::FreePages(void* address, size_t size) {
  CHECK(page_allocator_->FreePages(address, size));
#if defined(LEAK_SANITIZER)
  base::MutexGuard lock(&not_registered_regions_mutex_);
  if (not_registered_regions_.count(address) == 0) {
    __lsan_unregister_root_region(address, size);
  } else {
    not_registered_regions_.erase(address);
  }
#endif
  return true;
}

bool LsanPageAllocator::ReleasePages(void* address, size_t size,
                                     size_t new_size) {
  CHECK(page_allocator_->ReleasePages(address, size, new_size));
#if defined(LEAK_SANITIZER)
  base::MutexGuard lock(&not_registered_regions_mutex_);
  if (not_registered_regions_.count(address) == 0) {
    __lsan_unregister_root_region(address, size);
    __lsan_register_root_region(address, new_size);
  }
#endif
  return true;
}

}  // namespace base
}  // namespace v8
