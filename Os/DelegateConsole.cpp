// ======================================================================
// \title Os/DelegateConsole.cpp
// \brief common function implementation for Os::DelegateConsole
// ======================================================================
#include <Fw/Types/Assert.hpp>
#include <Os/DelegateConsole.hpp>

namespace Os {
DelegateConsole::DelegateConsole()
    : ConsoleInterface(), m_handle_storage(), m_delegate(*ConsoleInterface::getDelegate(m_handle_storage)) {}

DelegateConsole::~DelegateConsole() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<ConsoleInterface*>(&this->m_handle_storage[0]));
    m_delegate.~ConsoleInterface();
}

DelegateConsole::DelegateConsole(const DelegateConsole& other)
    : m_handle_storage(), m_delegate(*DelegateConsole::getDelegate(m_handle_storage, &other.m_delegate)) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<ConsoleInterface*>(&this->m_handle_storage[0]));
}

DelegateConsole& DelegateConsole::operator=(const DelegateConsole& other) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<ConsoleInterface*>(&this->m_handle_storage[0]));
    if (this != &other) {
        this->m_delegate = *ConsoleInterface::getDelegate(m_handle_storage, &other.m_delegate);
    }
    return *this;
}

void DelegateConsole::writeMessage(const CHAR* message, const FwSizeType size) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<ConsoleInterface*>(&this->m_handle_storage));
    FW_ASSERT(message != nullptr || size == 0);
    this->m_delegate.writeMessage(message, size);
}

ConsoleHandle* DelegateConsole::getHandle() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<ConsoleInterface*>(&this->m_handle_storage));
    return this->m_delegate.getHandle();
}
}  // namespace Os
