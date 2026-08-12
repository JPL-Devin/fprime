// ======================================================================
// \title Os/Stub/test/DefaultDirectory.cpp
// \brief sets default Os::Directory to test stub implementation via linker
// ======================================================================
#include "Os/Delegate.hpp"
#include "Os/Stub/test/Directory.hpp"

namespace Os {

//! \brief get a delegate for Directory that intercepts calls for stub test directory usage
//! \param aligned_new_memory: aligned memory to fill
//! \return: pointer to delegate
DirectoryInterface* DirectoryInterface::getDelegate(DirectoryHandleStorage& aligned_new_memory) {
    return Os::Delegate::makeDelegate<DirectoryInterface, Os::Stub::Directory::Test::TestDirectory>(aligned_new_memory);
}
}  // namespace Os
