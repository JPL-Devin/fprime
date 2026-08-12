// ======================================================================
// \title Os/FileInterface.hpp
// \brief definition of the Os::FileInterface abstraction
// ======================================================================
#ifndef Os_FileInterface_hpp_
#define Os_FileInterface_hpp_

#include <Fw/FPrimeBasicTypes.hpp>
#include <Fw/Types/ConstStringBase.hpp>
#include <Os/Os.hpp>
#include <Utils/Hash/Hash.hpp>
#include "config/OsDelegateFile.hpp"

// Forward declaration for UTs
namespace Os {
namespace Test {
namespace FileTest {
struct Tester;
}
}  // namespace Test
}  // namespace Os

namespace Os {

//! \brief base implementation of FileHandle
//!
struct FileHandle {};

//! \brief file abstraction supplying the shared file behavior
//!
//! This class encapsulates a very simple file interface that has the most often-used features. Implementations supply
//! the `_`-prefixed primitives, which perform raw operations on the underlying file. This class implements the shared
//! behavior on top of those primitives: mode tracking, mode-based error checking, CRC calculation, and the convenience
//! overloads.
class FileInterface {
    friend struct Os::Test::FileTest::Tester;

  public:
    enum Mode {
        OPEN_NO_MODE,     //!< File mode not yet selected
        OPEN_READ,        //!< Open file for reading
        OPEN_CREATE,      //!< Open file for writing and truncates file if it exists, ie same flags as creat()
        OPEN_WRITE,       //!< Open file for writing
        OPEN_SYNC_WRITE,  //!< Open file for writing; writes don't return until data is on disk
        OPEN_APPEND,      //!< Open file for appending
        MAX_OPEN_MODE     //!< Maximum value of mode
    };

    enum Status {
        OP_OK,              //!< Operation was successful
        DOESNT_EXIST,       //!< File doesn't exist (for read)
        NO_SPACE,           //!< No space left
        NO_PERMISSION,      //!< No permission to read/write file
        BAD_SIZE,           //!< Invalid size parameter
        NOT_OPENED,         //!< file hasn't been opened yet
        FILE_EXISTS,        //!< file already exist (for CREATE with O_EXCL enabled)
        NOT_SUPPORTED,      //!< Kernel or file system does not support operation
        INVALID_MODE,       //!< Mode for file access is invalid for current operation
        INVALID_ARGUMENT,   //!< Invalid argument passed in
        NO_MORE_RESOURCES,  //!< No more available resources
        OTHER_ERROR,        //!< A catch-all for other errors. Have to look in implementation-specific code
        OUTSIDE_SANDBOX,    //!< Path falls outside the configured sandbox directory
        MAX_STATUS          //!< Maximum value of status
    };

    enum OverwriteType {
        NO_OVERWRITE,  //!< Do NOT overwrite existing files
        OVERWRITE,     //!< Overwrite file when it exists and creation was requested
        MAX_OVERWRITE_TYPE
    };

    enum SeekType {
        RELATIVE,  //!< Relative seek from current file offset
        ABSOLUTE,  //!< Absolute seek from beginning of file
        MAX_SEEK_TYPE
    };

    enum WaitType {
        NO_WAIT,  //!< Do not wait for read/write operation to finish
        WAIT,     //!< Do wait for read/write operation to finish
        MAX_WAIT_TYPE
    };

    FileInterface() = default;
    virtual ~FileInterface() = default;

    // ------------------------------------
    // Shared implementation
    // ------------------------------------

    //! \brief determine if the file is open
    //! \return true if file is open, false otherwise
    //!
    bool isOpen() const;

    //! \brief open file with supplied path and mode
    //!
    //! Open the file passed in with the given mode. Opening files with `OPEN_CREATE` mode will not clobber existing
    //! files. Use the overload accepting `OverwriteType` to set the overwrite flag and clobber existing files.
    //!
    //! It is invalid to send `nullptr` as the path.
    //! It is invalid to supply `mode` as a non-enumerated value.
    //!
    //! \param path: c-string of path to open
    //! \param mode: file operation mode
    //! \return: status of the open
    //!
    Status open(const char* path, Mode mode);

