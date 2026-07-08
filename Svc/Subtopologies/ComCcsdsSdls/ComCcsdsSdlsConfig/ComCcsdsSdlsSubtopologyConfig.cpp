#include "ComCcsdsSdlsSubtopologyConfig.hpp"

namespace ComCcsdsSdls {
namespace Allocation {
// This instance can be changed to use a different allocator in the ComCcsdsSdls Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsdsSdls
