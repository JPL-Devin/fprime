// ======================================================================
// \title Os/File.cpp
// \brief shared function implementation for Os::FileInterface
// ======================================================================
#include <Fw/Types/Assert.hpp>
#include <Fw/Types/StringUtils.hpp>
#include <Os/File.hpp>
#include <algorithm>
#include <config/FppConstantsAc.hpp>

namespace Os {

FileInterface::Status FileInterface::open(const CHAR* filepath, FileInterface::Mode requested_mode) {
    return this->open(filepath, requested_mode, OverwriteType::NO_OVERWRITE);
}

FileInterface::Status FileInterface::open(const CHAR* filepath,
                                          FileInterface::Mode requested_mode,
                                          FileInterface::OverwriteType overwrite) {
    FW_ASSERT(nullptr != filepath);
    return this->open(filepath, static_cast<FwSizeType>(FileNameStringSize + 1), requested_mode, overwrite);
}

FileInterface::Status FileInterface::open(const CHAR* filepath, FwSizeType length, FileInterface::Mode requested_mode) {
    return this->open(filepath, length, requested_mode, OverwriteType::NO_OVERWRITE);
}

FileInterface::Status FileInterface::open(const CHAR* filepath,
                                          FwSizeType length,
                                          FileInterface::Mode requested_mode,
                                          FileInterface::OverwriteType overwrite) {
    FW_ASSERT(nullptr != filepath);
    const FwSizeType string_len = static_cast<FwSizeType>(Fw::StringUtils::string_length(filepath, length));
    FW_ASSERT(string_len < length, static_cast<FwAssertArgType>(string_len), static_cast<FwAssertArgType>(length));
    FW_ASSERT(Mode::OPEN_NO_MODE < requested_mode && Mode::MAX_OPEN_MODE > requested_mode);
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    FW_ASSERT((0 <= overwrite) && (overwrite < OverwriteType::MAX_OVERWRITE_TYPE));
    // Check for already opened file
    if (this->isOpen()) {
        return Status::INVALID_MODE;
    }
    Status status = this->_open(filepath, requested_mode, overwrite);
    if (status == Status::OP_OK) {
        this->m_mode = requested_mode;
        // Reset any open CRC calculations
        this->m_hash.init();
    }

    return status;
}

FileInterface::Status FileInterface::open(const Fw::ConstStringBase& path, FileInterface::Mode requested_mode) {
    return this->open(path.toChar(), static_cast<FwSizeType>(path.getCapacity()), requested_mode,
                      OverwriteType::NO_OVERWRITE);
}

FileInterface::Status FileInterface::open(const Fw::ConstStringBase& path,
                                          FileInterface::Mode requested_mode,
                                          FileInterface::OverwriteType overwrite) {
    return this->open(path.toChar(), static_cast<FwSizeType>(path.getCapacity()), requested_mode, overwrite);
}

void FileInterface::close() {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    this->_close();
    this->m_mode = Mode::OPEN_NO_MODE;
}

bool FileInterface::isOpen() const {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    return this->m_mode != Mode::OPEN_NO_MODE;
}

FileInterface::Status FileInterface::size(FwSizeType& size_result) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    if (OPEN_NO_MODE == this->m_mode) {
        return Status::NOT_OPENED;
    }
    return this->_size(size_result);
}

FileInterface::Status FileInterface::position(FwSizeType& position_result) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        return Status::NOT_OPENED;
    }
    return this->_position(position_result);
}

FileInterface::Status FileInterface::preallocate(FwSizeType offset, FwSizeType length) {
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        return Status::INVALID_MODE;
    }
    return this->_preallocate(offset, length);
}

FileInterface::Status FileInterface::seek(FwSignedSizeType offset, FileInterface::SeekType seekType) {
    FW_ASSERT((0 <= seekType) && (seekType < SeekType::MAX_SEEK_TYPE));
    // Cannot do a seek with a negative offset in absolute mode
    FW_ASSERT((seekType == SeekType::RELATIVE) || (offset >= 0));
    FW_ASSERT((0 <= this->m_mode) && (this->m_mode < Mode::MAX_OPEN_MODE));
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        return Status::NOT_OPENED;
    }
    return this->_seek(offset, seekType);
}

FileInterface::Status FileInterface::seek_absolute(FwSizeType offset) {
    Status status = Status::OTHER_ERROR;
    // If the offset can be represented by a signed value, then we can perform a single seek
    if (static_cast<FwSizeType>(std::numeric_limits<FwSignedSizeType>::max()) >= offset) {
        // Check that the bounding above is correct
        FW_ASSERT(static_cast<FwSignedSizeType>(offset) >= 0);
        status = this->seek(static_cast<FwSignedSizeType>(offset), SeekType::ABSOLUTE);
    }
    // Otherwise, a full seek to any value represented by FwSizeType can be performed
    // by at most 3 seeks of a FwSignedSizeType. Two half seeks (rounded down) that are
    // strictly bounded by std::numeric_limits<FwSignedSizeType>::max() and one seek of
    // a possibile "odd" byte to ensure odds offsets do not introduce an off-by-one-error.
    // Thus we perform 3 seeks to guarantee that we can reach any position.
    else {
        FwSignedSizeType half_offset = static_cast<FwSignedSizeType>(offset >> 1);
        bool is_odd = (offset % 2) == 1;
        status = this->seek(half_offset, SeekType::ABSOLUTE);
        if (status == Status::OP_OK) {
            status = this->seek(half_offset, SeekType::RELATIVE);
        }
        if (status == Status::OP_OK) {
            status = this->seek((is_odd) ? 1 : 0, SeekType::RELATIVE);
        }
    }
    return status;
}

