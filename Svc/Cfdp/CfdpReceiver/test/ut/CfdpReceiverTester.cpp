// ======================================================================
// \title  CfdpReceiverTester.cpp
// \author devin
// \brief  cpp file for CfdpReceiver component test harness
// ======================================================================

#include "CfdpReceiverTester.hpp"
#include <Svc/Cfdp/Types/CfdpPdu.hpp>
#include <cstring>
#include <Os/FileSystem.hpp>

#define INSTANCE 0
#define MAX_HISTORY_SIZE 100
#define QUEUE_DEPTH 10

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfdpReceiverTester::CfdpReceiverTester()
    : CfdpReceiverGTestBase("Tester", MAX_HISTORY_SIZE),
      component("CfdpReceiver"),
      m_bufferReturnCount(0),
      m_fileAnnounceCount(0) {
    this->connectPorts();
    this->initComponents();
    std::memset(m_pduBuf, 0, sizeof(m_pduBuf));
}

CfdpReceiverTester::~CfdpReceiverTester() {
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CfdpReceiverTester::testReceiveFile() {
    const char* srcPath = "sender_src.bin";
    const char* destPath = "cfdp_recv_test.bin";
    U8 fileData[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    const U32 fileSize = sizeof(fileData);

    // Compute expected checksum
    CFDP::Checksum checksum;
    checksum.update(fileData, 0, fileSize);

    // Send Metadata PDU
    this->sendMetadataPdu(1, 2, 100, fileSize, srcPath, destPath);
    ASSERT_EVENTS_MetadataReceived_SIZE(1);
    ASSERT_TLM_PdusReceived_SIZE(1);

    // Send File Data PDU
    this->clearHistory();
    this->sendFileDataPdu(1, 2, 100, 0, fileData, fileSize);
    ASSERT_TLM_TotalBytesReceived_SIZE(1);

    // Send EOF PDU
    this->clearHistory();
    this->sendEofPdu(1, 2, 100, 0, checksum.getValue(), fileSize);
    ASSERT_EVENTS_EofReceived_SIZE(1);
    ASSERT_EVENTS_FileReceived_SIZE(1);
    ASSERT_TLM_FilesReceived_SIZE(1);
    ASSERT_TLM_FilesReceived(0, 1);

    // Verify file announcement
    ASSERT_EQ(1U, m_fileAnnounceCount);

    // Verify component is back to idle
    ASSERT_EQ(CfdpReceiver::ReceiveMode::IDLE, this->component.m_receiveMode);

    // Clean up
    this->removeTestFile(destPath);
}

void CfdpReceiverTester::testPduTooSmall() {
    // Send a buffer that is too small to be a valid PDU
    U8 tinyBuf[] = {0x20, 0x00, 0x01};  // Only 3 bytes
    this->sendPduBuffer(tinyBuf, sizeof(tinyBuf));

    ASSERT_EVENTS_PduTooSmall_SIZE(1);
    ASSERT_TLM_Warnings_SIZE(1);
}

void CfdpReceiverTester::testInvalidVersion() {
    // Construct a PDU with wrong version (version 3 instead of 1)
    // First 3 bits of first byte = version
    U8 badVersionPdu[10];
    std::memset(badVersionPdu, 0, sizeof(badVersionPdu));
    badVersionPdu[0] = 0x60;  // Version 3 (0b011 << 5)
    badVersionPdu[1] = 0x00;
    badVersionPdu[2] = 0x02;  // data field length = 2
    badVersionPdu[3] = 0x10;  // entityIdLength=0 (1-byte), seqNumLength=0 (1-byte)
    badVersionPdu[4] = 0x01;  // source entity ID
    badVersionPdu[5] = 0x01;  // transaction seq num
    badVersionPdu[6] = 0x02;  // destination entity ID
    // data field
    badVersionPdu[7] = 0x07;  // directive code (Metadata)
    badVersionPdu[8] = 0x00;

    this->sendPduBuffer(badVersionPdu, 9);

    ASSERT_EVENTS_InvalidPduHeader_SIZE(1);
    ASSERT_TLM_Warnings_SIZE(1);
}

void CfdpReceiverTester::testFileDataBeforeMetadata() {
    // Send a file data PDU when in IDLE mode (no metadata received yet)
    // Build a valid file data PDU
    Cfdp::PduHeader header;
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = Cfdp::PduType::FILE_DATA;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;
    header.crcFlag = false;
    header.largeFileFlag = false;
    header.segmentationControl = false;
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    header.seqNumLength = 1;
    header.sourceEntityId = 1;
    header.transactionSeqNum = 100;
    header.destinationEntityId = 2;

    U8 fileData[] = {0x01, 0x02, 0x03, 0x04};
    Cfdp::FileDataPdu fdPdu;
    fdPdu.offset = 0;
    fdPdu.data = fileData;
    fdPdu.dataSize = sizeof(fileData);
    header.dataFieldLength = 4 + sizeof(fileData);  // offset + data

    U32 totalLen = Cfdp::serializeFileDataPdu(header, fdPdu, m_pduBuf, PDU_BUF_SIZE);
    ASSERT_GT(totalLen, 0U);

    this->sendPduBuffer(m_pduBuf, totalLen);

    ASSERT_EVENTS_InvalidReceiveMode_SIZE(1);
    ASSERT_TLM_Warnings_SIZE(1);
}

void CfdpReceiverTester::testEofBeforeMetadata() {
    // Send EOF when in IDLE mode
    Cfdp::PduHeader header;
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = Cfdp::PduType::FILE_DIRECTIVE;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;
    header.crcFlag = false;
    header.largeFileFlag = false;
    header.segmentationControl = false;
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    header.seqNumLength = 1;
    header.sourceEntityId = 1;
    header.transactionSeqNum = 100;
    header.destinationEntityId = 2;

    Cfdp::EofPdu eofPdu;
    eofPdu.conditionCode = Cfdp::ConditionCode::NO_ERROR;
    eofPdu.fileChecksum = 0;
    eofPdu.fileSize = 0;

    U8 paramBuf[16];
    U32 paramLen = eofPdu.serialize(paramBuf, sizeof(paramBuf));

    header.dataFieldLength = static_cast<U16>(1 + paramLen);

    U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::EOF_PDU,
        paramBuf, paramLen,
        m_pduBuf, PDU_BUF_SIZE
    );
    ASSERT_GT(totalLen, 0U);

    this->sendPduBuffer(m_pduBuf, totalLen);

    ASSERT_EVENTS_InvalidReceiveMode_SIZE(1);
    ASSERT_TLM_Warnings_SIZE(1);
}

void CfdpReceiverTester::testChecksumFailure() {
    const char* destPath = "cfdp_cksum_test.bin";
    U8 fileData[] = {0x01, 0x02, 0x03, 0x04};
    const U32 fileSize = sizeof(fileData);

    // Send Metadata
    this->sendMetadataPdu(1, 2, 200, fileSize, "src.bin", destPath);

    // Send File Data
    this->clearHistory();
    this->sendFileDataPdu(1, 2, 200, 0, fileData, fileSize);

    // Send EOF with wrong checksum
    this->clearHistory();
    this->sendEofPdu(1, 2, 200, 0, 0xDEADBEEF, fileSize);

    // File should still be received (per CFDP Class 1), but with checksum warning
    ASSERT_EVENTS_ChecksumFailure_SIZE(1);
    ASSERT_EVENTS_FileReceived_SIZE(1);

    this->removeTestFile(destPath);
}

void CfdpReceiverTester::testCancelCommand() {
    const char* destPath = "cfdp_cancel_cmd_test.bin";
    U8 fileData[] = {0x01, 0x02};

    // Start a transaction
    this->sendMetadataPdu(1, 2, 300, 4, "src.bin", destPath);

    // Send some data
    this->clearHistory();
    this->sendFileDataPdu(1, 2, 300, 0, fileData, sizeof(fileData));

    // Send Cancel command
    this->clearHistory();
    this->sendCmd_Cancel(0, 0);
    this->component.doDispatch();

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpReceiverComponentBase::OPCODE_CANCEL, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_ReceiveCanceled_SIZE(1);

    // Verify back to idle
    ASSERT_EQ(CfdpReceiver::ReceiveMode::IDLE, this->component.m_receiveMode);

    this->removeTestFile(destPath);
}

void CfdpReceiverTester::testCancelEof() {
    const char* destPath = "cfdp_cancel_eof_test.bin";

    // Start a transaction
    this->sendMetadataPdu(1, 2, 400, 4, "src.bin", destPath);

    // Send cancel EOF
    this->clearHistory();
    this->sendEofPdu(1, 2, 400,
                     static_cast<U8>(Cfdp::ConditionCode::CANCEL_REQUEST_RECEIVED),
                     0, 0);

    ASSERT_EVENTS_EofReceived_SIZE(1);
    ASSERT_EVENTS_ReceiveCanceled_SIZE(1);

    // Verify back to idle
    ASSERT_EQ(CfdpReceiver::ReceiveMode::IDLE, this->component.m_receiveMode);

    this->removeTestFile(destPath);
}

void CfdpReceiverTester::testPingResponse() {
    this->clearHistory();
    this->invoke_to_pingIn(0, 99);
    this->component.doDispatch();

    ASSERT_from_pingOut_SIZE(1);
    ASSERT_from_pingOut(0, 99U);
}

// ----------------------------------------------------------------------
// Handlers for from ports
// ----------------------------------------------------------------------

void CfdpReceiverTester::from_bufferSendOut_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->pushFromPortEntry_bufferSendOut(fwBuffer);
    m_bufferReturnCount++;
}

