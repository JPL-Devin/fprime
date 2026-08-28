// ======================================================================
// \title  CfdpPdu.cpp
// \author devin
// \brief  CFDP PDU serialization/deserialization per CCSDS 727.0-B-5
// ======================================================================

#include <Svc/Cfdp/Types/CfdpPdu.hpp>
#include <cstring>

namespace Svc {
namespace Cfdp {

// -----------------------------------------------------------------------
// Helper: serialize an unsigned integer into N bytes (big-endian)
// -----------------------------------------------------------------------
static void serializeUint(U8* buf, U64 value, U8 numBytes) {
    for (U8 i = 0; i < numBytes; i++) {
        buf[i] = static_cast<U8>((value >> (8 * (numBytes - 1 - i))) & 0xFF);
    }
}

// -----------------------------------------------------------------------
// Helper: deserialize an unsigned integer from N bytes (big-endian)
// -----------------------------------------------------------------------
static U64 deserializeUint(const U8* buf, U8 numBytes) {
    U64 value = 0;
    for (U8 i = 0; i < numBytes; i++) {
        value = (value << 8) | buf[i];
    }
    return value;
}

// -----------------------------------------------------------------------
// PduHeader
// -----------------------------------------------------------------------

U32 PduHeader::serialize(U8* buf, U32 bufSize) const {
    const U32 headerSize = this->getSerializedSize();
    if (bufSize < headerSize) {
        return 0;
    }

    // Byte 0: version(3) | pduType(1) | direction(1) | mode(1) | crcFlag(1) | largeFileFlag(1)
    buf[0] = static_cast<U8>(
        ((this->version & 0x07) << 5) |
        ((static_cast<U8>(this->pduType) & 0x01) << 4) |
        ((static_cast<U8>(this->direction) & 0x01) << 3) |
        ((static_cast<U8>(this->mode) & 0x01) << 2) |
        ((this->crcFlag ? 1 : 0) << 1) |
        (this->largeFileFlag ? 1 : 0)
    );

    // Bytes 1-2: PDU data field length (16 bits big-endian)
    buf[1] = static_cast<U8>((this->dataFieldLength >> 8) & 0xFF);
    buf[2] = static_cast<U8>(this->dataFieldLength & 0xFF);

    // Byte 3: segCtrl(1) | entityIdLen(3) | segMetaFlag(1) | seqNumLen(3)
    buf[3] = static_cast<U8>(
        ((this->segmentationControl ? 1 : 0) << 7) |
        ((this->entityIdLength & 0x07) << 4) |
        ((this->segmentMetadataFlag ? 1 : 0) << 3) |
        (this->seqNumLength & 0x07)
    );

    U32 offset = 4;
    const U8 eidLen = this->entityIdLength + 1;
    const U8 seqLen = this->seqNumLength + 1;

    // Source entity ID
    serializeUint(buf + offset, this->sourceEntityId, eidLen);
    offset += eidLen;

    // Transaction sequence number
    serializeUint(buf + offset, this->transactionSeqNum, seqLen);
    offset += seqLen;

    // Destination entity ID
    serializeUint(buf + offset, this->destinationEntityId, eidLen);
    offset += eidLen;

    return offset;
}

U32 PduHeader::deserialize(const U8* buf, U32 bufSize) {
    if (bufSize < 4) {
        return 0;
    }

    // Byte 0
    this->version = (buf[0] >> 5) & 0x07;
    this->pduType = static_cast<PduType>((buf[0] >> 4) & 0x01);
    this->direction = static_cast<Direction>((buf[0] >> 3) & 0x01);
    this->mode = static_cast<TransmissionMode>((buf[0] >> 2) & 0x01);
    this->crcFlag = ((buf[0] >> 1) & 0x01) != 0;
    this->largeFileFlag = (buf[0] & 0x01) != 0;

    // Bytes 1-2
    this->dataFieldLength = static_cast<U16>((static_cast<U16>(buf[1]) << 8) | buf[2]);

    // Byte 3
    this->segmentationControl = ((buf[3] >> 7) & 0x01) != 0;
    this->entityIdLength = (buf[3] >> 4) & 0x07;
    this->segmentMetadataFlag = ((buf[3] >> 3) & 0x01) != 0;
    this->seqNumLength = buf[3] & 0x07;

    const U8 eidLen = this->entityIdLength + 1;
    const U8 seqLen = this->seqNumLength + 1;
    const U32 headerSize = 4 + 2 * static_cast<U32>(eidLen) + static_cast<U32>(seqLen);

    if (bufSize < headerSize) {
        return 0;
    }

    U32 offset = 4;

    this->sourceEntityId = deserializeUint(buf + offset, eidLen);
    offset += eidLen;

    this->transactionSeqNum = deserializeUint(buf + offset, seqLen);
    offset += seqLen;

    this->destinationEntityId = deserializeUint(buf + offset, eidLen);
    offset += eidLen;

    return offset;
}

// -----------------------------------------------------------------------
// MetadataPdu
// -----------------------------------------------------------------------

U32 MetadataPdu::serialize(U8* buf, U32 bufSize) const {
    // byte 0: reserved(1) | closureRequested(1) | reserved(2) | checksumType(4)
    // bytes 1..4: fileSize (FSS 32-bit)
    // LV: source file name
    // LV: dest file name
    const U32 needed = 1 + 4 + 1 + this->sourceFileNameLen + 1 + this->destFileNameLen;
    if (bufSize < needed) {
        return 0;
    }

    U32 offset = 0;

    buf[offset] = static_cast<U8>(
        ((this->closureRequested ? 1 : 0) << 6) |
        (this->checksumType & 0x0F)
    );
    offset++;

    // File size (32-bit big-endian)
    serializeUint(buf + offset, this->fileSize, 4);
    offset += 4;

    // Source file name LV
    buf[offset] = this->sourceFileNameLen;
    offset++;
    if (this->sourceFileNameLen > 0) {
        (void)std::memcpy(buf + offset, this->sourceFileName, this->sourceFileNameLen);
        offset += this->sourceFileNameLen;
    }

    // Dest file name LV
    buf[offset] = this->destFileNameLen;
    offset++;
    if (this->destFileNameLen > 0) {
        (void)std::memcpy(buf + offset, this->destFileName, this->destFileNameLen);
        offset += this->destFileNameLen;
    }

    return offset;
}

U32 MetadataPdu::deserialize(const U8* buf, U32 bufSize, bool largeFile) {
    (void)largeFile;  // We only support small files for now

    if (bufSize < 6) {  // minimum: 1 byte flags + 4 bytes filesize + 1 byte src LV length
        return 0;
    }

    U32 offset = 0;

    this->closureRequested = ((buf[offset] >> 6) & 0x01) != 0;
    this->checksumType = buf[offset] & 0x0F;
    offset++;

    this->fileSize = static_cast<U32>(deserializeUint(buf + offset, 4));
    offset += 4;

    // Source file name LV
    if (offset >= bufSize) {
        return 0;
    }
    this->sourceFileNameLen = buf[offset];
    offset++;
    if (this->sourceFileNameLen > MAX_FILE_NAME_LEN || offset + this->sourceFileNameLen > bufSize) {
        return 0;
    }
    if (this->sourceFileNameLen > 0) {
        (void)std::memcpy(this->sourceFileName, buf + offset, this->sourceFileNameLen);
        offset += this->sourceFileNameLen;
    }
    this->sourceFileName[this->sourceFileNameLen] = '\0';

    // Dest file name LV
    if (offset >= bufSize) {
        return 0;
    }
    this->destFileNameLen = buf[offset];
    offset++;
    if (this->destFileNameLen > MAX_FILE_NAME_LEN || offset + this->destFileNameLen > bufSize) {
        return 0;
    }
    if (this->destFileNameLen > 0) {
        (void)std::memcpy(this->destFileName, buf + offset, this->destFileNameLen);
        offset += this->destFileNameLen;
    }
    this->destFileName[this->destFileNameLen] = '\0';

    return offset;
}

// -----------------------------------------------------------------------
// FileDataPdu
// -----------------------------------------------------------------------

U32 FileDataPdu::serialize(U8* buf, U32 bufSize) const {
    // offset (FSS 32-bit) + file data
    const U32 needed = 4 + this->dataSize;
    if (bufSize < needed) {
        return 0;
    }

    U32 off = 0;
    serializeUint(buf + off, this->offset, 4);
    off += 4;

    if (this->dataSize > 0 && this->data != nullptr) {
        (void)std::memcpy(buf + off, this->data, this->dataSize);
        off += this->dataSize;
    }

    return off;
}

U32 FileDataPdu::deserialize(const U8* buf, U32 bufSize, bool largeFile) {
    (void)largeFile;
    if (bufSize < 4) {
        return 0;
    }

    this->offset = static_cast<U32>(deserializeUint(buf, 4));
    this->data = buf + 4;
    this->dataSize = bufSize - 4;

    return bufSize;
}

// -----------------------------------------------------------------------
// EofPdu
// -----------------------------------------------------------------------

U32 EofPdu::serialize(U8* buf, U32 bufSize) const {
    // 1 byte (condCode|spare) + 4 bytes checksum + 4 bytes fileSize = 9
    if (bufSize < 9) {
        return 0;
    }

    U32 offset = 0;

    buf[offset] = static_cast<U8>((static_cast<U8>(this->conditionCode) & 0x0F) << 4);
    offset++;

    serializeUint(buf + offset, this->fileChecksum, 4);
    offset += 4;

    serializeUint(buf + offset, this->fileSize, 4);
    offset += 4;

    return offset;
}

U32 EofPdu::deserialize(const U8* buf, U32 bufSize, bool largeFile) {
    (void)largeFile;
    if (bufSize < 9) {
        return 0;
    }

    U32 offset = 0;

    this->conditionCode = static_cast<ConditionCode>((buf[offset] >> 4) & 0x0F);
    offset++;

    this->fileChecksum = static_cast<U32>(deserializeUint(buf + offset, 4));
    offset += 4;

    this->fileSize = static_cast<U32>(deserializeUint(buf + offset, 4));
    offset += 4;

    return offset;
}

// -----------------------------------------------------------------------
// FinishedPdu
// -----------------------------------------------------------------------

U32 FinishedPdu::serialize(U8* buf, U32 bufSize) const {
    if (bufSize < 1) {
        return 0;
    }

    // conditionCode(4) | spare(1) | deliveryCode(1) | fileStatus(2)
    buf[0] = static_cast<U8>(
        ((static_cast<U8>(this->conditionCode) & 0x0F) << 4) |
        ((static_cast<U8>(this->deliveryCode) & 0x01) << 2) |
        (static_cast<U8>(this->fileStatus) & 0x03)
    );

    return 1;
}

U32 FinishedPdu::deserialize(const U8* buf, U32 bufSize) {
    if (bufSize < 1) {
        return 0;
    }

    this->conditionCode = static_cast<ConditionCode>((buf[0] >> 4) & 0x0F);
    this->deliveryCode = static_cast<DeliveryCode>((buf[0] >> 2) & 0x01);
    this->fileStatus = static_cast<FileStatus>(buf[0] & 0x03);

    return 1;
}

// -----------------------------------------------------------------------
// Utility: serializeDirectivePdu
// -----------------------------------------------------------------------

U32 serializeDirectivePdu(
    const PduHeader& header,
    DirectiveCode code,
    const U8* directiveParams,
    U32 directiveParamsLen,
    U8* buf,
    U32 bufSize
) {
    const U32 headerSize = header.getSerializedSize();
    // data field = 1 byte directive code + params
    const U32 totalSize = headerSize + 1 + directiveParamsLen;
    if (bufSize < totalSize) {
        return 0;
    }

    U32 offset = header.serialize(buf, bufSize);
    if (offset == 0) {
        return 0;
    }

    // Directive code
    buf[offset] = static_cast<U8>(code);
    offset++;

    // Directive parameters
    if (directiveParamsLen > 0 && directiveParams != nullptr) {
        (void)std::memcpy(buf + offset, directiveParams, directiveParamsLen);
        offset += directiveParamsLen;
    }

    return offset;
}

// -----------------------------------------------------------------------
// Utility: serializeFileDataPdu
// -----------------------------------------------------------------------

U32 serializeFileDataPdu(
    const PduHeader& header,
    const FileDataPdu& fileData,
    U8* buf,
    U32 bufSize
) {
    const U32 headerSize = header.getSerializedSize();
    const U32 dataFieldSize = 4 + fileData.dataSize;  // FSS offset + data
    const U32 totalSize = headerSize + dataFieldSize;
    if (bufSize < totalSize) {
        return 0;
    }

    U32 offset = header.serialize(buf, bufSize);
    if (offset == 0) {
        return 0;
    }

    U32 dataBytes = fileData.serialize(buf + offset, bufSize - offset);
    if (dataBytes == 0) {
        return 0;
    }
    offset += dataBytes;

    return offset;
}

}  // namespace Cfdp
}  // namespace Svc
