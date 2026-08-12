// ======================================================================
// \title Os/Stub/DefaultConditionVariable.cpp
// \brief sets default Os::ConditionVariable to no-op stub implementation via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "Os/Stub/ConditionVariable.hpp"

namespace Os {

//! \brief get a delegate for condition variable
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
ConditionVariableInterface* ConditionVariableInterface::getDelegate(
    ConditionVariableHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<ConditionVariableInterface, Os::Stub::Mutex::StubConditionVariable,
                                      ConditionVariableHandleStorage>(aligned_new_memory);
}
}  // namespace Os
