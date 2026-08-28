// ======================================================================
// \title  CfdpReceiver.hpp
// \author devin
// \brief  hpp file for CfdpReceiver component implementation class
//
// CFDP Class 1 (Unacknowledged) receiving entity per CCSDS 727.0-B-5.
// Receives Metadata PDU, File Data PDUs, and EOF PDU for each transaction.
//
// \copyright
// Copyright 2025, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#ifndef Svc_CfdpReceiver_HPP
#define Svc_CfdpReceiver_HPP

#include <CFDP/Checksum/Checksum.hpp>
#include <Fw/Types/FileNameString.hpp>
#include <Os/File.hpp>
#include <Svc/Cfdp/CfdpReceiver/CfdpReceiverComponentAc.hpp>
#include <Svc/Cfdp/Types/CfdpPdu.hpp>

namespace Svc {

class CfdpReceiver final : public CfdpReceiverComponentBase {
    friend class CfdpReceiverTester;

  public:
    // ----------------------------------------------------------------------
    // Construction, initialization, and destruction
    // ----------------------------------------------------------------------

    //! Construct object CfdpReceiver
    CfdpReceiver(const char* const compName);

    //! Destroy object CfdpReceiver
    ~CfdpReceiver();

  private:
    // ----------------------------------------------------------------------
    // Types
    // ----------------------------------------------------------------------

    //! Receiver operating mode
    enum class ReceiveMode : U8 {
        IDLE = 0,       //!< No active transaction, waiting for Metadata PDU
        ACTIVE = 1      //!< Transaction in progress, receiving file data and EOF
    };

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for bufferSendIn
    void bufferSendIn_handler(const FwIndexType portNum, Fw::Buffer& buffer) override;

    //! Handler implementation for pingIn
    void pingIn_handler(const FwIndexType portNum, U32 key) override;

    // ----------------------------------------------------------------------
    // Command handler implementations
    // ----------------------------------------------------------------------

    //! Handler for Cancel command
    void Cancel_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Process a received file directive PDU
    void handleFileDirective(const Cfdp::PduHeader& header, const U8* dataField, U32 dataFieldLen);

    //! Process a received Metadata PDU
    void handleMetadataPdu(const Cfdp::PduHeader& header, const U8* params, U32 paramsLen);

    //! Process a received File Data PDU
    void handleFileDataPdu(const Cfdp::PduHeader& header, const U8* dataField, U32 dataFieldLen);

    //! Process a received EOF PDU
    void handleEofPdu(const Cfdp::PduHeader& header, const U8* params, U32 paramsLen);

    //! Reset to idle state, closing any open file
    void goToIdleMode();

    //! Check if file reception is complete (all data received + checksum OK)
    void checkCompletion();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Current receive mode
    ReceiveMode m_receiveMode;

    //! Active transaction state
    U64 m_transactionSourceEntity;  //!< Source entity ID of the current transaction
    U64 m_transactionSeqNum;        //!< Transaction sequence number

    //! File state
    Os::File m_file;
    Fw::FileNameString m_destFileName;
    Fw::FileNameString m_sourceFileName;
    U32 m_expectedFileSize;
    U32 m_bytesReceived;           //!< Total bytes of file data received
    CFDP::Checksum m_checksum;     //!< Running checksum of received data

    //! EOF state
    bool m_eofReceived;
    U32 m_eofFileChecksum;
    U32 m_eofFileSize;

    //! Telemetry counters
    U32 m_filesReceived;
    U32 m_pdusReceived;
    U32 m_warningCount;
    U64 m_totalBytesReceived;
};

}  // namespace Svc

#endif
