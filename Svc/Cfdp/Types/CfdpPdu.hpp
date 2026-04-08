// ======================================================================
// \title  CfdpPdu.hpp
// \author devin
// \brief  CFDP PDU header and type definitions per CCSDS 727.0-B-5
//
// \copyright
// Copyright 2025, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#ifndef Svc_CfdpPdu_HPP
#define Svc_CfdpPdu_HPP

#include <Fw/Buffer/Buffer.hpp>
#include <Fw/FPrimeBasicTypes.hpp>
#include <Fw/Types/Serializable.hpp>

namespace Svc {
namespace Cfdp {

// -----------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------

//! CFDP protocol version (version 2, encoded as 0b001)
static constexpr U8 CFDP_VERSION = 0x01;

//! Maximum entity ID length in bytes (1..8)
static constexpr U8 MAX_ENTITY_ID_LEN = 8;

//! Maximum transaction sequence number length in bytes (1..8)
static constexpr U8 MAX_SEQ_NUM_LEN = 8;

//! Maximum source/destination file name length
static constexpr U32 MAX_FILE_NAME_LEN = 255;

//! Maximum file data segment size
static constexpr U32 MAX_FILE_DATA_SIZE = 1024;

// -----------------------------------------------------------------------
// Enumerations from the CFDP standard
// -----------------------------------------------------------------------

//! PDU type (Table 5-1)
enum class PduType : U8 {
    FILE_DIRECTIVE = 0,
    FILE_DATA = 1
};

//! PDU direction (Table 5-1)
enum class Direction : U8 {
    TOWARD_RECEIVER = 0,
    TOWARD_SENDER = 1
};

//! Transmission mode (Table 5-1)
enum class TransmissionMode : U8 {
    ACKNOWLEDGED = 0,
    UNACKNOWLEDGED = 1
};

//! File directive codes (Table 5-4)
enum class DirectiveCode : U8 {
    RESERVED_0 = 0x00,
    RESERVED_1 = 0x01,
    RESERVED_2 = 0x02,
    RESERVED_3 = 0x03,
    EOF_PDU = 0x04,
    FINISHED_PDU = 0x05,
    ACK_PDU = 0x06,
    METADATA_PDU = 0x07,
    NAK_PDU = 0x08,
    PROMPT_PDU = 0x09,
    KEEP_ALIVE_PDU = 0x0C
};

//! Condition codes (Table 5-5)
enum class ConditionCode : U8 {
    NO_ERROR = 0x00,
    POSITIVE_ACK_LIMIT_REACHED = 0x01,
    KEEP_ALIVE_LIMIT_REACHED = 0x02,
    INVALID_TRANSMISSION_MODE = 0x03,
    FILESTORE_REJECTION = 0x04,
    FILE_CHECKSUM_FAILURE = 0x05,
    FILE_SIZE_ERROR = 0x06,
    NAK_LIMIT_REACHED = 0x07,
    INACTIVITY_DETECTED = 0x08,
    INVALID_FILE_STRUCTURE = 0x09,
    CHECK_LIMIT_REACHED = 0x0A,
    UNSUPPORTED_CHECKSUM_TYPE = 0x0B,
    SUSPEND_REQUEST_RECEIVED = 0x0E,
    CANCEL_REQUEST_RECEIVED = 0x0F
};

//! Delivery code for Finished PDU (Table 5-7)
enum class DeliveryCode : U8 {
    DATA_COMPLETE = 0,
    DATA_INCOMPLETE = 1
};

//! File status for Finished PDU (Table 5-7)
enum class FileStatus : U8 {
    DISCARDED_DELIBERATELY = 0x00,
    DISCARDED_FILESTORE_REJECTION = 0x01,
    RETAINED_SUCCESSFULLY = 0x02,
    STATUS_UNREPORTED = 0x03
};

// -----------------------------------------------------------------------
// PDU Header (Section 5.1, Table 5-1)
// -----------------------------------------------------------------------

//! Fixed PDU Header structure per CCSDS 727.0-B-5 Section 5.1
struct PduHeader {
    U8 version;                 //!< 3 bits, shall be 001
    PduType pduType;            //!< 1 bit
    Direction direction;        //!< 1 bit
    TransmissionMode mode;      //!< 1 bit
    bool crcFlag;               //!< 1 bit
    bool largeFileFlag;         //!< 1 bit
    U16 dataFieldLength;        //!< 16 bits, length of data field in octets
    bool segmentationControl;   //!< 1 bit
    U8 entityIdLength;          //!< 3 bits, actual length = value + 1
    bool segmentMetadataFlag;   //!< 1 bit
    U8 seqNumLength;            //!< 3 bits, actual length = value + 1
    U64 sourceEntityId;         //!< Variable length
    U64 transactionSeqNum;      //!< Variable length
    U64 destinationEntityId;    //!< Variable length

