// ======================================================================
// \title FileTester
// \brief Testing implementation of Os::File
// ======================================================================
#ifndef SVC_FILEWORKER_TEST_UT_FILETESTER_HPP
#define SVC_FILEWORKER_TEST_UT_FILETESTER_HPP

#include "Os/Delegate.hpp"
#include "Os/Directory.hpp"
#include "Os/File.hpp"
#include "Os/FileSystem.hpp"
#include "Os/Posix/Directory.hpp"
#include "Os/Posix/File.hpp"
#include "Os/Posix/FileSystem.hpp"

namespace Svc {

struct FileTesterHandle : public Os::FileHandle {};

//! \brief stub implementation of Os::File
//!
//! Stub implementation of `FileInterface` for use as a delegate class handling
//! error-only file operations.
//!
class FileTester : public Os::FileInterface {
  public:
    //! \brief constructor
    //!
    FileTester() = default;

    //! \brief destructor
    //!
    ~FileTester() override = default;

    Os::FileInterface::Status _open(const char* path, Mode mode, OverwriteType overwrite) override {
        return m_statOpen;
    }

    void _close() override { return; }

    Status _size(FwSizeType& size_result) override {
        (void)size_result;
        size_result = m_size;
        return m_statSize;
    }

    Status _position(FwSizeType& position_result) override { return OP_OK; }

    Status _preallocate(FwSizeType offset, FwSizeType length) override { return OP_OK; }

    Status _seek(FwSignedSizeType offset, SeekType seekType) override { return OP_OK; }

    Status _flush() override { return OP_OK; }

    Status _read(U8* buffer, FwSizeType& size, WaitType wait) override { return m_statRead; }

    Status _write(const U8* buffer, FwSizeType& size, WaitType wait) override { return m_statWrite; }

    Os::FileHandle* getHandle() override { return reinterpret_cast<Os::FileHandle*>(&m_handle); }

    static void setOpen(const Status s) { m_statOpen = s; }
    static void setRead(const Status s) { m_statRead = s; }
    static void setWrite(const Status s) { m_statWrite = s; }
    static void setSize(const Status s, const FwSizeType sz) {
        m_statSize = s;
        m_size = sz;
    }

  private:
    //! File handle for PosixFile
    FileTesterHandle m_handle;

    static Status m_statOpen;
    static Status m_statRead;
    static Status m_statWrite;
    static Status m_statSize;
    static FwSizeType m_size;
};

}  // namespace Svc

#endif  // SVC_FILEWORKER_TEST_UT_FILETESTER_HPP
