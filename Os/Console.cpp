// ======================================================================
// \title Os/Console.cpp
// \brief implementation of Os::ConsoleInterface shared behavior
// ======================================================================
#include <Os/Console.hpp>

namespace Os {

void ConsoleInterface::writeMessage(const Fw::ConstStringBase& message) {
    this->writeMessage(message.toChar(), message.length());
}

void ConsoleInterface::write(const CHAR* message, const FwSizeType size) {
    ConsoleInterface::getSingleton().writeMessage(message, size);
}

void ConsoleInterface::write(const Fw::ConstStringBase& message) {
    ConsoleInterface::getSingleton().writeMessage(message.toChar(), message.length());
}

void ConsoleInterface::init() {
    // Force trigger on the fly singleton setup
    (void)ConsoleInterface::getSingleton();
}

Console& ConsoleInterface::getSingleton() {
    static Console s_singleton;
    // Registration happens once: re-registering on every access would silently replace a
    // logger the project registered after the console singleton was first used
    static bool s_registered = false;
    if (not s_registered) {
        s_registered = true;
        Fw::Logger::registerLogger(&s_singleton);
    }
    return s_singleton;
}
}  // namespace Os
