// ======================================================================
// \title  TmFramer.cpp
// \author thomas-bc
// \brief  cpp file for TmFramer component implementation class
// ======================================================================

#include "Svc/Ccsds/TmFramer/TmFramer.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "config/FppConstantsAc.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TmFramer ::TmFramer(const char* const compName)
    : TmFramerComponentBase(compName), m_masterFrameCount(0), m_virtualFrameCount(0) {}

TmFramer ::~TmFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void TmFramer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    const U16 canonicalFhp = context.get_firstHeaderPointer();
    if (canonicalFhp == static_cast<U16>(ComCfg::FhpValues::FHP_UNSET)) {
        // No packer in the chain: legacy one-packet-per-frame behavior
        this->frame_legacy(data, context);
    } else if (context.get_zeroCopyFrame()) {
        // Packer-built data zone with reserved headroom/trailer: frame in place
        this->frame_zero_copy(data, context);
    } else {
        // Packer-built data zone without reserves (e.g. SDLS allocate-and-copy fallback)
        this->frame_packed_copy(data, context);
    }
}

void TmFramer ::frame_legacy(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    FW_ASSERT(ComCfg::TmFramerLegacyPathEnabled != 0);
    FW_ASSERT(data.getSize() <= ComCfg::TmFrameFixedSize - TMHeader::SERIALIZED_SIZE - TMTrailer::SERIALIZED_SIZE,
              static_cast<FwAssertArgType>(data.getSize()));
    FW_ASSERT(this->m_bufferState == BufferOwnershipState::OWNED, static_cast<FwAssertArgType>(this->m_bufferState));

    // First Header Pointer is always 0 since we are always wrapping a single entire packet at offset 0
    TMHeader header = this->build_header(context, 0);

    // -------------------------------------------------
    // Data field
    // -------------------------------------------------
    // Payload packet
    Fw::SerializeStatus status;
    // Create frame Fw::Buffer using member data field
    Fw::Buffer frameBuffer = Fw::Buffer(this->m_frameBuffer, sizeof(this->m_frameBuffer));
    auto frameSerializer = frameBuffer.getSerializer();
    status = frameSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = frameSerializer.serializeFrom(data.getData(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // As per TM Standard 4.2.2.5, fill the rest of the data field with an Idle Packet
    this->fill_with_idle_packet(frameSerializer);

    inject_fecf(frameBuffer);

    this->m_bufferState = BufferOwnershipState::NOT_OWNED;
    this->dataOut_out(0, frameBuffer, context);
    this->dataReturnOut_out(0, data, context);  // return ownership of the original data buffer
}

void TmFramer ::frame_zero_copy(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // One frame in flight at a time per the Communication Adapter Interface
    FW_ASSERT(!this->m_zeroCopyInFlight);
    // The data zone must exactly fill the frame data field
    FW_ASSERT(data.getSize() == TmPayloadCapacity, static_cast<FwAssertArgType>(data.getSize()));
    // The buffer must carry headroom for the TM primary header and reserve for the trailer
    FW_ASSERT(data.getOffset() >= TMHeader::SERIALIZED_SIZE, static_cast<FwAssertArgType>(data.getOffset()));
    FW_ASSERT(data.getOffset() + data.getSize() + TMTrailer::SERIALIZED_SIZE <= data.getCapacity(),
              static_cast<FwAssertArgType>(data.getOffset()), static_cast<FwAssertArgType>(data.getCapacity()));

    TMHeader header = this->build_header(context, map_fhp(context.get_firstHeaderPointer()));

    // Expand the buffer window to the full frame: header headroom in front, trailer reserve behind
    data.advance(-static_cast<FwSignedSizeType>(TMHeader::SERIALIZED_SIZE));
    data.setSize(ComCfg::TmFrameFixedSize);

    auto frameSerializer = data.getSerializer();
    Fw::SerializeStatus status = frameSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    inject_fecf(data);

    // The zone buffer is owned upstream: hold the return until the frame comes back
    this->m_zeroCopyInFlight = true;
    this->dataOut_out(0, data, context);
}

void TmFramer ::frame_packed_copy(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // The data zone must exactly fill the frame data field (the packer idle-fills partial zones)
    FW_ASSERT(data.getSize() == TmPayloadCapacity, static_cast<FwAssertArgType>(data.getSize()));
    FW_ASSERT(this->m_bufferState == BufferOwnershipState::OWNED, static_cast<FwAssertArgType>(this->m_bufferState));

    TMHeader header = this->build_header(context, map_fhp(context.get_firstHeaderPointer()));

    Fw::SerializeStatus status;
    Fw::Buffer frameBuffer = Fw::Buffer(this->m_frameBuffer, sizeof(this->m_frameBuffer));
    auto frameSerializer = frameBuffer.getSerializer();
    status = frameSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = frameSerializer.serializeFrom(data.getData(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    inject_fecf(frameBuffer);

    this->m_bufferState = BufferOwnershipState::NOT_OWNED;
    this->dataOut_out(0, frameBuffer, context);
    this->dataReturnOut_out(0, data, context);  // return ownership of the data zone buffer
}

TMHeader TmFramer ::build_header(const ComCfg::FrameContext& context, U16 wireFhp) {
    TMHeader header;

    // GVCID (Global Virtual Channel ID) (Standard 4.1.2.2 and 4.1.2.3)
    U16 globalVcId = static_cast<U16>(context.get_vcId() << TMSubfields::virtualChannelIdOffset);
    globalVcId |= static_cast<U16>(ComCfg::SpacecraftId << TMSubfields::spacecraftIdOffset);
    globalVcId |= 0x0;  // Operational Control Field: Flag set to 0 (Standard 4.1.2.4)

    // Data Field Status (Standard 4.1.2.7):
    // - all flags to 0 except segment length id 0b11 per standard (4.1.2.7)
    // - First Header Pointer in the 11 least significant bits (4.1.2.7.6)
    U16 dataFieldStatus = 0;
    dataFieldStatus |= 0x3 << TMSubfields::segLengthOffset;  // Seg Length Id '11' (0x3) per Standard (4.1.2.7.5)
    dataFieldStatus |= static_cast<U16>(wireFhp & TMSubfields::firstHeaderPtrMask);

    header.set_globalVcId(globalVcId);
    header.set_masterFrameCount(this->m_masterFrameCount);
    header.set_virtualFrameCount(this->m_virtualFrameCount);
    header.set_dataFieldStatus(dataFieldStatus);

    // We use only a single Virtual Channel for now, so master and virtual frame counts are the same
    this->m_masterFrameCount++;   // U8 intended to wrap around (modulo 256)
    this->m_virtualFrameCount++;  // U8 intended to wrap around (modulo 256)

    return header;
}

U16 TmFramer ::map_fhp(U16 canonicalFhp) {
    U16 wireFhp = canonicalFhp;
    if (canonicalFhp == static_cast<U16>(ComCfg::FhpValues::FHP_NO_PACKET_START)) {
        wireFhp = TMSubfields::FHP_NO_PACKET_START;
    } else if (canonicalFhp == static_cast<U16>(ComCfg::FhpValues::FHP_IDLE_DATA_ONLY)) {
        wireFhp = TMSubfields::FHP_IDLE_DATA_ONLY;
    } else {
        // Byte offsets must be encodable below the 11-bit TM sentinel values
        FW_ASSERT(canonicalFhp < TMSubfields::FHP_IDLE_DATA_ONLY, static_cast<FwAssertArgType>(canonicalFhp));
    }
    return wireFhp;
}

void TmFramer ::inject_fecf(Fw::Buffer& frameBuffer) {
    FW_ASSERT(frameBuffer.getSize() == ComCfg::TmFrameFixedSize, static_cast<FwAssertArgType>(frameBuffer.getSize()));
    TMTrailer trailer;
    // Compute CRC over the entire frame buffer minus the FECF trailer (Standard 4.1.6)
    constexpr FwSizeType fecfStart = ComCfg::TmFrameFixedSize - TMTrailer::SERIALIZED_SIZE;
    U16 crc = Ccsds::Utils::CRC16::compute(frameBuffer.getData(), fecfStart);
    // Set the Frame Error Control Field (FECF)
    trailer.set_fecf(crc);
    // Move the serializer pointer to the end of the location where the trailer will be serialized
    auto frameSerializer = frameBuffer.getSerializer();
    Fw::SerializeStatus status = frameSerializer.moveSerToOffset(fecfStart);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = frameSerializer.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
}

void TmFramer ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    if (this->isConnected_comStatusOut_OutputPort(portNum)) {
        this->comStatusOut_out(portNum, condition);
    }
}

void TmFramer ::dataReturnIn_handler(FwIndexType portNum,
                                     Fw::Buffer& frameBuffer,
                                     const ComCfg::FrameContext& context) {
    const bool isMemberBuffer = (frameBuffer.getData() >= &this->m_frameBuffer[0]) &&
                                (frameBuffer.getData() < &this->m_frameBuffer[0] + sizeof(this->m_frameBuffer));
    if (isMemberBuffer) {
        this->m_bufferState = BufferOwnershipState::OWNED;
    } else {
        // An upstream-owned zero-copy frame: forward ownership back upstream
        FW_ASSERT(this->m_zeroCopyInFlight);
        this->m_zeroCopyInFlight = false;
        this->dataReturnOut_out(0, frameBuffer, context);
    }
}

void TmFramer ::fill_with_idle_packet(Fw::SerialBufferBase& serializer) {
    constexpr U16 endIndex = ComCfg::TmFrameFixedSize - TMTrailer::SERIALIZED_SIZE;
    constexpr U16 idleApid = static_cast<U16>(ComCfg::Apid::SPP_IDLE_PACKET);
    const U16 startIndex = static_cast<U16>(serializer.getSize());
    const U16 idlePacketSize = static_cast<U16>(endIndex - startIndex);
    // Length token is defined as the number of bytes of payload data minus 1
    const U16 lengthToken = static_cast<U16>(idlePacketSize - SpacePacketHeader::SERIALIZED_SIZE - 1);

    FW_ASSERT(idlePacketSize >= 7, static_cast<FwAssertArgType>(idlePacketSize));  // 7 bytes minimum for idle packet
    FW_ASSERT(idlePacketSize <= ComCfg::TmFrameFixedSize, static_cast<FwAssertArgType>(idlePacketSize));

    SpacePacketHeader header;
    header.set_packetIdentification(idleApid);
    header.set_packetSequenceControl(
        0x3 << SpacePacketSubfields::SeqFlagsOffset);  // Sequence Flags = 0b11 (unsegmented) & unused Seq count
    header.set_packetDataLength(lengthToken);
    // Serialize header and idle data into the frame
    Fw::SerializeStatus status = serializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    for (U16 i = static_cast<U16>(startIndex + SpacePacketHeader::SERIALIZED_SIZE); i < endIndex; i++) {
        status = serializer.serializeFrom(IDLE_DATA_PATTERN);  // Idle data
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }
}
}  // namespace Ccsds
}  // namespace Svc
