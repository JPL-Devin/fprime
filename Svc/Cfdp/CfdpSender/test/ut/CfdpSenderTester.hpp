// ======================================================================
// \title  CfdpSenderTester.hpp
// \author devin
// \brief  hpp file for CfdpSender component test harness
// ======================================================================

#ifndef Svc_CfdpSenderTester_HPP
#define Svc_CfdpSenderTester_HPP

#include <Svc/Cfdp/CfdpSender/CfdpSender.hpp>
#include "CfdpSenderGTestBase.hpp"

namespace Svc {

class CfdpSenderTester : public CfdpSenderGTestBase {
  public:
    CfdpSenderTester();
    ~CfdpSenderTester();

    // Tests
    void testSendFileCommand();
    void testSendFilePort();
    void testSendPartialFile();
    void testCancelDuringTransfer();
    void testSendWhileBusy();
    void testFileOpenError();
    void testTransmitPduFailureRetry();
    void testPingResponse();

  private:
    // Handlers for from ports
    void from_bufferSendOut_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) override;
    void from_pingOut_handler(FwIndexType portNum, U32 key) override;
    Fw::Buffer from_bufferGetOut_handler(FwIndexType portNum, FwSizeType size) override;
    void from_FileComplete_handler(FwIndexType portNum, const Svc::SendFileResponse& resp) override;

    // Helpers
    void connectPorts();
    void initComponents();
    void createTestFile(const char* path, const U8* data, U32 size);
    void removeTestFile(const char* path);
    void driveRunCycles(U32 count);

    // Member variables
    CfdpSender component;

    // Buffer pool for bufferGetOut
    static constexpr U32 BUFFER_POOL_SIZE = 16;
    static constexpr U32 BUFFER_MAX_SIZE = 2048;
    U8 m_bufferPool[BUFFER_POOL_SIZE][BUFFER_MAX_SIZE];
    U32 m_bufferPoolIdx;
    bool m_bufferGetFail;  //!< When true, bufferGetOut returns empty buffer

    // Track sent buffers
    static constexpr U32 MAX_SENT_BUFFERS = 64;
    Fw::Buffer m_sentBuffers[MAX_SENT_BUFFERS];
    U32 m_sentBufferCount;

    // Track file complete responses
    Svc::SendFileResponse m_lastFileCompleteResponse;
    U32 m_fileCompleteCount;
};

}  // namespace Svc

#endif
