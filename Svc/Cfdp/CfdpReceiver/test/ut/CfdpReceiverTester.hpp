// ======================================================================
// \title  CfdpReceiverTester.hpp
// \author devin
// \brief  hpp file for CfdpReceiver component test harness
// ======================================================================

#ifndef Svc_CfdpReceiverTester_HPP
#define Svc_CfdpReceiverTester_HPP

#include <Svc/Cfdp/CfdpReceiver/CfdpReceiver.hpp>
#include "CfdpReceiverGTestBase.hpp"

namespace Svc {

class CfdpReceiverTester : public CfdpReceiverGTestBase {
  public:
    CfdpReceiverTester();
    ~CfdpReceiverTester();

    // Tests
    void testReceiveFile();
    void testPduTooSmall();
    void testInvalidVersion();
    void testFileDataBeforeMetadata();
    void testEofBeforeMetadata();
    void testChecksumFailure();
    void testCancelCommand();
    void testCancelEof();
    void testPingResponse();

  private:
    // Handlers for from ports
    void from_bufferSendOut_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) override;
    void from_pingOut_handler(FwIndexType portNum, U32 key) override;
    void from_fileAnnounce_handler(FwIndexType portNum, Fw::StringBase& fileName) override;

    // Helpers
    void connectPorts();
    void initComponents();

    //! Build and send a complete CFDP PDU buffer to the receiver
    void sendPduBuffer(const U8* data, U32 size);

    //! Build a Metadata PDU and send it to the receiver
    void sendMetadataPdu(
        U64 sourceEntityId,
        U64 destEntityId,
        U32 transactionSeqNum,
        U32 fileSize,
        const char* srcFileName,
        const char* destFileName
    );

    //! Build a File Data PDU and send it to the receiver
    void sendFileDataPdu(
        U64 sourceEntityId,
        U64 destEntityId,
        U32 transactionSeqNum,
        U32 offset,
        const U8* data,
        U32 dataSize
    );

    //! Build an EOF PDU and send it to the receiver
    void sendEofPdu(
        U64 sourceEntityId,
        U64 destEntityId,
        U32 transactionSeqNum,
        U8 conditionCode,
        U32 fileChecksum,
        U32 fileSize
    );

    void removeTestFile(const char* path);

    // Member variables
    CfdpReceiver component;

    // Track buffer returns
    U32 m_bufferReturnCount;

    // Track file announcements
    U32 m_fileAnnounceCount;

    // Internal buffer for building PDUs
    static constexpr U32 PDU_BUF_SIZE = 2048;
    U8 m_pduBuf[PDU_BUF_SIZE];
};

}  // namespace Svc

#endif