    //! \brief open file with supplied path, mode, and overwrite type
    //!
    //! Open the file passed in with the given mode. If overwrite is set to OVERWRITE, then opening files in
    //! OPEN_CREATE mode will clobber existing files. Set overwrite to NO_OVERWRITE to preserve existing files.
    //!
    //! It is invalid to send `nullptr` as the path.
    //! It is invalid to supply `mode` as a non-enumerated value.
    //! It is invalid to supply `overwrite` as a non-enumerated value.
    //!
    //! \param path: c-string of path to open
    //! \param mode: file operation mode
    //! \param overwrite: overwrite existing file on create
    //! \return: status of the open
    //!
    Status open(const char* path, Mode mode, OverwriteType overwrite);

    //! \brief open file with supplied path, bounded length, and mode
    //!
    //! Open the file passed in with the given mode. The path length is bounded by `length`.
    //!
    //! It is invalid to send `nullptr` as the path.
    //! It is invalid to supply `mode` as a non-enumerated value.
    //! It is invalid for the path to not be null-terminated within `length` characters.
    //!
    //! \param path: c-string of path to open
    //! \param length: bound on the path buffer size
    //! \param mode: file operation mode
    //! \return: status of the open
    //!
    Status open(const char* path, FwSizeType length, Mode mode);

    //! \brief open file with supplied path, bounded length, mode, and overwrite type
    //!
    //! Open the file passed in with the given mode. The path length is bounded by `length`. This is the core open
    //! implementation to which all other open overloads delegate.
    //!
    //! It is invalid to send `nullptr` as the path.
    //! It is invalid to supply `mode` as a non-enumerated value.
    //! It is invalid to supply `overwrite` as a non-enumerated value.
    //! It is invalid for the path to not be null-terminated within `length` characters.
    //!
    //! \param path: c-string of path to open
    //! \param length: bound on the path buffer size
    //! \param mode: file operation mode
    //! \param overwrite: overwrite existing file on create
    //! \return: status of the open
    //!
    Status open(const char* path, FwSizeType length, Mode mode, OverwriteType overwrite);

    //! \brief open file with supplied string path and mode
    //!
    //! \param path: ConstStringBase reference of path to open
    //! \param mode: file operation mode
    //! \return: status of the open
    //!
    Status open(const Fw::ConstStringBase& path, Mode mode);

    //! \brief open file with supplied string path, mode, and overwrite type
    //!
    //! \param path: ConstStringBase reference of path to open
    //! \param mode: file operation mode
    //! \param overwrite: overwrite existing file on create
    //! \return: status of the open
    //!
    Status open(const Fw::ConstStringBase& path, Mode mode, OverwriteType overwrite);

    //! \brief close the file, if not opened then do nothing
    //!
    //! Closes the file, if open. Otherwise this function does nothing. `mode` is set to `OPEN_NO_MODE`.
    //!
    void close();

    //! \brief get size of currently open file
    //! \param size_result: output parameter for size.
    //! \return OP_OK on success otherwise error status
    //!
    Status size(FwSizeType& size_result);

    //! \brief get file pointer position of the currently open file
    //! \param position_result: output parameter for position.
    //! \return OP_OK on success otherwise error status
    //!
    Status position(FwSizeType& position_result);

    //! \brief pre-allocate file storage
    //!
    //! Pre-allocates file storage with at least `length` storage starting at `offset`. No-op on implementations
    //! that cannot pre-allocate.
    //!
    //! \param offset: offset into file
    //! \param length: length after offset to preallocate
    //! \return OP_OK on success otherwise error status
    //!
    Status preallocate(FwSizeType offset, FwSizeType length);

