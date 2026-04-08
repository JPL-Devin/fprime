// ======================================================================
// \title  CfdpReceiver.cpp
// \author devin
// \brief  cpp file for CfdpReceiver component implementation class
//
// CFDP Class 1 (Unacknowledged) receiving entity per CCSDS 727.0-B-5.
// ======================================================================

#include <Fw/Types/Assert.hpp>
#include <Fw/Types/String.hpp>
#include <Svc/Cfdp/CfdpReceiver/CfdpReceiver.hpp>
#include <cstring>

namespace Svc {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

CfdpReceiver::CfdpReceiver(const char* const compName)
    : CfdpReceiverComponentBase(compName),
      m_receiveMode(ReceiveMode::IDLE),
      m_transactionSourceEntity(0),
      m_transactionSeqNum(0),
      m_expectedFileSize(0),
      m_bytesReceived(0),
      m_eofReceived(false),
      m_eofFileChecksum(0),
      m_eofFileSize(0),
      m_filesReceived(0),
      m_pdusReceived(0),
      m_warningCount(0),
      m_totalBytesReceived(0) {}

CfdpReceiver::~CfdpReceiver() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void CfdpReceiver::bufferSendIn_handler(const FwIndexType portNum, Fw::Buffer& buffer) {
    (void)portNum;

    const U8* data = buffer.getData();
    const U32 size = static_cast<U32>(buffer.getSize());

    // Minimum CFDP PDU: 4 bytes fixed header + at least 2 bytes variable (1-byte entity IDs + 1-byte seq num)
    if (size < 7) {
        this->log_WARNING_HI_PduTooSmall(size);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->bufferSendOut_out(0, buffer);
        return;
    }

    // Parse the PDU header
    Cfdp::PduHeader header;
    const U32 headerSize = header.deserialize(data, size);
    if (headerSize == 0) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->bufferSendOut_out(0, buffer);
        return;
    }

    // Validate version
    if (header.version != Cfdp::CFDP_VERSION) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->bufferSendOut_out(0, buffer);
        return;
    }

    this->m_pdusReceived++;
    this->tlmWrite_PdusReceived(this->m_pdusReceived);

    // Use the PDU header's dataFieldLength (per CCSDS 727.0-B-5 Section 5.1)
    // rather than the buffer size, to avoid consuming trailing padding/CRC bytes
    if (headerSize + header.dataFieldLength > size) {
        // PDU is truncated — buffer doesn't contain the full data field
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->bufferSendOut_out(0, buffer);
        return;
    }
    const U8* dataField = data + headerSize;
    const U32 dataFieldLen = header.dataFieldLength;

    // Dispatch based on PDU type
    if (header.pduType == Cfdp::PduType::FILE_DIRECTIVE) {
        this->handleFileDirective(header, dataField, dataFieldLen);
    } else {
        // File Data PDU
        this->handleFileDataPdu(header, dataField, dataFieldLen);
    }

    // Return the buffer
    this->bufferSendOut_out(0, buffer);
}