    //! Compute the serialized size of this header in bytes
    U32 getSerializedSize() const {
        // 4 fixed bytes + entityIdLength+1 (source) + seqNumLength+1 (seq) + entityIdLength+1 (dest)
        return 4 + 2 * (static_cast<U32>(entityIdLength) + 1) + (static_cast<U32>(seqNumLength) + 1);
    }

    //! Serialize into a buffer. Returns number of bytes written, or 0 on error.
    U32 serialize(U8* buf, U32 bufSize) const;

    //! Deserialize from a buffer. Returns number of bytes consumed, or 0 on error.
    U32 deserialize(const U8* buf, U32 bufSize);
};

// -----------------------------------------------------------------------
// Metadata PDU (Section 5.2.5, Table 5-9)
// -----------------------------------------------------------------------

struct MetadataPdu {
    bool closureRequested;      //!< 1 bit
    U8 checksumType;            //!< 4 bits (0 = modular checksum)
    U32 fileSize;               //!< FSS (32-bit for small files)
    U8 sourceFileNameLen;       //!< LV length
    char sourceFileName[MAX_FILE_NAME_LEN + 1];
    U8 destFileNameLen;         //!< LV length
    char destFileName[MAX_FILE_NAME_LEN + 1];

    //! Serialize the metadata PDU data field (after directive code).
    //! Returns bytes written, or 0 on error.
    U32 serialize(U8* buf, U32 bufSize) const;

    //! Deserialize. Returns bytes consumed, or 0 on error.
    U32 deserialize(const U8* buf, U32 bufSize, bool largeFile);
};

// -----------------------------------------------------------------------
// File Data PDU (Section 5.3, Table 5-14)
// -----------------------------------------------------------------------

struct FileDataPdu {
    U32 offset;                 //!< FSS offset into file
    const U8* data;             //!< Pointer to file data (not owned)
    U32 dataSize;               //!< Size of file data

    //! Serialize. Returns bytes written, or 0 on error.
    U32 serialize(U8* buf, U32 bufSize) const;

    //! Deserialize from buffer. data pointer will point into buf.
    //! Returns bytes consumed, or 0 on error.
    U32 deserialize(const U8* buf, U32 bufSize, bool largeFile);
};

// -----------------------------------------------------------------------
// EOF PDU (Section 5.2.2, Table 5-6)
// -----------------------------------------------------------------------

struct EofPdu {
    ConditionCode conditionCode;  //!< 4 bits
    U32 fileChecksum;             //!< 32 bits
    U32 fileSize;                 //!< FSS (32-bit for small files)

    //! Serialize the EOF directive parameter field.
    U32 serialize(U8* buf, U32 bufSize) const;

    //! Deserialize. Returns bytes consumed, or 0 on error.
    U32 deserialize(const U8* buf, U32 bufSize, bool largeFile);
};

// -----------------------------------------------------------------------
// Finished PDU (Section 5.2.3, Table 5-7) - only for closure-requested
// -----------------------------------------------------------------------

struct FinishedPdu {
    ConditionCode conditionCode;
    DeliveryCode deliveryCode;
    FileStatus fileStatus;

    //! Serialize.
    U32 serialize(U8* buf, U32 bufSize) const;

    //! Deserialize.
    U32 deserialize(const U8* buf, U32 bufSize);
};

// -----------------------------------------------------------------------
// Utility: serialize a complete PDU (header + data field) into a Fw::Buffer
// -----------------------------------------------------------------------

//! Serialize a full CFDP PDU (header + directive code + directive params) into buf.
//! For file directive PDUs, directiveCode is included.
//! For file data PDUs, only the data field is appended.
//! Returns total bytes written, or 0 on error.
U32 serializeDirectivePdu(
    const PduHeader& header,
    DirectiveCode code,
    const U8* directiveParams,
    U32 directiveParamsLen,
    U8* buf,
    U32 bufSize
);

//! Serialize a file data PDU (header + file data content).
U32 serializeFileDataPdu(
    const PduHeader& header,
    const FileDataPdu& fileData,
    U8* buf,
    U32 bufSize
);

}  // namespace Cfdp
}  // namespace Svc

#endif