    //! \brief seek the file pointer to the given offset
    //!
    //! Seek the file pointer to the given `offset`. If `seekType` is set to `ABSOLUTE` then the offset is calculated
    //! from the start of the file, and if it is set to `RELATIVE` it is calculated from the current position.
    //!
    //! \param offset: offset to seek to
    //! \param seekType: `ABSOLUTE` for seeking from beginning of file, `RELATIVE` to use current position.
    //! \return OP_OK on success otherwise error status
    //!
    Status seek(FwSignedSizeType offset, SeekType seekType);

    //! \brief seek the file pointer to the given offset absolutely with the full range
    //!
    //! Equivalent to calling `seek` with `ABSOLUTE` as the `seekType` with the exception that it can handle the full
    //! range of `FwSizeType` values as returned by `size` and `position` calls. Internally, it will perform multiple
    //! seeks to reach the desired offset while never exceeding the signed limit of the basic `seek` function.
    //!
    //! \param offset_unsigned: offset to absolutely seek to
    //! \return OP_OK on success otherwise error status
    Status seek_absolute(FwSizeType offset_unsigned);

    //! \brief flush file contents to storage
    //!
    //! Flushes the file contents to storage (i.e. out of the OS cache to disk). Does nothing in implementations
    //! that do not support flushing.
    //!
    //! \return OP_OK on success otherwise error status
    //!
    Status flush();

    //! \brief read data from this file into supplied buffer bounded by size, waiting for data
    //!
    //! `size` will be updated to the count of bytes actually read.
    //!
    //! It is invalid to pass `nullptr` to this function call.
    //!
    //! \param buffer: memory location to store data read from file
    //! \param size: size of data to read
    //! \return OP_OK on success otherwise error status
    //!
    Status read(U8* buffer, FwSizeType& size);

    //! \brief read data from this file into supplied buffer bounded by size
    //!
    //! When `wait` is set to `WAIT`, this will block until the requested size has been read or the end of the file
    //! has been reached. When `wait` is set to `NO_WAIT` it will return whatever data is currently available.
    //!
    //! It is invalid to pass `nullptr` to this function call.
    //! It is invalid to supply wait as a non-enumerated value.
    //!
    //! \param buffer: memory location to store data read from file
    //! \param size: size of data to read
    //! \param wait: `WAIT` to wait for data, `NO_WAIT` to return what is currently available
    //! \return OP_OK on success otherwise error status
    //!
    Status read(U8* buffer, FwSizeType& size, WaitType wait);

    //! \brief read a line from the file using `\n` as the delimiter
    //!
    //! Reads a single line from the file including the terminating '\n'. This will return an error if no line is
    //! found within the specified buffer size. In the case of EOF, the line is read without the terminating '\n'.
    //!
    //! In the case of an error, this function will seek to the original location in the file. Otherwise, the
    //! pointer will point to the first character after the `\n` or EOF in the case of no `\n`.
    //!
    //! It is invalid to send a null buffer.
    //! It is an error if the file is not opened for reading.
    //!
    //! \param buffer: memory location to store data read from file
    //! \param size: maximum size of buffer to store the new line
    //! \param wait: `WAIT` to wait for data, `NO_WAIT` to return what is currently available
    //! \return OP_OK on success otherwise error status
    Status readline(U8* buffer, FwSizeType& size, WaitType wait);

    //! \brief write data to this file from the supplied buffer bounded by size, waiting for the write to finish
    //!
    //! `size` will be updated to the count of bytes actually written.
    //!
    //! It is invalid to pass `nullptr` to this function call.
    //!
    //! \param buffer: memory location of data to write to file
    //! \param size: size of data to write
    //! \return OP_OK on success otherwise error status
    //!
    Status write(const U8* buffer, FwSizeType& size);

