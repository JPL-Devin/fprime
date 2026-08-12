// ======================================================================
// \title Os/Stub/File.cpp
// \brief stub implementation for Os::File
// ======================================================================
#include "Os/Stub/File.hpp"

namespace Os {
namespace Stub {
namespace File {

StubFile::Status StubFile::_open(const char* filepath, StubFile::Mode open_mode, OverwriteType overwrite) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

void StubFile::_close() {}

StubFile::Status StubFile::_size(FwSizeType& size_result) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_position(FwSizeType& position_result) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_preallocate(FwSizeType offset, FwSizeType length) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_seek(FwSignedSizeType offset, SeekType seekType) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_flush() {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_read(U8* buffer, FwSizeType& size, StubFile::WaitType wait) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

StubFile::Status StubFile::_write(const U8* buffer, FwSizeType& size, StubFile::WaitType wait) {
    Status status = Status::NOT_SUPPORTED;
    return status;
}

FileHandle* StubFile::getHandle() {
    return &this->m_handle;
}

}  // namespace File
}  // namespace Stub
}  // namespace Os
