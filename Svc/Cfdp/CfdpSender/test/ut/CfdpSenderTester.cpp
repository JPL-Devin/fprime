// ======================================================================
// \title  CfdpSenderTester.cpp
// \author devin
// \brief  cpp file for CfdpSender component test harness
// ======================================================================

#include "CfdpSenderTester.hpp"
#include <cstring>
#include <cstdio>
#include <Os/File.hpp>
#include <Os/FileSystem.hpp>

#define INSTANCE 0
#define MAX_HISTORY_SIZE 100
#define QUEUE_DEPTH 10

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CfdpSenderTester::CfdpSenderTester()
    : CfdpSenderGTestBase("Tester", MAX_HISTORY_SIZE),
      component("CfdpSender"),
      m_bufferPoolIdx(0),
      m_bufferGetFail(false),
      m_sentBufferCount(0),
      m_lastFileCompleteResponse(Svc::SendFileResponse(Svc::SendFileStatus::STATUS_ERROR, 0)),
      m_fileCompleteCount(0) {
    this->connectPorts();
    this->initComponents();
    this->component.configure(1, 2, 512);
    std::memset(m_bufferPool, 0, sizeof(m_bufferPool));
}

CfdpSenderTester::~CfdpSenderTester() {
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CfdpSenderTester::testSendFileCommand() {
    // Create a test file
    const char* srcPath = "cfdp_sender_test_src.bin";
    const char* destPath = "cfdp_sender_test_dst.bin";
    U8 fileData[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Send the SendFile command
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    this->sendCmd_SendFile(0, 0, srcStr, dstStr);
    this->component.doDispatch();

    // Verify SendStarted event was emitted during startTransfer()
    ASSERT_EVENTS_SendStarted_SIZE(1);

    // Drive the Run handler to send Metadata PDU
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // Should have sent one PDU (metadata)
    ASSERT_from_bufferSendOut_SIZE(1);

    // Drive Run to send file data PDUs
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // Should have sent a file data PDU
    ASSERT_from_bufferSendOut_SIZE(1);

    // Drive Run to send EOF PDU (file is small, should be done after one data PDU)
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // Should have sent EOF PDU
    ASSERT_from_bufferSendOut_SIZE(1);

    // Verify telemetry
    ASSERT_TLM_FilesSent_SIZE(1);
    ASSERT_TLM_FilesSent(0, 1);

    // Verify command response (OK)
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpSenderComponentBase::OPCODE_SENDFILE, 0, Fw::CmdResponse::OK);

    // Verify FileSent event
    ASSERT_EVENTS_FileSent_SIZE(1);

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testSendFilePort() {
    // Create a test file
    const char* srcPath = "cfdp_port_test_src.bin";
    const char* destPath = "cfdp_port_test_dst.bin";
    U8 fileData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Invoke the SendFile port
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    Svc::SendFileResponse resp = this->invoke_to_SendFile(0, srcStr, dstStr, 0, 0);
    ASSERT_EQ(Svc::SendFileStatus::STATUS_OK, resp.get_status());

    // Drive Run cycles: metadata, data, EOF
    for (U32 i = 0; i < 3; i++) {
        this->clearHistory();
        this->invoke_to_Run(0, 0);
        this->component.doDispatch();
        ASSERT_from_bufferSendOut_SIZE(1);
    }

    // Verify file complete callback
    ASSERT_EQ(1U, m_fileCompleteCount);
    ASSERT_EQ(Svc::SendFileStatus::STATUS_OK, m_lastFileCompleteResponse.get_status());

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testSendPartialFile() {
    // Create a test file with 32 bytes
    const char* srcPath = "cfdp_partial_test_src.bin";
    const char* destPath = "cfdp_partial_test_dst.bin";
    U8 fileData[32];
    for (U32 i = 0; i < sizeof(fileData); i++) {
        fileData[i] = static_cast<U8>(i);
    }
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Send partial: offset=8, length=16 (bytes 8-23)
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    this->sendCmd_SendPartial(0, 0, srcStr, dstStr, 8, 16);
    this->component.doDispatch();

    // Drive Run cycles: metadata, data, EOF
    for (U32 i = 0; i < 3; i++) {
        this->clearHistory();
        this->invoke_to_Run(0, 0);
        this->component.doDispatch();
        ASSERT_from_bufferSendOut_SIZE(1);
    }

    // Verify command response (OK)
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpSenderComponentBase::OPCODE_SENDPARTIAL, 0, Fw::CmdResponse::OK);

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testCancelDuringTransfer() {
    // Create a test file
    const char* srcPath = "cfdp_cancel_test_src.bin";
    const char* destPath = "cfdp_cancel_test_dst.bin";
    U8 fileData[256];
    std::memset(fileData, 0xAA, sizeof(fileData));
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Start a transfer
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    this->sendCmd_SendFile(0, 0, srcStr, dstStr);
    this->component.doDispatch();

    // Drive one Run to send metadata
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();
    ASSERT_from_bufferSendOut_SIZE(1);

    // Send Cancel command
    this->clearHistory();
    this->sendCmd_Cancel(0, 1);
    this->component.doDispatch();

    // Cancel command should be accepted
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpSenderComponentBase::OPCODE_CANCEL, 1, Fw::CmdResponse::OK);

    // Drive Run to send cancel EOF
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // Should have sent cancel EOF PDU
    ASSERT_from_bufferSendOut_SIZE(1);

    // Verify cancel event
    ASSERT_EVENTS_SendCanceled_SIZE(1);

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testSendWhileBusy() {
    // Create a test file
    const char* srcPath = "cfdp_busy_test_src.bin";
    const char* destPath = "cfdp_busy_test_dst.bin";
    U8 fileData[] = {0x01, 0x02, 0x03, 0x04};
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Start first transfer
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    this->sendCmd_SendFile(0, 0, srcStr, dstStr);
    this->component.doDispatch();

    // Try to send again while busy
    this->clearHistory();
    this->sendCmd_SendFile(0, 1, srcStr, dstStr);
    this->component.doDispatch();

    // Second command should get BUSY response
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpSenderComponentBase::OPCODE_SENDFILE, 1, Fw::CmdResponse::BUSY);

    // Clean up - finish the first transfer
    for (U32 i = 0; i < 3; i++) {
        this->clearHistory();
        this->invoke_to_Run(0, 0);
        this->component.doDispatch();
    }

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testFileOpenError() {
    // Try to send a nonexistent file
    Fw::String srcStr("nonexistent_file.bin");
    Fw::String dstStr("dest.bin");
    this->sendCmd_SendFile(0, 0, srcStr, dstStr);
    this->component.doDispatch();

    // Should get error response
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, CfdpSenderComponentBase::OPCODE_SENDFILE, 0, Fw::CmdResponse::EXECUTION_ERROR);

    // Verify warning event
    ASSERT_EVENTS_FileOpenError_SIZE(1);
}

void CfdpSenderTester::testTransmitPduFailureRetry() {
    // Create a test file
    const char* srcPath = "cfdp_retry_test_src.bin";
    const char* destPath = "cfdp_retry_test_dst.bin";
    U8 fileData[] = {0x01, 0x02, 0x03, 0x04};
    this->createTestFile(srcPath, fileData, sizeof(fileData));

    // Start a transfer
    Fw::String srcStr(srcPath);
    Fw::String dstStr(destPath);
    this->sendCmd_SendFile(0, 0, srcStr, dstStr);
    this->component.doDispatch();

    // Fail buffer allocation
    this->m_bufferGetFail = true;

    // Drive Run - transmitPdu should fail, state should NOT advance
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // No buffer should have been sent (transmit failed)
    ASSERT_from_bufferSendOut_SIZE(0);

    // Verify warning telemetry was emitted
    ASSERT_TLM_Warnings_SIZE(1);

    // Re-enable buffer allocation
    this->m_bufferGetFail = false;

    // Drive Run again - should retry metadata PDU successfully
    this->clearHistory();
    this->invoke_to_Run(0, 0);
    this->component.doDispatch();

    // Now should have sent the metadata PDU
    ASSERT_from_bufferSendOut_SIZE(1);

    // Drive remaining cycles to complete transfer
    for (U32 i = 0; i < 2; i++) {
        this->clearHistory();
        this->invoke_to_Run(0, 0);
        this->component.doDispatch();
        ASSERT_from_bufferSendOut_SIZE(1);
    }

    // Verify transfer completed
    ASSERT_TLM_FilesSent_SIZE(1);

    this->removeTestFile(srcPath);
}

void CfdpSenderTester::testPingResponse() {
    this->clearHistory();
    this->invoke_to_pingIn(0, 42);
    this->component.doDispatch();

    ASSERT_from_pingOut_SIZE(1);
    ASSERT_from_pingOut(0, 42U);
}

// ----------------------------------------------------------------------
// Handlers for from ports
// ----------------------------------------------------------------------

void CfdpSenderTester::from_bufferSendOut_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->pushFromPortEntry_bufferSendOut(fwBuffer);
    if (m_sentBufferCount < MAX_SENT_BUFFERS) {
        m_sentBuffers[m_sentBufferCount++] = fwBuffer;
    }
}

void CfdpSenderTester::from_pingOut_handler(FwIndexType portNum, U32 key) {
    this->pushFromPortEntry_pingOut(key);
}

Fw::Buffer CfdpSenderTester::from_bufferGetOut_handler(FwIndexType portNum, FwSizeType size) {
    if (m_bufferGetFail) {
        return Fw::Buffer();  // Return empty buffer to simulate allocation failure
    }

    U32 idx = m_bufferPoolIdx % BUFFER_POOL_SIZE;
    m_bufferPoolIdx++;

    U32 allocSize = (size <= BUFFER_MAX_SIZE) ? static_cast<U32>(size) : BUFFER_MAX_SIZE;
    return Fw::Buffer(m_bufferPool[idx], allocSize);
}

void CfdpSenderTester::from_FileComplete_handler(FwIndexType portNum, const Svc::SendFileResponse& resp) {
    this->pushFromPortEntry_FileComplete(resp);
    m_lastFileCompleteResponse = resp;
    m_fileCompleteCount++;
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void CfdpSenderTester::connectPorts() {
    // Run
    this->connect_to_Run(0, this->component.get_Run_InputPort(0));

    // SendFile
    this->connect_to_SendFile(0, this->component.get_SendFile_InputPort(0));

    // bufferReturn
    this->connect_to_bufferReturn(0, this->component.get_bufferReturn_InputPort(0));

    // pingIn
    this->connect_to_pingIn(0, this->component.get_pingIn_InputPort(0));

    // Cmd
    this->connect_to_cmdIn(0, this->component.get_cmdIn_InputPort(0));

    // From ports
    this->component.set_bufferSendOut_OutputPort(0, this->get_from_bufferSendOut(0));
    this->component.set_bufferGetOut_OutputPort(0, this->get_from_bufferGetOut(0));
    this->component.set_FileComplete_OutputPort(0, this->get_from_FileComplete(0));
    this->component.set_pingOut_OutputPort(0, this->get_from_pingOut(0));
    this->component.set_cmdRegOut_OutputPort(0, this->get_from_cmdRegOut(0));
    this->component.set_cmdResponseOut_OutputPort(0, this->get_from_cmdResponseOut(0));
    this->component.set_eventOut_OutputPort(0, this->get_from_eventOut(0));
    this->component.set_textEventOut_OutputPort(0, this->get_from_textEventOut(0));
    this->component.set_tlmOut_OutputPort(0, this->get_from_tlmOut(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
}

void CfdpSenderTester::initComponents() {
    this->init();
    this->component.init(QUEUE_DEPTH, INSTANCE);
}

void CfdpSenderTester::createTestFile(const char* path, const U8* data, U32 size) {
    Os::File file;
    Os::File::Status status = file.open(path, Os::File::Mode::OPEN_CREATE);
    FW_ASSERT(status == Os::File::OP_OK, static_cast<FwAssertArgType>(status));
    FwSizeType writeSize = static_cast<FwSizeType>(size);
    status = file.write(data, writeSize);
    FW_ASSERT(status == Os::File::OP_OK, static_cast<FwAssertArgType>(status));
    file.close();
}

void CfdpSenderTester::removeTestFile(const char* path) {
    Os::FileSystem::removeFile(path);
}

void CfdpSenderTester::driveRunCycles(U32 count) {
    for (U32 i = 0; i < count; i++) {
        this->invoke_to_Run(0, 0);
        this->component.doDispatch();
    }
}

}  // namespace Svc
