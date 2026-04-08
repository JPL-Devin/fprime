// ======================================================================
// \title  CfdpSender.hpp
// \author devin
// \brief  hpp file for CfdpSender component implementation class
//
// CFDP Class 1 (Unacknowledged) sending entity per CCSDS 727.0-B-5.
// Sends Metadata PDU, File Data PDUs, and EOF PDU for each transaction.
//
// \copyright
// Copyright 2025, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
// ======================================================================

#ifndef Svc_CfdpSender_HPP
#define Svc_CfdpSender_HPP

#include <CFDP/Checksum/Checksum.hpp>
#include <Fw/Types/FileNameString.hpp>
#include <Os/File.hpp>
#include <Os/FileSystem.hpp>
#include <Os/Mutex.hpp>
#include <Svc/Cfdp/CfdpSender/CfdpSenderComponentAc.hpp>
#include <Svc/Cfdp/Types/CfdpPdu.hpp>

namespace Svc {

class CfdpSender final : public CfdpSenderComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Construction, initialization, and destruction
    // ----------------------------------------------------------------------

    //! Construct object CfdpSender
    CfdpSender(const char* const compName);

    //! Configure the CFDP sender
    //! \param localEntityId The CFDP entity ID of this sender
    //! \param remoteEntityId The CFDP entity ID of the destination
    //! \param pduBufferSize Maximum PDU buffer size for data segments
    void configure(
        U64 localEntityId,
        U64 remoteEntityId,
        U32 pduBufferSize
    );

    //! Destroy object CfdpSender
    ~CfdpSender();

  private:
    // ----------------------------------------------------------------------
    // Types
    // ----------------------------------------------------------------------

    //! Sender operating mode
    enum class Mode {
        IDLE,       //!< No active transaction
        METADATA,   //!< Metadata PDU to be sent
        DATA,       //!< Sending file data PDUs
        EOF_PDU,    //!< EOF PDU to be sent
        WAIT,       //!< Waiting for buffer return
        CANCEL      //!< Canceling current transaction
    };

    //! Source of send request
    enum class CallerSource { COMMAND, PORT };

    //! Tracks a single file transfer request
    struct TransferEntry {
        Fw::FileNameString srcFilename;
        Fw::FileNameString destFilename;
        U32 offset;
        U32 length;
        CallerSource source;
        FwOpcodeType opCode;
        U32 cmdSeq;
        U32 context;
    };

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for Run
    void Run_handler(const FwIndexType portNum, U32 context) override;

    //! Handler implementation for SendFile
    Svc::SendFileResponse SendFile_handler(
        const FwIndexType portNum,
        const Fw::StringBase& sourceFilename,
        const Fw::StringBase& destFilename,
        U32 offset,
        U32 length
    ) override;

    //! Handler implementation for bufferReturn
    void bufferReturn_handler(const FwIndexType portNum, Fw::Buffer& fwBuffer) override;

    //! Handler implementation for pingIn
    void pingIn_handler(const FwIndexType portNum, U32 key) override;

    // ----------------------------------------------------------------------
    // Command handler implementations
    // ----------------------------------------------------------------------

    //! Handler for SendFile command
    void SendFile_cmdHandler(
        const FwOpcodeType opCode,
        const U32 cmdSeq,
        const Fw::CmdStringArg& sourceFilename,
        const Fw::CmdStringArg& destFilename
    ) override;

    //! Handler for Cancel command
    void Cancel_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) override;

    //! Handler for SendPartial command
    void SendPartial_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        const Fw::CmdStringArg& sourceFilename,
        const Fw::CmdStringArg& destFilename,
        U32 startOffset,
        U32 length
    ) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Start a new file transfer
    void startTransfer();

    //! Build and transmit a Metadata PDU
    void sendMetadataPdu();

    //! Build and transmit the next File Data PDU
    void sendFileDataPdu();

    //! Build and transmit an EOF PDU
    void sendEofPdu();

    //! Build and transmit a cancel (EOF with cancel condition) PDU
    void sendCancelEofPdu();

    //! Finish the current transfer and go to IDLE
    void finishTransfer(bool canceled);

    //! Send response based on caller source
    void sendResponse(SendFileStatus status);

    //! Fill in a CFDP PDU header with common fields
    void fillPduHeader(Cfdp::PduHeader& header, Cfdp::PduType pduType, U16 dataFieldLength) const;

    //! Transmit a serialized PDU buffer
    void transmitPdu(const U8* data, U32 size);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    bool m_configured;          //!< Whether configure() has been called

    //! CFDP entity configuration
    U64 m_localEntityId;
    U64 m_remoteEntityId;
    U32 m_pduBufferSize;

    //! Transaction state
    Mode m_mode;
    Os::Mutex m_modeMutex;
    U32 m_transactionSeqNum;    //!< Next transaction sequence number
    TransferEntry m_curEntry;   //!< Current in-progress transfer

    //! File state
    Os::File m_file;
    FwSizeType m_fileSize;
    U32 m_byteOffset;          //!< Current byte offset in file
    U32 m_endOffset;           //!< End byte offset
    CFDP::Checksum m_checksum; //!< Running checksum

    //! Telemetry counters
    U32 m_filesSent;
    U32 m_pdusSent;
    U32 m_warningCount;
    U64 m_totalBytesSent;

    //! Context ID for port-initiated requests
    U32 m_cntxId;

    //! Internal buffer for serializing PDUs
    static constexpr U32 PDU_SERIALIZE_BUFFER_SIZE = 2048;
    U8 m_serializeBuffer[PDU_SERIALIZE_BUFFER_SIZE];
};

}  // namespace Svc

#endif