    //! \brief write data to this file from the supplied buffer bounded by size
    //!
    //! When `wait` is set to `WAIT`, this will block until the requested size has been written successfully to disk.
    //! When `wait` is set to `NO_WAIT` it will return once the data is sent to the OS.
    //!
    //! It is invalid to pass `nullptr` to this function call.
    //! It is invalid to supply wait as a non-enumerated value.
    //!
    //! \param buffer: memory location of data to write to file
    //! \param size: size of data to write
    //! \param wait: `WAIT` to wait for data to write to disk, `NO_WAIT` to return once sent to the OS
    //! \return OP_OK on success otherwise error status
    //!
    Status write(const U8* buffer, FwSizeType& size, WaitType wait);

    //! \brief calculate the CRC32 of the entire file
    //!
    //! Calculates the CRC32 of the file's contents. The `crc` parameter will be updated to contain the CRC or 0 on
    //! failure. This call will be decomposed into calculations on sections of the file `FW_FILE_CHUNK_SIZE` bytes
    //! long. This function requires that the file already be opened for "READ" mode.
    //!
    //! \note: the file pointer will be positioned at the end of the file after this call.
    //!
    //! \param crc: U32 bit value to fill with CRC
    //! \return OP_OK on success otherwise error status
    //!
    Status calculateCrc(U32& crc);

    //! \brief calculate the CRC32 of the next section of data
    //!
    //! Starting at the current file pointer, this will add `size` bytes of data to the currently calculated CRC.
    //! Call `finalizeCrc` to retrieve the CRC or `calculateCrc` to perform a CRC on the entire file. This call will
    //! not block waiting for data on the underlying read, nor will it reset the file position pointer. On error,
    //! the current CRC results should be discarded by reopening the file or calling `finalizeCrc` and discarding its
    //! result. `size` will be updated with the `size` actually read and used in the CRC calculation.
    //!
    //! This function requires that the file already be opened for "READ" mode.
    //!
    //! It is illegal for size to be greater than FW_FILE_CHUNK_SIZE.
    //!
    //! \param size: size of data to read for CRC
    //! \return: status of the CRC calculation
    //!
    Status incrementalCrc(FwSizeType& size);

    //! \brief finalize and retrieve the CRC value
    //!
    //! Finalizes the CRC computation and returns the CRC value. Note: this will reset any active CRC calculation and
    //! effectively re-initializes any `incrementalCrc` calculation.
    //!
    //! \param crc: value to fill
    //! \return status of the CRC calculation
    //!
    Status finalizeCrc(U32& crc);

    // ------------------------------------
    // Implementation primitives
    // ------------------------------------

    //! \brief open the file with the supplied path, mode, and overwrite type
    //!
    //! Implementations may assume the path is non-null, null-terminated, and that the mode and overwrite type are
    //! enumerated values. Mode tracking is performed by the shared implementation.
    //!
    //! \param path: c-string of path to open
    //! \param mode: file operation mode
    //! \param overwrite: overwrite existing file on create
    //! \return: status of the open
    //!
    virtual Status _open(const char* path, Mode mode, OverwriteType overwrite) = 0;

    //! \brief close the underlying file
    virtual void _close() = 0;

    //! \brief get size of the underlying file
    //! \param size_result: output parameter for size.
    //! \return OP_OK on success otherwise error status
    virtual Status _size(FwSizeType& size_result) = 0;

    //! \brief get file pointer position of the underlying file
    //! \param position_result: output parameter for position.
    //! \return OP_OK on success otherwise error status
    virtual Status _position(FwSizeType& position_result) = 0;

    //! \brief pre-allocate storage for the underlying file
    //! \param offset: offset into file
    //! \param length: length after offset to preallocate
    //! \return OP_OK on success otherwise error status
    virtual Status _preallocate(FwSizeType offset, FwSizeType length) = 0;

    //! \brief seek the underlying file pointer to the given offset
    //! \param offset: offset to seek to
    //! \param seekType: `ABSOLUTE` for seeking from beginning of file, `RELATIVE` to use current position.
    //! \return OP_OK on success otherwise error status
    virtual Status _seek(FwSignedSizeType offset, SeekType seekType) = 0;

