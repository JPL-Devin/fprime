// ======================================================================
// \title Os/DelegateFile.hpp
// \brief Define the Os::DelegateFile class
// ======================================================================
#ifndef Os_DelegateFile_hpp_
#define Os_DelegateFile_hpp_

#include "Os/FileInterface.hpp"

namespace Os {

//! \brief file implementation selected at link time
//!
//! Forwards each implementation primitive to the delegate constructed by `FileInterface::getDelegate`. All shared file
//! behavior is supplied by `FileInterface`.
class DelegateFile final : public FileInterface {
  public:
    //! \brief constructor
    DelegateFile();

    //! \brief destructor, closes the file if it is open
    ~DelegateFile() final;

    //! \brief copy constructor that copies the internal representation
    DelegateFile(const DelegateFile& other);

    //! \brief assignment operator that copies the internal representation
    DelegateFile& operator=(const DelegateFile& other);

    //! \brief open file with supplied path, mode, and overwrite type. Delegates to implementation.
    Status _open(const char* path, Mode mode, OverwriteType overwrite) override;

    //! \brief close the underlying file. Delegates to implementation.
    void _close() override;

    //! \brief get size of the underlying file. Delegates to implementation.
    Status _size(FwSizeType& size_result) override;

    //! \brief get file pointer position of the underlying file. Delegates to implementation.
    Status _position(FwSizeType& position_result) override;

    //! \brief pre-allocate storage for the underlying file. Delegates to implementation.
    Status _preallocate(FwSizeType offset, FwSizeType length) override;

    //! \brief seek the underlying file pointer to the given offset. Delegates to implementation.
    Status _seek(FwSignedSizeType offset, SeekType seekType) override;

    //! \brief flush the underlying file's contents to storage. Delegates to implementation.
    Status _flush() override;

    //! \brief read data from the underlying file. Delegates to implementation.
    Status _read(U8* buffer, FwSizeType& size, WaitType wait) override;

    //! \brief write data to the underlying file. Delegates to implementation.
    Status _write(const U8* buffer, FwSizeType& size, WaitType wait) override;

    //! \brief returns the raw file handle. Delegates to implementation.
    FileHandle* getHandle() override;

    //! \brief return the implementation's raw descriptor. Delegates to implementation.
    Status getRawDescriptor(FwSizeType& descriptor) override;

  private:
    // This section is used to store the implementation-defined file handle. To Os::File and fprime, this type is
    // opaque and thus normal allocation cannot be done. Instead, we allow the implementor to store then handle in
    // the byte-array here and set `handle` to that address for storage.
    //
    alignas(FW_HANDLE_ALIGNMENT) FileHandleStorage m_handle_storage;  //!< Storage for aligned FileHandle data
    FileInterface& m_delegate;                                        //!< Delegate for the real implementation
};
}  // namespace Os
#endif  // Os_DelegateFile_hpp_