void CfdpReceiverTester::from_pingOut_handler(FwIndexType portNum, U32 key) {
    this->pushFromPortEntry_pingOut(key);
}

void CfdpReceiverTester::from_fileAnnounce_handler(FwIndexType portNum, Fw::StringBase& fileName) {
    this->pushFromPortEntry_fileAnnounce(fileName);
    m_fileAnnounceCount++;
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void CfdpReceiverTester::connectPorts() {
    // bufferSendIn
    this->connect_to_bufferSendIn(0, this->component.get_bufferSendIn_InputPort(0));

    // pingIn
    this->connect_to_pingIn(0, this->component.get_pingIn_InputPort(0));

    // Cmd
    this->connect_to_cmdIn(0, this->component.get_cmdIn_InputPort(0));

    // From ports
    this->component.set_bufferSendOut_OutputPort(0, this->get_from_bufferSendOut(0));
    this->component.set_fileAnnounce_OutputPort(0, this->get_from_fileAnnounce(0));
    this->component.set_pingOut_OutputPort(0, this->get_from_pingOut(0));
    this->component.set_cmdRegOut_OutputPort(0, this->get_from_cmdRegOut(0));
    this->component.set_cmdResponseOut_OutputPort(0, this->get_from_cmdResponseOut(0));
    this->component.set_eventOut_OutputPort(0, this->get_from_eventOut(0));
    this->component.set_textEventOut_OutputPort(0, this->get_from_textEventOut(0));
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
}

void CfdpReceiverTester::initComponents() {
    this->init();
    this->component.init(QUEUE_DEPTH, INSTANCE);
}

void CfdpReceiverTester::sendPduBuffer(const U8* data, U32 size) {
    // Create a buffer wrapping the data (use a local copy since buffer is non-const)
    U8 bufData[PDU_BUF_SIZE];
    FW_ASSERT(size <= PDU_BUF_SIZE);
    std::memcpy(bufData, data, size);
    Fw::Buffer buffer(bufData, size);

    this->invoke_to_bufferSendIn(0, buffer);
    this->component.doDispatch();
}

void CfdpReceiverTester::sendMetadataPdu(
    U64 sourceEntityId,
    U64 destEntityId,
    U32 transactionSeqNum,
    U32 fileSize,
    const char* srcFileName,
    const char* destFileName
) {
    Cfdp::MetadataPdu metadata;
    metadata.closureRequested = false;
    metadata.checksumType = 0;
    metadata.fileSize = fileSize;

    const U32 srcLen = static_cast<U32>(std::strlen(srcFileName));
    const U32 dstLen = static_cast<U32>(std::strlen(destFileName));
    metadata.sourceFileNameLen = static_cast<U8>(srcLen > Cfdp::MAX_FILE_NAME_LEN ? Cfdp::MAX_FILE_NAME_LEN : srcLen);
    std::memcpy(metadata.sourceFileName, srcFileName, metadata.sourceFileNameLen);
    metadata.sourceFileName[metadata.sourceFileNameLen] = '\0';
    metadata.destFileNameLen = static_cast<U8>(dstLen > Cfdp::MAX_FILE_NAME_LEN ? Cfdp::MAX_FILE_NAME_LEN : dstLen);
    std::memcpy(metadata.destFileName, destFileName, metadata.destFileNameLen);
    metadata.destFileName[metadata.destFileNameLen] = '\0';

    U8 paramBuf[512];
    U32 paramLen = metadata.serialize(paramBuf, sizeof(paramBuf));
    FW_ASSERT(paramLen > 0);

    U16 dataFieldLen = static_cast<U16>(1 + paramLen);

    Cfdp::PduHeader header;
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = Cfdp::PduType::FILE_DIRECTIVE;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;
    header.crcFlag = false;
    header.largeFileFlag = false;
    header.dataFieldLength = dataFieldLen;
    header.segmentationControl = false;
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    header.seqNumLength = 1;
    header.sourceEntityId = sourceEntityId;
    header.transactionSeqNum = transactionSeqNum;
    header.destinationEntityId = destEntityId;

    U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::METADATA_PDU,
        paramBuf, paramLen,
        m_pduBuf, PDU_BUF_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->sendPduBuffer(m_pduBuf, totalLen);
}

void CfdpReceiverTester::sendFileDataPdu(
    U64 sourceEntityId,
    U64 destEntityId,
    U32 transactionSeqNum,
    U32 offset,
    const U8* data,
    U32 dataSize
) {
    Cfdp::PduHeader header;
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = Cfdp::PduType::FILE_DATA;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;
    header.crcFlag = false;
    header.largeFileFlag = false;
    header.dataFieldLength = static_cast<U16>(4 + dataSize);
    header.segmentationControl = false;
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    header.seqNumLength = 1;
    header.sourceEntityId = sourceEntityId;
    header.transactionSeqNum = transactionSeqNum;
    header.destinationEntityId = destEntityId;

    Cfdp::FileDataPdu fdPdu;
    fdPdu.offset = offset;
    fdPdu.data = const_cast<U8*>(data);
    fdPdu.dataSize = dataSize;

    U32 totalLen = Cfdp::serializeFileDataPdu(
        header, fdPdu,
        m_pduBuf, PDU_BUF_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->sendPduBuffer(m_pduBuf, totalLen);
}

void CfdpReceiverTester::sendEofPdu(
    U64 sourceEntityId,
    U64 destEntityId,
    U32 transactionSeqNum,
    U8 conditionCode,
    U32 fileChecksum,
    U32 fileSize
) {
    Cfdp::EofPdu eofPdu;
    eofPdu.conditionCode = static_cast<Cfdp::ConditionCode>(conditionCode);
    eofPdu.fileChecksum = fileChecksum;
    eofPdu.fileSize = fileSize;

    U8 paramBuf[16];
    U32 paramLen = eofPdu.serialize(paramBuf, sizeof(paramBuf));
    FW_ASSERT(paramLen > 0);

    U16 dataFieldLen = static_cast<U16>(1 + paramLen);

    Cfdp::PduHeader header;
    header.version = Cfdp::CFDP_VERSION;
    header.pduType = Cfdp::PduType::FILE_DIRECTIVE;
    header.direction = Cfdp::Direction::TOWARD_RECEIVER;
    header.mode = Cfdp::TransmissionMode::UNACKNOWLEDGED;
    header.crcFlag = false;
    header.largeFileFlag = false;
    header.dataFieldLength = dataFieldLen;
    header.segmentationControl = false;
    header.entityIdLength = 1;
    header.segmentMetadataFlag = false;
    header.seqNumLength = 1;
    header.sourceEntityId = sourceEntityId;
    header.transactionSeqNum = transactionSeqNum;
    header.destinationEntityId = destEntityId;

    U32 totalLen = Cfdp::serializeDirectivePdu(
        header, Cfdp::DirectiveCode::EOF_PDU,
        paramBuf, paramLen,
        m_pduBuf, PDU_BUF_SIZE
    );
    FW_ASSERT(totalLen > 0);

    this->sendPduBuffer(m_pduBuf, totalLen);
}

void CfdpReceiverTester::removeTestFile(const char* path) {
    Os::FileSystem::removeFile(path);
}

}  // namespace Svc
