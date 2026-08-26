// ======================================================================
// \title  UslpFramer.hpp
// \author Devin
// \brief  hpp file for UslpFramer component implementation class
// ======================================================================

#ifndef Svc_Ccsds_UslpFramer_HPP
#define Svc_Ccsds_UslpFramer_HPP

#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/USLPHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTfdfHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTrailerSerializableAc.hpp"
#include "Svc/Ccsds/UslpFramer/UslpFramerComponentAc.hpp"

namespace Svc {

namespace Ccsds {

class UslpFramer final : public UslpFramerComponentBase {
    friend class UslpFramerTester;

    //! Length in octets of the Virtual Channel Frame Count field (4.1.2.11.4)
    static constexpr FwSizeType VCF_COUNT_LENGTH = sizeof(U32);
    //! Length in octets of the First Header Pointer field of the TFDF header (4.1.4.2.4)
    static constexpr FwSizeType FHP_LENGTH = sizeof(U16);
    //! Total overhead: primary header (7) + VCF count (4) + TFDF header (3) + FECF (2)
    static constexpr FwSizeType FRAME_OVERHEAD = USLPHeader::SERIALIZED_SIZE + VCF_COUNT_LENGTH +
                                                 USLPTfdfHeader::SERIALIZED_SIZE + FHP_LENGTH +
                                                 USLPTrailer::SERIALIZED_SIZE;

    static_assert(ComCfg::UslpFrameFixedSize > FRAME_OVERHEAD,
                  "USLP Frame Fixed Size must be at least large enough to hold header, VCF count, "
                  "TFDF header and trailer");
    // The Frame Length field carries total octets minus 1 in 16 bits (4.1.2.7)
    static_assert(ComCfg::UslpFrameFixedSize <= 0x10000,
                  "USLP Frame Fixed Size must fit in the 16-bit Frame Length field (total octets minus 1)");

    static constexpr FwSizeType UslpPayloadCapacity = ComCfg::UslpFrameFixedSize - FRAME_OVERHEAD;

    // Ensure the frame can hold any packet coming from the F Prime communications stack.
    // Unlike TM, no extra idle-packet room is needed: a zero-octet gap requires no fill
    static_assert(UslpPayloadCapacity >= FW_COM_BUFFER_MAX_SIZE,
                  "USLP Frame Fixed Size must be at least large enough to hold frame overhead and a full com buffer");
    static_assert(UslpPayloadCapacity >= FW_FILE_BUFFER_MAX_SIZE,
                  "USLP Frame Fixed Size must be at least large enough to hold frame overhead and a full file buffer");
    static_assert(UslpPayloadCapacity >= ComCfg::AggregationSize,
                  "USLP payload capacity must hold a full ComAggregator buffer");

    static constexpr U8 IDLE_DATA_PATTERN = 0x55;

    enum class BufferOwnershipState {
        NOT_OWNED,  //!< The buffer is currently not owned by the UslpFramer
        OWNED,      //!< The buffer is currently owned by the UslpFramer
    };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct UslpFramer object
    UslpFramer(const char* const compName  //!< The component name
    );

    //! Destroy UslpFramer object
    ~UslpFramer();

    //! Configure the UslpFramer
    //!
    //! \param vcId Virtual Channel ID (6 bits, per CCSDS 732.1-B-3 4.1.2.4)
    //! \param mapId Multiplexer Access Point ID (4 bits, per CCSDS 732.1-B-3 4.1.2.5)
    void configure(U8 vcId, U8 mapId);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comStatusIn
    //!
    //! Port receiving the general status from the downstream component
    //! indicating it is ready or not-ready for more input
    void comStatusIn_handler(FwIndexType portNum,    //!< The port number
                             Fw::Success& condition  //!< Condition success/failure
                             ) override;

    //! Handler implementation for dataIn
    //!
    //! Port to receive data to frame, in a Fw::Buffer with optional context.
    //! This is essentially the CCSDS USLP MAPP.request Service Primitive
    //! (USLP Protocol 3.4.3.2)
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Buffer coming from a deallocate call in a ComDriver component
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------
  private:
    //! Complete the fixed-length TFDZ with an Encapsulation Idle Packet
    //! (CCSDS 133.1-B-3 4.1.3.2) as per CCSDS 732.1-B-3 paragraph 4.1.4.3.4.
    //! The idle packet is inserted at the current serialization offset and
    //! fills the TFDZ up to the end of the frame minus the FECF
    void fill_with_idle_packet(Fw::SerialBufferBase& serializer);

    // ----------------------------------------------------------------------
    // Members
    // ----------------------------------------------------------------------
  private:
    // Because the USLP protocol uses fixed width frames, and only one frame is in transit between ComQueue and
    // ComInterface at a time, we can use a member fixed-size buffer to hold the frame data
    U8 m_frameBuffer[ComCfg::UslpFrameFixedSize];                      //!< Buffer to hold the frame data
    BufferOwnershipState m_bufferState = BufferOwnershipState::OWNED;  //!< whether m_frameBuffer is owned by UslpFramer

    U32 m_vcFrameCount;  //!< Virtual Channel Frame Count - 4 octets on the wire - wraps around at 2^32 - 1
    U8 m_vcId;           //!< Virtual Channel ID - 6 bits
    U8 m_mapId;          //!< Multiplexer Access Point ID - 4 bits
};

}  // namespace Ccsds
}  // namespace Svc

#endif
