// ======================================================================
// \title Os/DelegateFile.cpp
// \brief implementation of the Os::DelegateFile class
// ======================================================================
#include <Fw/Types/Assert.hpp>
#include <Os/DelegateFile.hpp>

namespace Os {

DelegateFile::DelegateFile() : m_handle_storage(), m_delegate(*FileInterface::getDelegate(m_handle_storage)) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
}

DelegateFile::~DelegateFile() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    if (this->m_mode != OPEN_NO_MODE) {
        this->close();
    }
    m_delegate.~FileInterface();
}

DelegateFile::DelegateFile(const DelegateFile& other)
    : FileInterface(other),
      m_handle_storage(),
      m_delegate(*FileInterface::getDelegate(m_handle_storage, &other.m_delegate)) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
}

DelegateFile& DelegateFile::operator=(const DelegateFile& other) {
    if (this != &other) {
        // The delegate below is constructed over the storage of the existing one. Any file this
        // object currently holds must be closed first or its handle is orphaned permanently.
        if (this->m_mode != OPEN_NO_MODE) {
            this->close();
        }
        this->m_delegate.~FileInterface();
        FileInterface::operator=(other);
        (void)FileInterface::getDelegate(m_handle_storage, &other.m_delegate);
        FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    }
    return *this;
}

DelegateFile::Status DelegateFile::_open(const char* path, Mode mode, OverwriteType overwrite) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._open(path, mode, overwrite);
}

void DelegateFile::_close() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    this->m_delegate._close();
}

DelegateFile::Status DelegateFile::_size(FwSizeType& size_result) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._size(size_result);
}

DelegateFile::Status DelegateFile::_position(FwSizeType& position_result) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._position(position_result);
}

DelegateFile::Status DelegateFile::_preallocate(FwSizeType offset, FwSizeType length) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._preallocate(offset, length);
}

DelegateFile::Status DelegateFile::_seek(FwSignedSizeType offset, SeekType seekType) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._seek(offset, seekType);
}

DelegateFile::Status DelegateFile::_flush() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._flush();
}

DelegateFile::Status DelegateFile::_read(U8* buffer, FwSizeType& size, WaitType wait) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._read(buffer, size, wait);
}

DelegateFile::Status DelegateFile::_write(const U8* buffer, FwSizeType& size, WaitType wait) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate._write(buffer, size, wait);
}

FileHandle* DelegateFile::getHandle() {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate.getHandle();
}

DelegateFile::Status DelegateFile::getRawDescriptor(FwSizeType& descriptor) {
    FW_ASSERT(&this->m_delegate == reinterpret_cast<FileInterface*>(&this->m_handle_storage[0]));
    return this->m_delegate.getRawDescriptor(descriptor);
}
}  // namespace Os
