// ======================================================================
// \title config/OsDelegates.hpp
// \brief configured selection of the concrete implementation behind each Os OSAL alias
//
// This header configures how each aliased Os service (Os::RawTime, Os::Mutex, ...)
// resolves to a concrete implementation. Two mechanisms are available per service:
//
// 1. Link-time selection (default): the alias names the link-time delegate
//    (e.g. Os::Mutex = Os::DelegateMutex), which wraps an interface reference.
//    At construction, the delegate calls <Service>Interface::getDelegate() to
//    construct the platform-specific implementation via placement-new. The linker
//    selects which getDelegate() based on which Default<Service>.cpp is linked.
//    Calls dispatch through the vtable at runtime.
//
// 2. Compile-time selection (performance optimization): the alias names a
//    concrete implementation directly. This eliminates the wrapper and virtual
//    dispatch, enabling inlining and aggressive LTO optimization.
//
// Projects override this file as a whole (place a copy in the project config
// directory and register it in that directory's CMakeLists.txt), changing only
// the services they want to select at compile time and leaving the others at
// their link-time default. Example, selecting only Mutex at compile time:
//
//     namespace MyPlatform { class MyMutex; }
//     namespace Os {
//     class DelegateRawTime;
//     using RawTime = DelegateRawTime;   // unchanged: link-time default
//     using Mutex = MyPlatform::MyMutex;  // compile-time selection
//     }  // namespace Os
//     #define OS_RAW_TIME_HEADER <Os/DelegateRawTime.hpp>
//     #define OS_MUTEX_HEADER "MyPlatform/Os/MyMutex.hpp"
//
// Each OS_<SERVICE>_HEADER macro names the header defining the aliased type. It is
// included by Os/<Service>.hpp AFTER Os/<Service>Interface.hpp, and the aliased type
// MUST derive from Os::<Service>Interface.
//
// IMPORTANT: CIRCULAR DEPENDENCY PREVENTION
//
//   - This header MUST NOT include any Os OSAL headers (Os/*.hpp).
//   - Only forward-declare types and define the Os::<Service> aliases.
//   - Violating this constraint will create circular dependencies.
// ======================================================================
#ifndef CONFIG_OS_DELEGATES_HPP
#define CONFIG_OS_DELEGATES_HPP

namespace Os {

// ---- RawTime ---------------------------------------------------------
class DelegateRawTime;
using RawTime = DelegateRawTime;

// ---- Mutex -----------------------------------------------------------
// WARNING: if the build links an Os_Mutex implementation module that also
// provides Os::ConditionVariable, that ConditionVariable must understand the
// aliased type's MutexHandle (e.g. Os/Posix/ConditionVariable.cpp casts the
// handle to PosixMutexHandle). Mixing a compile-time Mutex with a mismatched
// ConditionVariable implementation is undefined behavior with no build-time error.
class DelegateMutex;
using Mutex = DelegateMutex;

}  // namespace Os

#define OS_RAW_TIME_HEADER <Os/DelegateRawTime.hpp>
#define OS_MUTEX_HEADER <Os/DelegateMutex.hpp>

#endif  // CONFIG_OS_DELEGATES_HPP
