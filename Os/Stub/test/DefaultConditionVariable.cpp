// ======================================================================
// \title Os/Stub/test/DefaultConditionVariable.cpp
// \brief sets default Os::ConditionVariable to test stub implementation via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "Os/Stub/test/ConditionVariable.hpp"

namespace Os {

//! \brief get a delegate for ConditionVariableInterface for stub test usage
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
ConditionVariableInterface* ConditionVariableInterface::getDelegate(
    ConditionVariableHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<ConditionVariableInterface,
                                      Os::Stub::ConditionVariable::Test::TestConditionVariable,
                                      ConditionVariableHandleStorage>(aligned_new_memory);
}
}  // namespace Os
