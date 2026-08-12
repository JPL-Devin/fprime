// ======================================================================
// \title Os/ConditionVariableInterface.hpp
// \brief Os::ConditionVariableHandle and Os::ConditionVariableInterface definitions
// ======================================================================
#ifndef OS_CONDITIONVARIABLEINTERFACE_HPP_
#define OS_CONDITIONVARIABLEINTERFACE_HPP_

#include "Os/Mutex.hpp"
#include "Os/Os.hpp"
#include "config/OsDelegateConditionVariable.hpp"

namespace Os {

//! \brief Condition variable handle parent
class ConditionVariableHandle {};

//! \brief interface for condition variables
//!
//! Condition variables allow a program to block on a condition while atomically releasing an Os::Mutex and atomically
//! reacquiring the mutex once the condition has been notified.
class ConditionVariableInterface {
  public:
    enum Status {
        OP_OK,                  //!<  Operation was successful
        ERROR_MUTEX_NOT_HELD,   //!< When trying to wait but we don't hold the mutex
        ERROR_DIFFERENT_MUTEX,  //!< When trying to use a different mutex than expected mutex
        ERROR_NOT_IMPLEMENTED,  //!< When trying to use a feature that isn't implemented
        NOT_SUPPORTED,          //!< ConditionVariable does not support operation
        ERROR_OTHER             //!< All other errors
    };

    //! Default constructor
    ConditionVariableInterface() = default;
    //! Default destructor
    virtual ~ConditionVariableInterface() = default;

    //! \brief copy constructor is forbidden
    ConditionVariableInterface(const ConditionVariableInterface& other) = delete;

    //! \brief assignment operator is forbidden
    virtual ConditionVariableInterface& operator=(const ConditionVariableInterface& other) = delete;

    //! \brief wait on a condition variable
    //!
    //! Wait on a condition variable. This function will atomically unlock the provided mutex and block on the condition
    //! in one step. Blocking will occur until a future `notify` or `notifyAll` call is made to this variable on another
    //! thread of execution.
    //!
    //! \param mutex: mutex to unlock as part of this operation
    //! \return status of the conditional wait
    virtual Status pend(Os::Mutex& mutex) = 0;

    //! \brief wait on a condition variable, asserting on failure
    //!
    //! Calls `pend` and asserts that the operation succeeded.
    //!
    //! \param mutex: mutex to unlock as part of this operation
    virtual void wait(Os::Mutex& mutex);

    //! \brief notify a single waiter on this condition variable
    //!
    //! Notify a single waiter on this condition variable. It is not necessary to hold the mutex supplied by the waiters
    //! and it is advantageous not to hold the lock to prevent immediate re-blocking.
    virtual void notify() = 0;

    //! \brief notify all waiters on this condition variable
    //!
    //! Notify all waiters on this condition variable. It is not necessary to hold the mutex supplied by the waiters
    //! and it is advantageous not to hold the lock to prevent immediate re-blocking.
    virtual void notifyAll() = 0;

    //! \brief return the underlying condition variable handle (implementation specific).
    //! \return internal task handle representation
    virtual ConditionVariableHandle* getHandle() = 0;

    //! \brief provide a pointer to a ConditionVariable delegate object
    static ConditionVariableInterface* getDelegate(ConditionVariableHandleStorage& aligned_new_memory);
};
}  // namespace Os

#endif  // OS_CONDITIONVARIABLEINTERFACE_HPP_