FileInterface::Status FileInterface::flush() {
    FW_ASSERT(this->m_mode < Mode::MAX_OPEN_MODE);
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        return Status::INVALID_MODE;
    }
    return this->_flush();
}

FileInterface::Status FileInterface::read(U8* buffer, FwSizeType& size) {
    return this->read(buffer, size, WaitType::WAIT);
}

FileInterface::Status FileInterface::read(U8* buffer, FwSizeType& size, FileInterface::WaitType wait) {
    FW_ASSERT(buffer != nullptr);
    FW_ASSERT(this->m_mode < Mode::MAX_OPEN_MODE);
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        size = 0;
        return Status::NOT_OPENED;
    } else if (OPEN_READ != this->m_mode) {
        size = 0;
        return Status::INVALID_MODE;
    }
    return this->_read(buffer, size, wait);
}

FileInterface::Status FileInterface::write(const U8* buffer, FwSizeType& size) {
    return this->write(buffer, size, WaitType::WAIT);
}

FileInterface::Status FileInterface::write(const U8* buffer, FwSizeType& size, FileInterface::WaitType wait) {
    FW_ASSERT(buffer != nullptr);
    FW_ASSERT(this->m_mode < Mode::MAX_OPEN_MODE);
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        size = 0;
        return Status::NOT_OPENED;
    } else if (OPEN_READ == this->m_mode) {
        size = 0;
        return Status::INVALID_MODE;
    }
    return this->_write(buffer, size, wait);
}

FileInterface::Status FileInterface::calculateCrc(U32& crc) {
    Status status = Status::OP_OK;
    FwSizeType size = FW_FILE_CHUNK_SIZE;
    crc = 0;
    for (FwSizeType i = 0; i < std::numeric_limits<FwSizeType>::max(); i++) {
        status = this->incrementalCrc(size);
        // Break on eof or error
        if ((size != FW_FILE_CHUNK_SIZE) || (status != Status::OP_OK)) {
            break;
        }
    }
    // When successful, finalize the CRC
    if (status == Status::OP_OK) {
        status = this->finalizeCrc(crc);
    }
    return status;
}

FileInterface::Status FileInterface::incrementalCrc(FwSizeType& size) {
    Status status = Status::OP_OK;
    FW_ASSERT(size <= FW_FILE_CHUNK_SIZE, FwAssertArgType(size));
    if (OPEN_NO_MODE == this->m_mode) {
        status = Status::NOT_OPENED;
    } else if (OPEN_READ != this->m_mode) {
        status = Status::INVALID_MODE;
    } else {
        // Read data without waiting for additional data to be available
        status = this->read(this->m_crc_buffer, size, WaitType::NO_WAIT);
        if (Status::OP_OK == status) {
            FW_ASSERT(size <= FW_FILE_CHUNK_SIZE, FwAssertArgType(size));
            this->m_hash.update(this->m_crc_buffer, size);
        }
    }
    return status;
}

FileInterface::Status FileInterface::finalizeCrc(U32& crc) {
    Status status = Status::OP_OK;
    this->m_hash.finalize(crc);
    // Historically, the CRC calculation in File omitted the final 1's complement step. Utils::Hash performs that step
    // and as such, we must undo it before returning the value to ensure backwards compatibility.
    crc = ~crc;
    this->m_hash.init();
    return status;
}

FileInterface::Status FileInterface::readline(U8* buffer, FwSizeType& size, FileInterface::WaitType wait) {
    const FwSizeType requested_size = size;
    FW_ASSERT(buffer != nullptr);
    FW_ASSERT(this->m_mode < Mode::MAX_OPEN_MODE);
    // Check that the file is open before attempting operation
    if (OPEN_NO_MODE == this->m_mode) {
        size = 0;
        return Status::NOT_OPENED;
    } else if (OPEN_READ != this->m_mode) {
        size = 0;
        return Status::INVALID_MODE;
    }
    FwSizeType original_location = 0;
    Status status = this->position(original_location);
    if (status != Status::OP_OK) {
        size = 0;
        (void)this->seek_absolute(original_location);
        return status;
    }
    FwSizeType read = 0;
    // Loop reading chunk by chunk
    for (FwSizeType i = 0; i < size; i += read) {
        // read in chunks to avoid large buffer allocations
        FwSizeType current_chunk_size = std::min(size - i, static_cast<FwSizeType>(FW_FILE_CHUNK_SIZE));
        read = current_chunk_size;
        status = this->read(buffer + i, read, wait);
        if (status != Status::OP_OK) {
            // Contract: on error, seek back to the original location
            size = 0;
            (void)this->seek_absolute(original_location);
            return status;
        }
        // EOF break out now
        if (read == 0) {
            size = i;
            return Status::OP_OK;
        }
        // Loop from i to i + current_chunk_size looking for `\n`
        const FwSizeType chunk_end = i + read;
        for (FwSizeType j = i; j < chunk_end; j++) {
            // Newline seek back to after it, return the size read
            if (buffer[j] == '\n') {
                size = j + 1;
                // Ensure that the computation worked and there is not overflow
                FW_ASSERT(size <= requested_size);
                FW_ASSERT(std::numeric_limits<FwSizeType>::max() - size >= original_location);
                (void)this->seek_absolute(original_location + j + 1);
                return Status::OP_OK;
            }
        }
    }
    // Failed to find newline within data available
    // Contract: on error, seek back to the original location
    size = 0;
    (void)this->seek_absolute(original_location);
    return Status::OTHER_ERROR;
}

FileInterface::Status FileInterface::getRawDescriptor(FwSizeType& descriptor) {
    (void)descriptor;
    return Status::NOT_SUPPORTED;
}
}  // namespace Os
