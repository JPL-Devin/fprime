// ======================================================================
// \title Os/Posix/DefaultConditionVariable.cpp
// \brief sets default Os::ConditionVariable Posix implementation via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "Os/Posix/ConditionVariable.hpp"

namespace Os {

//! \brief get a delegate for ConditionVariableInterface that intercepts calls for Posix
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
ConditionVariableInterface* ConditionVariableInterface::getDelegate(
    ConditionVariableHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<ConditionVariableInterface, Os::Posix::Mutex::PosixConditionVariable,
                                      ConditionVariableHandleStorage>(aligned_new_memory);
}
}  // namespace Os
