// ======================================================================
// \title Os/DelegateConditionVariable.hpp
// \brief Define the Os::DelegateConditionVariable class
// ======================================================================
#ifndef OS_DELEGATECONDITIONVARIABLE_HPP_
#define OS_DELEGATECONDITIONVARIABLE_HPP_

#include "Os/ConditionVariableInterface.hpp"

namespace Os {

//! \brief condition variable implementation selected at link time
//!
//! Condition variables allow a program to block on a condition while atomically releasing an Os::Mutex and atomically
//! reacquiring the mutex once the condition has been notified.
class DelegateConditionVariable final : public ConditionVariableInterface {
  public:
    //! \brief default constructor
    DelegateConditionVariable();

    //! \brief default virtual destructor
    ~DelegateConditionVariable() final;

    //! \brief copy constructor is forbidden
    DelegateConditionVariable(const ConditionVariableInterface& other) = delete;

    //! \brief copy constructor is forbidden
    DelegateConditionVariable(const ConditionVariableInterface* other) = delete;

    //! \brief assignment operator is forbidden
    ConditionVariableInterface& operator=(const ConditionVariableInterface& other) override = delete;

    //! \brief wait on a condition variable
    //!
    //! Atomically unlocks the supplied mutex and blocks until a future `notify` or `notifyAll` call. Delegates to the
    //! underlying implementation.
    //!
    //! \warning it is invalid to supply a mutex different from those supplied by others
    //! \warning conditions *must* be rechecked after the condition variable unlocks
    //! \warning the mutex must be locked by the calling task
    //!
    //! \param mutex: mutex to unlock as part of this operation
    //! \return status of the conditional wait
    Status pend(Os::Mutex& mutex) override;

    //! \brief wait on a condition variable, asserting on failure
    //!
    //! \param mutex: mutex to unlock as part of this operation
    void wait(Os::Mutex& mutex) override;

    //! \brief notify a single waiter on this condition variable. Delegates to implementation.
    void notify() override;

    //! \brief notify all waiters on this condition variable. Delegates to implementation.
    void notifyAll() override;

    //! \brief return the underlying condition variable handle (implementation specific). Delegates to implementation.
    //! \return internal task handle representation
    ConditionVariableHandle* getHandle() override;

  private:
    //! Pointer to mutex object previously used
    Os::Mutex* m_lock = nullptr;

    // This section is used to store the implementation-defined handle. To fprime, this type is opaque and thus normal
    // allocation cannot be done. Instead, the implementor stores the handle in the byte-array here.
    alignas(FW_HANDLE_ALIGNMENT) ConditionVariableHandleStorage m_handle_storage;  //!< Storage for aligned handle data
    ConditionVariableInterface& m_delegate;  //!< Delegate for the real implementation
};
}  // namespace Os

#endif  // OS_DELEGATECONDITIONVARIABLE_HPP_
