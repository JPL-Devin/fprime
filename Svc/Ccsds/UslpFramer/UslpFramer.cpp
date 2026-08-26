// ======================================================================
// \title  UslpFramer.cpp
// \author Devin
// \brief  cpp file for UslpFramer component implementation class
// ======================================================================

#include "Svc/Ccsds/UslpFramer/UslpFramer.hpp"
#include "Svc/Ccsds/Types/EppLengthOfLengthEnumAc.hpp"
#include "Svc/Ccsds/Types/EppProtocolIdEnumAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "config/FppConstantsAc.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UslpFramer ::UslpFramer(const char* const compName)
    : UslpFramerComponentBase(compName), m_vcFrameCount(0), m_vcId(0), m_mapId(0) {}

UslpFramer ::~UslpFramer() {}

void UslpFramer ::configure(U8 vcId, U8 mapId) {
    // Virtual Channel ID is 6 bits (per CCSDS 732.1-B-3 Section 4.1.2.4)
    FW_ASSERT((vcId & ~0x3F) == 0, static_cast<FwAssertArgType>(vcId));
    // MAP ID is 4 bits (per CCSDS 732.1-B-3 Section 4.1.2.5)
    FW_ASSERT((mapId & ~0x0F) == 0, static_cast<FwAssertArgType>(mapId));
    this->m_vcId = vcId;
    this->m_mapId = mapId;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void UslpFramer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Unreachable given the static_asserts on UslpPayloadCapacity (com, file, and
    // aggregation buffer bounds) - programming error only
    FW_ASSERT(data.getSize() <= UslpPayloadCapacity, static_cast<FwAssertArgType>(data.getSize()));
    FW_ASSERT(this->m_bufferState == BufferOwnershipState::OWNED, static_cast<FwAssertArgType>(this->m_bufferState));

    // -----------------------------------------------
    // Transfer Frame Primary Header (Standard 4.1.2)
    // -----------------------------------------------
    USLPHeader header;

    U32 tfvnScidVcidMap = 0;
    // Transfer Frame Version Number: 0b1100 on the wire (Standard 4.1.2.2)
    tfvnScidVcidMap |= static_cast<U32>(USLPHeaderSubfields::frameVersionValue)
                       << USLPHeaderSubfields::frameVersionOffset;
    // Spacecraft ID (Standard 4.1.2.2.3)
    tfvnScidVcidMap |= static_cast<U32>(ComCfg::SpacecraftId) << USLPHeaderSubfields::spacecraftIdOffset;
    // Source-or-destination identifier: 0 = source (Standard 4.1.2.3.4)
    tfvnScidVcidMap |= 0x0 << USLPHeaderSubfields::sourceOrDestOffset;
    // Virtual Channel ID (Standard 4.1.2.4)
    tfvnScidVcidMap |= static_cast<U32>(this->m_vcId) << USLPHeaderSubfields::virtualChannelIdOffset;
    // MAP ID (Standard 4.1.2.5)
    tfvnScidVcidMap |= static_cast<U32>(this->m_mapId) << USLPHeaderSubfields::mapIdOffset;
    // End of Frame Primary Header flag: 0 = non-truncated frame (Standard 4.1.2.6)
    tfvnScidVcidMap |= 0x0 << USLPHeaderSubfields::eofphOffset;

    U8 flags = 0;
    // Bypass/Sequence Control flag: 1 = Expedited (Standard 4.1.2.8)
    flags |= 0x1 << USLPHeaderSubfields::bypassFlagOffset;
    // Protocol Control Command flag: 0 = user data (Standard 4.1.2.8.2)
    flags |= 0x0 << USLPHeaderSubfields::protocolCommandOffset;
    // Spares: 00 (Standard 4.1.2.9)
    flags |= 0x0 << USLPHeaderSubfields::spareOffset;
    // OCF flag: 0 = no Operational Control Field (Standard 4.1.2.10)
    flags |= 0x0 << USLPHeaderSubfields::ocfFlagOffset;
    // VCF Count Length: 4 octets (Standard 4.1.2.11)
    flags |= static_cast<U8>(VCF_COUNT_LENGTH) << USLPHeaderSubfields::vcfCountLengthOffset;

    header.set_tfvnScidVcidMap(tfvnScidVcidMap);
    // Frame Length: total frame octets minus 1 (Standard 4.1.2.7)
    header.set_frameLength(static_cast<U16>(ComCfg::UslpFrameFixedSize - 1));
    header.set_flags(flags);

    // -------------------------------------------------
    // Serialize header, VCF count, and TFDF header
    // -------------------------------------------------
    Fw::SerializeStatus status;
    // Create frame Fw::Buffer using member data field
    Fw::Buffer frameBuffer = Fw::Buffer(this->m_frameBuffer, sizeof(this->m_frameBuffer));
    auto frameSerializer = frameBuffer.getSerializer();
    status = frameSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Virtual Channel Frame Count: 4 octets, big-endian (Standard 4.1.2.12)
    status = frameSerializer.serializeFrom(this->m_vcFrameCount);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    this->m_vcFrameCount++;  // U32 intended to wrap around (modulo 2^32)

    // TFDF header (Standard 4.1.4.2): construction rule 000 (fixed-length TFDZ,
    // packets spanning frames) and UPID 0b00000 (Space/Encapsulation Packets)
    USLPTfdfHeader tfdfHeader;
    tfdfHeader.set_rulesAndUpid(
        static_cast<U8>((USLPTfdfSubfields::RULE_PACKETS_SPANNING << USLPTfdfSubfields::rulesOffset) |
                        USLPTfdfSubfields::UPID_SPACE_PACKETS));
    status = frameSerializer.serializeFrom(tfdfHeader);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // First Header Pointer: always 0, the packet is aligned to the start of the TFDZ (Standard 4.1.4.2.4)
    status = frameSerializer.serializeFrom(static_cast<U16>(0));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // -------------------------------------------------
    // Transfer Frame Data Zone
    // -------------------------------------------------
    status = frameSerializer.serializeFrom(data.getData(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // As per Standard 4.1.4.3.4, complete the fixed-length TFDZ with an Encapsulation Idle Packet
    this->fill_with_idle_packet(frameSerializer);

    // -------------------------------------------------
    // Trailer (FECF, Standard 4.1.6)
    // -------------------------------------------------
    USLPTrailer trailer;
    // Compute CRC over the entire frame buffer minus the FECF trailer (Standard 4.1.6)
    U16 crc =
        Ccsds::Utils::CRC16::compute(frameBuffer.getData(), sizeof(this->m_frameBuffer) - USLPTrailer::SERIALIZED_SIZE);
    trailer.set_fecf(crc);
    // Move the serializer pointer to the end of the location where the trailer will be serialized
    status = frameSerializer.moveSerToOffset(ComCfg::UslpFrameFixedSize - USLPTrailer::SERIALIZED_SIZE);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = frameSerializer.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    this->m_bufferState = BufferOwnershipState::NOT_OWNED;
    this->dataOut_out(0, frameBuffer, context);
    this->dataReturnOut_out(0, data, context);  // return ownership of the original data buffer
}

void UslpFramer ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    if (this->isConnected_comStatusOut_OutputPort(portNum)) {
        this->comStatusOut_out(portNum, condition);
    }
}

void UslpFramer ::dataReturnIn_handler(FwIndexType portNum,
                                       Fw::Buffer& frameBuffer,
                                       const ComCfg::FrameContext& context) {
    // Assert that the returned buffer is the member frame buffer, and set ownership state
    FW_ASSERT(frameBuffer.getData() == this->m_frameBuffer);
    FW_ASSERT(frameBuffer.getSize() == sizeof(this->m_frameBuffer),
              static_cast<FwAssertArgType>(frameBuffer.getSize()));
    this->m_bufferState = BufferOwnershipState::OWNED;
}

void UslpFramer ::fill_with_idle_packet(Fw::SerialBufferBase& serializer) {
    constexpr U16 endIndex = ComCfg::UslpFrameFixedSize - USLPTrailer::SERIALIZED_SIZE;
    const U16 startIndex = static_cast<U16>(serializer.getSize());
    // Gap of octets to fill between the end of the payload and the FECF
    const U16 gap = static_cast<U16>(endIndex - startIndex);

    if (gap == 0) {
        return;  // TFDZ is already full - no idle packet needed
    }

    // Encapsulation Packet first octet: 3 bits PVN (0b111) | 3 bits protocol ID | 2 bits length-of-length
    // Per CCSDS 133.1-B-3 Section 4.1.2, with Idle protocol ID per Section 4.1.3.2
    U8 firstOctet = static_cast<U8>(ComCfg::Pvn::ENCAPSULATION_PACKET_PROTOCOL << EPPSubfields::packetVersionOffset);
    firstOctet |= static_cast<U8>(EppProtocolId::Idle << EPPSubfields::protocolIdOffset);

    Fw::SerializeStatus status;
    U16 headerLength = 0;
    if (gap == 1) {
        // Single-octet Encapsulation Idle Packet: length-of-length 0b00 (CCSDS 133.1-B-3 4.1.2.7.2)
        firstOctet |= static_cast<U8>(EppLengthOfLength::Zero);
        status = serializer.serializeFrom(firstOctet);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        headerLength = 1;
    } else if (gap <= 0xFF) {
        // Two-octet header: first octet + 1-octet Packet Length (CCSDS 133.1-B-3 4.1.2.1.1)
        firstOctet |= static_cast<U8>(EppLengthOfLength::One);
        status = serializer.serializeFrom(firstOctet);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        // Packet Length field contains the total packet length in octets (CCSDS 133.1-B-3 4.1.2.6)
        status = serializer.serializeFrom(static_cast<U8>(gap));
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        headerLength = 2;
    } else {
        // Four-octet header: first octet + 1-octet Protocol ID Extension/User Defined Field +
        // 2-octet Packet Length (CCSDS 133.1-B-3 4.1.2.1.1). Sufficient for any 16-bit frame length.
        firstOctet |= static_cast<U8>(EppLengthOfLength::Two);
        status = serializer.serializeFrom(firstOctet);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        status = serializer.serializeFrom(static_cast<U8>(0));
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        status = serializer.serializeFrom(gap);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        headerLength = 4;
    }

    // Fill the remainder of the idle packet with the idle data pattern
    for (U16 i = static_cast<U16>(startIndex + headerLength); i < endIndex; i++) {
        status = serializer.serializeFrom(IDLE_DATA_PATTERN);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }
}

}  // namespace Ccsds
}  // namespace Svc