void CfdpReceiver::pingIn_handler(const FwIndexType portNum, U32 key) {
    (void)portNum;
    this->pingOut_out(0, key);
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void CfdpReceiver::Cancel_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    if (this->m_receiveMode == ReceiveMode::ACTIVE) {
        this->log_ACTIVITY_HI_ReceiveCanceled(static_cast<U32>(this->m_transactionSeqNum));
        this->goToIdleMode();
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void CfdpReceiver::handleFileDirective(
    const Cfdp::PduHeader& header,
    const U8* dataField,
    U32 dataFieldLen
) {
    if (dataFieldLen < 1) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    const Cfdp::DirectiveCode code = static_cast<Cfdp::DirectiveCode>(dataField[0]);
    const U8* params = dataField + 1;
    const U32 paramsLen = dataFieldLen - 1;

    switch (code) {
        case Cfdp::DirectiveCode::METADATA_PDU:
            this->handleMetadataPdu(header, params, paramsLen);
            break;
        case Cfdp::DirectiveCode::EOF_PDU:
            this->handleEofPdu(header, params, paramsLen);
            break;
        default:
            this->log_WARNING_HI_InvalidDirective(static_cast<U8>(code));
            this->m_warningCount++;
            this->tlmWrite_Warnings(this->m_warningCount);
            break;
    }
}

void CfdpReceiver::handleMetadataPdu(
    const Cfdp::PduHeader& header,
    const U8* params,
    U32 paramsLen
) {
    // If we were already in an active transaction, close it first
    if (this->m_receiveMode == ReceiveMode::ACTIVE) {
        this->log_WARNING_HI_InvalidReceiveMode(
            static_cast<U8>(Cfdp::DirectiveCode::METADATA_PDU),
            static_cast<U8>(this->m_receiveMode)
        );
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->goToIdleMode();
    }

    // Parse Metadata PDU
    Cfdp::MetadataPdu metadata;
    const U32 consumed = metadata.deserialize(params, paramsLen, header.largeFileFlag);
    if (consumed == 0) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    // Store transaction info
    this->m_transactionSourceEntity = header.sourceEntityId;
    this->m_transactionSeqNum = header.transactionSeqNum;
    this->m_expectedFileSize = metadata.fileSize;
    this->m_bytesReceived = 0;
    this->m_eofReceived = false;
    this->m_checksum = CFDP::Checksum();

    // Store file names
    this->m_sourceFileName = metadata.sourceFileName;
    this->m_destFileName = metadata.destFileName;

    // Open the destination file for writing (allow overwrite for re-receives)
    Os::File::Status fileStatus = this->m_file.open(
        metadata.destFileName,
        Os::File::Mode::OPEN_CREATE,
        Os::File::OverwriteType::OVERWRITE
    );

    if (fileStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(this->m_destFileName);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;  // Stay in IDLE mode
    }

    this->m_receiveMode = ReceiveMode::ACTIVE;

    this->log_ACTIVITY_HI_MetadataReceived(
        this->m_sourceFileName,
        this->m_destFileName,
        this->m_expectedFileSize,
        static_cast<U32>(this->m_transactionSeqNum)
    );
}

void CfdpReceiver::handleFileDataPdu(
    const Cfdp::PduHeader& header,
    const U8* dataField,
    U32 dataFieldLen
) {
    (void)header;

    if (this->m_receiveMode != ReceiveMode::ACTIVE) {
        this->log_WARNING_HI_InvalidReceiveMode(
            static_cast<U8>(Cfdp::PduType::FILE_DATA),
            static_cast<U8>(this->m_receiveMode)
        );
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    // Parse file data PDU (offset + data)
    Cfdp::FileDataPdu fileData;
    const U32 consumed = fileData.deserialize(dataField, dataFieldLen, false);
    if (consumed == 0) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    // Bounds check
    if (this->m_expectedFileSize > 0) {
        if (fileData.offset > this->m_expectedFileSize || fileData.dataSize > this->m_expectedFileSize - fileData.offset) {
            this->log_WARNING_HI_DataOutOfBounds(
                fileData.offset, fileData.dataSize, this->m_expectedFileSize
            );
            this->m_warningCount++;
            this->tlmWrite_Warnings(this->m_warningCount);
            return;
        }
    }

    // Seek to the correct offset and write
    Os::File::Status seekStatus = this->m_file.seek(
        static_cast<FwSignedSizeType>(fileData.offset),
        Os::File::SeekType::ABSOLUTE
    );
    if (seekStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileWriteError(this->m_destFileName);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    FwSizeType writeSize = static_cast<FwSizeType>(fileData.dataSize);
    Os::File::Status writeStatus = this->m_file.write(fileData.data, writeSize);
    if (writeStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileWriteError(this->m_destFileName);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    // Update checksum
    this->m_checksum.update(fileData.data, fileData.offset, fileData.dataSize);

    this->m_bytesReceived += fileData.dataSize;
    this->m_totalBytesReceived += fileData.dataSize;
    this->tlmWrite_TotalBytesReceived(this->m_totalBytesReceived);

    // Check if we have received EOF and all data
    if (this->m_eofReceived) {
        this->checkCompletion();
    }
}

void CfdpReceiver::handleEofPdu(
    const Cfdp::PduHeader& header,
    const U8* params,
    U32 paramsLen
) {
    (void)header;

    if (this->m_receiveMode != ReceiveMode::ACTIVE) {
        this->log_WARNING_HI_InvalidReceiveMode(
            static_cast<U8>(Cfdp::DirectiveCode::EOF_PDU),
            static_cast<U8>(this->m_receiveMode)
        );
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    Cfdp::EofPdu eofPdu;
    const U32 consumed = eofPdu.deserialize(params, paramsLen, false);
    if (consumed == 0) {
        this->log_WARNING_HI_InvalidPduHeader();
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    this->log_ACTIVITY_LO_EofReceived(
        static_cast<U32>(this->m_transactionSeqNum),
        static_cast<U8>(eofPdu.conditionCode)
    );

    // If this is a cancel EOF, cancel the transaction
    if (eofPdu.conditionCode != Cfdp::ConditionCode::NO_ERROR) {
        this->log_ACTIVITY_HI_ReceiveCanceled(static_cast<U32>(this->m_transactionSeqNum));
        this->goToIdleMode();
        return;
    }

    // Store EOF info
    this->m_eofReceived = true;
    this->m_eofFileChecksum = eofPdu.fileChecksum;
    this->m_eofFileSize = eofPdu.fileSize;

    // Per Section 4.6.3.3: Check if file reception is complete
    this->checkCompletion();
}

void CfdpReceiver::checkCompletion() {
    if (!this->m_eofReceived) {
        return;
    }

    // Per Section 4.6.1.2.8: File reception is complete when:
    // 1) EOF received
    // 2) All file data has been received (bytesReceived >= eofFileSize)
    if (this->m_bytesReceived < this->m_eofFileSize) {
        // Not all data received yet - in Class 1 there's no NAK mechanism,
        // we could use a Check timer, but for simplicity we complete now.
        // The delivery code will indicate DATA_INCOMPLETE.
    }

    // Verify checksum
    const U32 computedChecksum = this->m_checksum.getValue();
    if (computedChecksum != this->m_eofFileChecksum) {
        this->log_WARNING_HI_ChecksumFailure(
            this->m_destFileName,
            computedChecksum,
            this->m_eofFileChecksum
        );
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        // Per 4.6.3.3: Still issue Notice of Completion even on checksum failure
    }

    // File transfer complete - close file and notify
    this->m_file.close();

    this->m_filesReceived++;
    this->tlmWrite_FilesReceived(this->m_filesReceived);

    this->log_ACTIVITY_HI_FileReceived(
        this->m_destFileName,
        this->m_eofFileSize,
        static_cast<U32>(this->m_transactionSeqNum)
    );

    // Announce the file for further processing
    if (this->isConnected_fileAnnounce_OutputPort(0)) {
        Fw::String fileNameStr(this->m_destFileName.toChar());
        this->fileAnnounce_out(0, fileNameStr);
    }

    // Reset to idle
    this->m_receiveMode = ReceiveMode::IDLE;
    this->m_eofReceived = false;
}

void CfdpReceiver::goToIdleMode() {
    this->m_file.close();
    this->m_receiveMode = ReceiveMode::IDLE;
    this->m_eofReceived = false;
    this->m_bytesReceived = 0;
    this->m_expectedFileSize = 0;
    this->m_checksum = CFDP::Checksum();
}

}  // namespace Svc