    //! \brief flush the underlying file's contents to storage
    //! \return OP_OK on success otherwise error status
    virtual Status _flush() = 0;

    //! \brief read data from the underlying file into the supplied buffer bounded by size
    //! \param buffer: memory location to store data read from file
    //! \param size: size of data to read
    //! \param wait: `WAIT` to wait for data, `NO_WAIT` to return what is currently available
    //! \return OP_OK on success otherwise error status
    virtual Status _read(U8* buffer, FwSizeType& size, WaitType wait) = 0;

    //! \brief write data to the underlying file from the supplied buffer bounded by size
    //! \param buffer: memory location of data to write to file
    //! \param size: size of data to write
    //! \param wait: `WAIT` to wait for data to write to disk, `NO_WAIT` to return once sent to the OS
    //! \return OP_OK on success otherwise error status
    virtual Status _write(const U8* buffer, FwSizeType& size, WaitType wait) = 0;

    //! \brief returns the raw file handle
    //!
    //! Gets the raw file handle from the implementation. Note: users must include the implementation specific
    //! header to make any real use of this handle. Otherwise it will be an opaque type.
    //!
    //! \return raw file handle
    //!
    virtual FileHandle* getHandle() = 0;

    //! \brief return the implementation's raw descriptor for this file
    //!
    //! Supplies the implementation's raw descriptor (e.g. a posix file descriptor) for use with implementation
    //! specific APIs. Implementations without such a descriptor return `NOT_SUPPORTED` and leave `descriptor`
    //! unmodified.
    //!
    //! \param descriptor: output parameter filled with the raw descriptor
    //! \return OP_OK on success, NOT_SUPPORTED when no descriptor exists, otherwise error status
    //!
    virtual Status getRawDescriptor(FwSizeType& descriptor);

    //! \brief provide a pointer to a file delegate object
    //!
    //! This function must return a pointer to a `FileInterface` object that contains the real implementation of the
    //! file functions as defined by the implementor.  This function must do several things to be considered correctly
    //! implemented:
    //!
    //! 1. Assert that the supplied memory is non-null. e.g `FW_ASSERT(aligned_placement_new_memory != NULL);`
    //! 2. Assert that their implementation fits within FW_FILE_HANDLE_MAX_SIZE.
    //!    e.g. `static_assert(sizeof(PosixFileImplementation) <= sizeof Os::File::m_handle_storage,
    //!        "FW_FILE_HANDLE_MAX_SIZE too small");`
    //! 3. Assert that their implementation aligns within FW_HANDLE_ALIGNMENT.
    //!    e.g. `static_assert((FW_HANDLE_ALIGNMENT % alignof(PosixFileImplementation)) == 0, "Bad handle alignment");`
    //! 4. If to_copy is null, placement new their implementation into `aligned_placement_new_memory`
    //!    e.g. `FileInterface* interface = new (aligned_placement_new_memory) PosixFileImplementation;`
    //! 5. If to_copy is non-null, placement new using copy constructor their implementation into
    //!    `aligned_placement_new_memory`
    //!    e.g. `FileInterface* interface = new (aligned_placement_new_memory) PosixFileImplementation(*to_copy);`
    //! 6. Return the result of the placement new
    //!    e.g. `return interface;`
    //!
    //! \return result of placement new, must be equivalent to `aligned_placement_new_memory`
    //!
    static FileInterface* getDelegate(FileHandleStorage& aligned_placement_new_memory,
                                      const FileInterface* to_copy = nullptr);

  protected:
    static const U32 INITIAL_CRC = 0xFFFFFFFF;  //!< Initial value for CRC calculation

    Mode m_mode = Mode::OPEN_NO_MODE;  //!< Stores mode for error checking

    Utils::Hash m_hash;  //!< Hash object for incremental CRC calculation
    U8 m_crc_buffer[FW_FILE_CHUNK_SIZE];
};
}  // namespace Os
#endif  // Os_FileInterface_hpp_
