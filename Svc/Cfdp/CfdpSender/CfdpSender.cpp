// ======================================================================
// \title  CfdpSender.cpp
// \author devin
// \brief  cpp file for CfdpSender component implementation class
//
// CFDP Class 1 (Unacknowledged) sending entity per CCSDS 727.0-B-5.
// ======================================================================

#include <Fw/Types/Assert.hpp>
#include <Svc/Cfdp/CfdpSender/CfdpSender.hpp>
#include <cstring>
#include <limits>

namespace Svc {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

CfdpSender::CfdpSender(const char* const compName)
    : CfdpSenderComponentBase(compName),
      m_configured(false),
      m_localEntityId(0),
      m_remoteEntityId(0),
      m_pduBufferSize(512),
      m_mode(Mode::IDLE),
      m_transactionSeqNum(0),
      m_curEntry(),
      m_fileSize(0U),
      m_byteOffset(0),
      m_endOffset(0),
      m_filesSent(0),
      m_pdusSent(0),
      m_warningCount(0),
      m_totalBytesSent(0),
      m_cntxId(0) {}

CfdpSender::~CfdpSender() {}

void CfdpSender::configure(
    U64 localEntityId,
    U64 remoteEntityId,
    U32 pduBufferSize
) {
    this->m_localEntityId = localEntityId;
    this->m_remoteEntityId = remoteEntityId;
    this->m_pduBufferSize = pduBufferSize;
    this->m_configured = true;
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void CfdpSender::Run_handler(const FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    this->m_modeMutex.lock();
    const Mode currentMode = this->m_mode;
    this->m_modeMutex.unLock();

    switch (currentMode) {
        case Mode::IDLE:
            // Nothing to do
            break;
        case Mode::METADATA:
            this->sendMetadataPdu();
            break;
        case Mode::DATA:
            this->sendFileDataPdu();
            break;
        case Mode::EOF_PDU:
            this->sendEofPdu();
            break;
        case Mode::CANCEL:
            this->sendCancelEofPdu();
            break;
        case Mode::WAIT:
            // Waiting for buffer return, nothing to do
            break;
    }
}

Svc::SendFileResponse CfdpSender::SendFile_handler(
    const FwIndexType portNum,
    const Fw::StringBase& sourceFilename,
    const Fw::StringBase& destFilename,
    U32 offset,
    U32 length
) {
    (void)portNum;

    // Check if already busy
    this->m_modeMutex.lock();
    const bool busy = (this->m_mode != Mode::IDLE);
    this->m_modeMutex.unLock();

    if (busy) {
        return SendFileResponse(SendFileStatus::STATUS_BUSY, std::numeric_limits<U32>::max());
    }

    // Guard against filename overflow
    if (sourceFilename.length() >= this->m_curEntry.srcFilename.getCapacity()) {
        this->log_WARNING_HI_SourceFilenameOverflow();
        return SendFileResponse(SendFileStatus::STATUS_ERROR, std::numeric_limits<U32>::max());
    }
    if (destFilename.length() >= this->m_curEntry.destFilename.getCapacity()) {
        this->log_WARNING_HI_DestFilenameOverflow();
        return SendFileResponse(SendFileStatus::STATUS_ERROR, std::numeric_limits<U32>::max());
    }

    this->m_curEntry.srcFilename = sourceFilename;
    this->m_curEntry.destFilename = destFilename;
    this->m_curEntry.offset = offset;
    this->m_curEntry.length = length;
    this->m_curEntry.source = CallerSource::PORT;
    this->m_curEntry.opCode = 0;
    this->m_curEntry.cmdSeq = 0;
    this->m_curEntry.context = this->m_cntxId++;

    this->startTransfer();

    if (this->m_mode == Mode::IDLE) {
        // startTransfer failed
        return SendFileResponse(SendFileStatus::STATUS_ERROR, std::numeric_limits<U32>::max());
    }

    return SendFileResponse(SendFileStatus::STATUS_OK, this->m_curEntry.context);
}

void CfdpSender::bufferReturn_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) {
    (void)portNum;
    (void)fwBuffer;
    // In Class 1, we don't require strict flow control on buffer returns.
    // The Run handler drives the state machine. If we were in WAIT mode,
    // transition back to the appropriate sending mode.
    this->m_modeMutex.lock();
    if (this->m_mode == Mode::WAIT) {
        if (this->m_byteOffset < this->m_endOffset) {
            this->m_mode = Mode::DATA;
        } else {
            this->m_mode = Mode::EOF_PDU;
        }
    }
    this->m_modeMutex.unLock();
}

void CfdpSender::pingIn_handler(const FwIndexType portNum, U32 key) {
    (void)portNum;
    this->pingOut_out(0, key);
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void CfdpSender::SendFile_cmdHandler(
    const FwOpcodeType opCode,
    const U32 cmdSeq,
    const Fw::CmdStringArg& sourceFilename,
    const Fw::CmdStringArg& destFilename
) {
    // Check busy
    this->m_modeMutex.lock();
    const bool busy = (this->m_mode != Mode::IDLE);
    this->m_modeMutex.unLock();

    if (busy) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }

    // Guard filename overflow
    if (sourceFilename.length() >= this->m_curEntry.srcFilename.getCapacity()) {
        this->log_WARNING_HI_SourceFilenameOverflow();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (destFilename.length() >= this->m_curEntry.destFilename.getCapacity()) {
        this->log_WARNING_HI_DestFilenameOverflow();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->m_curEntry.srcFilename = sourceFilename;
    this->m_curEntry.destFilename = destFilename;
    this->m_curEntry.offset = 0;
    this->m_curEntry.length = 0;
    this->m_curEntry.source = CallerSource::COMMAND;
    this->m_curEntry.opCode = opCode;
    this->m_curEntry.cmdSeq = cmdSeq;
    this->m_curEntry.context = std::numeric_limits<U32>::max();

    this->startTransfer();

    if (this->m_mode == Mode::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void CfdpSender::Cancel_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
    this->m_modeMutex.lock();
    if (this->m_mode != Mode::IDLE) {
        this->m_mode = Mode::CANCEL;
    }
    this->m_modeMutex.unLock();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CfdpSender::SendPartial_cmdHandler(
    FwOpcodeType opCode,
    U32 cmdSeq,
    const Fw::CmdStringArg& sourceFilename,
    const Fw::CmdStringArg& destFilename,
    U32 startOffset,
    U32 length
) {
    // Check busy
    this->m_modeMutex.lock();
    const bool busy = (this->m_mode != Mode::IDLE);
    this->m_modeMutex.unLock();

    if (busy) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }

    // Guard filename overflow
    if (sourceFilename.length() >= this->m_curEntry.srcFilename.getCapacity()) {
        this->log_WARNING_HI_SourceFilenameOverflow();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (destFilename.length() >= this->m_curEntry.destFilename.getCapacity()) {
        this->log_WARNING_HI_DestFilenameOverflow();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->m_curEntry.srcFilename = sourceFilename;
    this->m_curEntry.destFilename = destFilename;
    this->m_curEntry.offset = startOffset;
    this->m_curEntry.length = length;
    this->m_curEntry.source = CallerSource::COMMAND;
    this->m_curEntry.opCode = opCode;
    this->m_curEntry.cmdSeq = cmdSeq;
    this->m_curEntry.context = std::numeric_limits<U32>::max();

    this->startTransfer();

    if (this->m_mode == Mode::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void CfdpSender::startTransfer() {
    FW_ASSERT(this->m_configured);

    // Open the file
    Os::File::Status fileStatus = this->m_file.open(
        this->m_curEntry.srcFilename.toChar(),
        Os::File::Mode::OPEN_READ
    );

    if (fileStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(this->m_curEntry.srcFilename);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    // Get file size
    FwSizeType fileSizeOut = 0;
    Os::FileSystem::Status fsStatus = Os::FileSystem::getFileSize(
        this->m_curEntry.srcFilename.toChar(),
        fileSizeOut
    );
    if (fsStatus != Os::FileSystem::OP_OK) {
        this->log_WARNING_HI_FileOpenError(this->m_curEntry.srcFilename);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->m_file.close();
        return;
    }
    this->m_fileSize = fileSizeOut;

    // Validate parameters
    if (this->m_fileSize == 0U) {
        this->log_WARNING_HI_ZeroSizeFile(this->m_curEntry.srcFilename);
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->m_file.close();
        return;
    }

    if (static_cast<FwSizeType>(this->m_curEntry.offset) >= this->m_fileSize) {
        this->log_WARNING_HI_PartialSendError(
            this->m_curEntry.srcFilename,
            this->m_curEntry.offset,
            static_cast<U32>(this->m_fileSize)
        );
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->m_file.close();
        return;
    }

    // Set up byte range
    this->m_byteOffset = this->m_curEntry.offset;
    if (this->m_curEntry.length > 0) {
        this->m_endOffset = this->m_curEntry.offset + this->m_curEntry.length;
        if (this->m_endOffset > static_cast<U32>(this->m_fileSize)) {
            this->m_endOffset = static_cast<U32>(this->m_fileSize);
        }
    } else {
        this->m_endOffset = static_cast<U32>(this->m_fileSize);
    }

    // Reset checksum
    this->m_checksum = CFDP::Checksum();

    // Increment transaction sequence number
    this->m_transactionSeqNum++;
    this->tlmWrite_TransactionSeqNum(this->m_transactionSeqNum);

    // Enter METADATA mode - the Run handler will send it
    this->m_modeMutex.lock();
    this->m_mode = Mode::METADATA;
    this->m_modeMutex.unLock();

    this->log_ACTIVITY_HI_SendStarted(
        this->m_endOffset - this->m_byteOffset,
        this->m_curEntry.srcFilename,
        this->m_curEntry.destFilename,
        this->m_transactionSeqNum
    );
}

void CfdpSender::fillPduHeader(
    Cfdp::PduHeader& header,
    Cfdp::PduType pduType,
    U16 dataFieldLength
) const {
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = pduType;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;  // Class 1
    header.crcFlag = false;  // No CRC for now
    header.largeFileFlag = false;  // Small files only
    header.dataFieldLength = dataFieldLength;
    header.segmentationControl = false;
    // Use 2-byte entity IDs (entityIdLength = 1 means 2 bytes)
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    // Use 2-byte sequence numbers
    header.seqNumLength = 1;
    header.sourceEntityId = this->m_localEntityId;
    header.transactionSeqNum = this->m_transactionSeqNum;
    header.destinationEntityId = this->m_remoteEntityId;
}

void CfdpSender::sendMetadataPdu() {
    // Build Metadata PDU per Section 5.2.5
    Cfdp::MetadataPdu metadata;
    metadata.closureRequested = false;  // Class 1, no closure
    metadata.checksumType = 0;  // Modular checksum (type 0)
    metadata.fileSize = this->m_endOffset - this->m_byteOffset;

    const U32 srcLen = static_cast<U32>(std::strlen(this->m_curEntry.srcFilename.toChar()));
    const U32 dstLen = static_cast<U32>(std::strlen(this->m_curEntry.destFilename.toChar()));
    metadata.sourceFileNameLen = static_cast<U8>(srcLen > Cfdp::MAX_FILE_NAME_LEN ? Cfdp::MAX_FILE_NAME_LEN : srcLen);
    (void)std::memcpy(metadata.sourceFileName, this->m_curEntry.srcFilename.toChar(), metadata.sourceFileNameLen);
    metadata.sourceFileName[metadata.sourceFileNameLen] = '\0';

    metadata.destFileNameLen = static_cast<U8>(dstLen > Cfdp::MAX_FILE_NAME_LEN ? Cfdp::MAX_FILE_NAME_LEN : dstLen);
    (void)std::memcpy(metadata.destFileName, this->m_curEntry.destFilename.toChar(), metadata.destFileNameLen);
    metadata.destFileName[metadata.destFileNameLen] = '\0';

    // Serialize metadata parameters
    U8 paramBuf[512];
    const U32 paramLen = metadata.serialize(paramBuf, sizeof(paramBuf));
    FW_ASSERT(paramLen > 0);

    // Data field length = 1 (directive code) + paramLen
    const U16 dataFieldLen = static_cast<U16>(1 + paramLen);

    // Build PDU header
    Cfdp::PduHeader header;
    this->fillPduHeader(header, Cfdp::PduType::FILE_DIRECTIVE, dataFieldLen);

    // Serialize full PDU
    const U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::METADATA_PDU,
        paramBuf, paramLen,
        this->m_serializeBuffer, PDU_SERIALIZE_BUFFER_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->transmitPdu(this->m_serializeBuffer, totalLen);

    // Transition to DATA mode
    this->m_modeMutex.lock();
    this->m_mode = Mode::DATA;
    this->m_modeMutex.unLock();
}

void CfdpSender::sendFileDataPdu() {
    if (this->m_byteOffset >= this->m_endOffset) {
        // All data sent, move to EOF
        this->m_modeMutex.lock();
        this->m_mode = Mode::EOF_PDU;
        this->m_modeMutex.unLock();
        return;
    }

    // Calculate data size for this PDU
    // Reserve space for: PDU header (max ~12 bytes) + FSS offset (4 bytes)
    const U32 headerOverhead = 12 + 4;  // Conservative estimate
    U32 maxDataSize = this->m_pduBufferSize > headerOverhead
                      ? this->m_pduBufferSize - headerOverhead
                      : 256;
    if (maxDataSize > Cfdp::MAX_FILE_DATA_SIZE) {
        maxDataSize = Cfdp::MAX_FILE_DATA_SIZE;
    }

    U32 dataSize = this->m_endOffset - this->m_byteOffset;
    if (dataSize > maxDataSize) {
        dataSize = maxDataSize;
    }

    // Read file data
    U8 fileData[Cfdp::MAX_FILE_DATA_SIZE];
    FwSizeType readSize = static_cast<FwSizeType>(dataSize);

    // Seek to correct position
    Os::File::Status seekStatus = this->m_file.seek(
        static_cast<FwSignedSizeType>(this->m_byteOffset),
        Os::File::SeekType::ABSOLUTE
    );
    if (seekStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileReadError(this->m_curEntry.srcFilename, static_cast<I32>(seekStatus));
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->finishTransfer(true);
        return;
    }

    Os::File::Status readStatus = this->m_file.read(fileData, readSize);
    if (readStatus != Os::File::OP_OK || readSize == 0) {
        this->log_WARNING_HI_FileReadError(this->m_curEntry.srcFilename, static_cast<I32>(readStatus));
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        this->finishTransfer(true);
        return;
    }

    const U32 actualDataSize = static_cast<U32>(readSize);

    // Update checksum
    this->m_checksum.update(fileData, this->m_byteOffset, actualDataSize);

    // Build File Data PDU
    Cfdp::FileDataPdu fileDataPdu;
    fileDataPdu.offset = this->m_byteOffset;
    fileDataPdu.data = fileData;
    fileDataPdu.dataSize = actualDataSize;

    // Data field length = 4 (FSS offset) + actualDataSize
    const U16 dataFieldLen = static_cast<U16>(4 + actualDataSize);

    Cfdp::PduHeader header;
    this->fillPduHeader(header, Cfdp::PduType::FILE_DATA, dataFieldLen);

    const U32 totalLen = Cfdp::serializeFileDataPdu(
        header, fileDataPdu,
        this->m_serializeBuffer, PDU_SERIALIZE_BUFFER_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->transmitPdu(this->m_serializeBuffer, totalLen);

    this->m_byteOffset += actualDataSize;
    this->m_totalBytesSent += actualDataSize;
    this->tlmWrite_TotalBytesSent(this->m_totalBytesSent);

    // If all data sent, transition to EOF
    if (this->m_byteOffset >= this->m_endOffset) {
        this->m_modeMutex.lock();
        this->m_mode = Mode::EOF_PDU;
        this->m_modeMutex.unLock();
    }
}

void CfdpSender::sendEofPdu() {
    // Build EOF PDU per Section 5.2.2
    Cfdp::EofPdu eofPdu;
    eofPdu.conditionCode = Cfdp::ConditionCode::NO_ERROR;
    eofPdu.fileChecksum = this->m_checksum.getValue();
    eofPdu.fileSize = this->m_endOffset - this->m_curEntry.offset;

    U8 paramBuf[16];
    const U32 paramLen = eofPdu.serialize(paramBuf, sizeof(paramBuf));
    FW_ASSERT(paramLen > 0);

    const U16 dataFieldLen = static_cast<U16>(1 + paramLen);

    Cfdp::PduHeader header;
    this->fillPduHeader(header, Cfdp::PduType::FILE_DIRECTIVE, dataFieldLen);

    const U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::EOF_PDU,
        paramBuf, paramLen,
        this->m_serializeBuffer, PDU_SERIALIZE_BUFFER_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->transmitPdu(this->m_serializeBuffer, totalLen);

    this->log_ACTIVITY_LO_EofSent(this->m_transactionSeqNum, this->m_checksum.getValue());

    // Per 4.6.3.2.1: Transmission of EOF (No error) causes Notice of Completion
    this->finishTransfer(false);
}

void CfdpSender::sendCancelEofPdu() {
    // Build EOF (cancel) PDU per Section 4.11.2.2
    Cfdp::EofPdu eofPdu;
    eofPdu.conditionCode = Cfdp::ConditionCode::CANCEL_REQUEST_RECEIVED;
    eofPdu.fileChecksum = this->m_checksum.getValue();
    eofPdu.fileSize = this->m_byteOffset - this->m_curEntry.offset;  // Progress so far

    U8 paramBuf[16];
    const U32 paramLen = eofPdu.serialize(paramBuf, sizeof(paramBuf));
    FW_ASSERT(paramLen > 0);

    const U16 dataFieldLen = static_cast<U16>(1 + paramLen);

    Cfdp::PduHeader header;
    this->fillPduHeader(header, Cfdp::PduType::FILE_DIRECTIVE, dataFieldLen);

    const U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::EOF_PDU,
        paramBuf, paramLen,
        this->m_serializeBuffer, PDU_SERIALIZE_BUFFER_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->transmitPdu(this->m_serializeBuffer, totalLen);

    this->log_ACTIVITY_HI_SendCanceled(this->m_curEntry.srcFilename, this->m_curEntry.destFilename);

    this->finishTransfer(true);
}

void CfdpSender::finishTransfer(bool canceled) {
    this->m_file.close();

    if (!canceled) {
        this->m_filesSent++;
        this->tlmWrite_FilesSent(this->m_filesSent);
        this->log_ACTIVITY_HI_FileSent(
            this->m_curEntry.srcFilename,
            this->m_curEntry.destFilename,
            this->m_endOffset - this->m_curEntry.offset,
            this->m_transactionSeqNum
        );
        this->sendResponse(SendFileStatus::STATUS_OK);
    } else {
        this->sendResponse(SendFileStatus::STATUS_ERROR);
    }

    this->m_modeMutex.lock();
    this->m_mode = Mode::IDLE;
    this->m_modeMutex.unLock();
}

void CfdpSender::sendResponse(SendFileStatus status) {
    if (this->m_curEntry.source == CallerSource::COMMAND) {
        Fw::CmdResponse cmdResp;
        switch (status.e) {
            case SendFileStatus::STATUS_OK:
                cmdResp = Fw::CmdResponse::OK;
                break;
            case SendFileStatus::STATUS_ERROR:
                cmdResp = Fw::CmdResponse::EXECUTION_ERROR;
                break;
            default:
                cmdResp = Fw::CmdResponse::EXECUTION_ERROR;
                break;
        }
        this->cmdResponse_out(this->m_curEntry.opCode, this->m_curEntry.cmdSeq, cmdResp);
    } else {
        for (FwIndexType i = 0; i < this->getNum_FileComplete_OutputPorts(); i++) {
            if (this->isConnected_FileComplete_OutputPort(i)) {
                this->FileComplete_out(i, Svc::SendFileResponse(status, this->m_curEntry.context));
            }
        }
    }
}

void CfdpSender::transmitPdu(const U8* data, U32 size) {
    // Allocate a buffer via the buffer get port
    Fw::Buffer sendBuffer = this->bufferGetOut_out(0, size);
    if (sendBuffer.getData() == nullptr || sendBuffer.getSize() < size) {
        // Cannot get buffer, increment warning
        this->m_warningCount++;
        this->tlmWrite_Warnings(this->m_warningCount);
        return;
    }

    (void)std::memcpy(sendBuffer.getData(), data, size);
    sendBuffer.setSize(size);

    this->bufferSendOut_out(0, sendBuffer);

    this->m_pdusSent++;
    this->tlmWrite_PdusSent(this->m_pdusSent);
}

}  // namespace Svc
