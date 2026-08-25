// ======================================================================
// \title  UslpDeframer.cpp
// \author thomas-bc
// \brief  cpp file for UslpDeframer component implementation class
// ======================================================================

#include "Svc/Ccsds/UslpDeframer/UslpDeframer.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/USLPHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTfdfHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "config/FpConfig.hpp"

namespace Svc {
namespace Ccsds {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UslpDeframer ::UslpDeframer(const char* const compName)
    : UslpDeframerComponentBase(compName),
      m_vcId(0),
      m_spacecraftId(ComCfg::SpacecraftId),
      m_mapId(0),
      m_vcfCountLength(0),
      m_acceptAllVcid(false),
      m_framesProcessed(0),
      m_crcErrorCount(0) {}

UslpDeframer ::~UslpDeframer() {}

void UslpDeframer::configure(U8 vcId, U16 spacecraftId, U8 mapId, U8 vcfCountLength, bool acceptAllVcid) {
    // Configuration values are trusted input - assert on bit-width violations
    FW_ASSERT(vcId < (1 << 6), vcId);                // VCID is 6 bits
    FW_ASSERT(mapId < (1 << 4), mapId);              // MAP ID is 4 bits
    FW_ASSERT(vcfCountLength <= 7, vcfCountLength);  // VCF Count Length is 3 bits (0-7 octets)
    this->m_vcId = vcId;
    this->m_spacecraftId = spacecraftId;
    this->m_mapId = mapId;
    this->m_vcfCountLength = vcfCountLength;
    this->m_acceptAllVcid = acceptAllVcid;
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void UslpDeframer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // CCSDS USLP Transfer Frame (CCSDS 732.1-B-3), non-truncated:
    // 7 octets  - Transfer Frame Primary Header
    //   4b  - Transfer Frame Version Number (0b1100)
    //   16b - Spacecraft ID
    //   1b  - Source-or-Destination Identifier (1 = spacecraft is destination)
    //   6b  - Virtual Channel ID
    //   4b  - MAP ID
    //   1b  - End of Frame Primary Header flag (0 = non-truncated frame)
    //   16b - Frame Length (total octets minus 1)
    //   1b  - Bypass/Sequence Control flag
    //   1b  - Protocol Control Command flag
    //   2b  - Reserved spares (0b00)
    //   1b  - OCF flag
    //   3b  - VCF Count Length (octets)
    // 0-7 octets - Virtual Channel Frame Count (per VCF Count Length)
    // 1 octet    - TFDF Header (3b construction rule, 5b UPID)
    // Variable   - Transfer Frame Data Zone
    // 2 octets   - Frame Error Control Field (CRC16)
    const FwSizeType minFrameSize = static_cast<FwSizeType>(USLPHeader::SERIALIZED_SIZE) +
                                    USLPTfdfHeader::SERIALIZED_SIZE + USLPTrailer::SERIALIZED_SIZE;
    if (data.getSize() <= minFrameSize) {
        // Incoming buffer is not long enough to contain a valid frame with a non-empty data zone
        this->log_WARNING_LO_InvalidPacket();
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_LENGTH);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    USLPHeader header;
    Fw::SerializeStatus status = data.getDeserializer().deserializeTo(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);  // size guaranteed by the check above

    const U32 idWord = header.get_tfvnScidVcidMap();
    const U8 tfvn =
        static_cast<U8>((idWord & USLPHeaderSubfields::frameVersionMask) >> USLPHeaderSubfields::frameVersionOffset);
    if (tfvn != USLPHeaderSubfields::frameVersionValue) {
        this->log_WARNING_LO_InvalidFrameVersion(tfvn, static_cast<U8>(USLPHeaderSubfields::frameVersionValue));
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_VERSION);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    if ((idWord & USLPHeaderSubfields::eofphMask) != 0) {
        // End of Frame Primary Header flag set means a truncated frame, which is not supported
        this->log_WARNING_LO_TruncatedFrameNotSupported();
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    const U16 spacecraftId =
        static_cast<U16>((idWord & USLPHeaderSubfields::spacecraftIdMask) >> USLPHeaderSubfields::spacecraftIdOffset);
    if (spacecraftId != this->m_spacecraftId) {
        this->log_WARNING_LO_InvalidSpacecraftId(spacecraftId, this->m_spacecraftId);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_SCID);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    const U8 sourceOrDest =
        static_cast<U8>((idWord & USLPHeaderSubfields::sourceOrDestMask) >> USLPHeaderSubfields::sourceOrDestOffset);
    if (sourceOrDest != 1) {
        // On uplink the spacecraft is the destination (CCSDS 732.1-B-3 4.1.2.3.3)
        this->log_WARNING_LO_InvalidSourceOrDest(sourceOrDest);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    // USLP defines the Frame Length as total octets minus 1; widen before adding 1 back to avoid U16 wrap
    const FwSizeType totalLength = static_cast<FwSizeType>(header.get_frameLength()) + 1;
    if (totalLength != static_cast<FwSizeType>(data.getSize())) {
        this->log_WARNING_HI_InvalidFrameLength(header.get_frameLength(), static_cast<FwSizeType>(data.getSize()));
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_LENGTH);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    const U8 vcId = static_cast<U8>((idWord & USLPHeaderSubfields::virtualChannelIdMask) >>
                                    USLPHeaderSubfields::virtualChannelIdOffset);
    if ((not this->m_acceptAllVcid) && (vcId != this->m_vcId)) {
        this->log_ACTIVITY_LO_InvalidVcId(vcId, this->m_vcId);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_VCID);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    const U8 mapId = static_cast<U8>((idWord & USLPHeaderSubfields::mapIdMask) >> USLPHeaderSubfields::mapIdOffset);
    if (mapId != this->m_mapId) {
        this->log_ACTIVITY_LO_InvalidMapId(mapId, this->m_mapId);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_MAP);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    const U8 flags = header.get_flags();
    if ((flags & USLPHeaderSubfields::protocolCommandMask) != 0) {
        // Protocol control command frames are not user data and are not supported
        this->log_WARNING_LO_ProtocolCommandNotSupported();
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    if ((flags & USLPHeaderSubfields::spareMask) != 0) {
        const U8 spares = static_cast<U8>((flags & USLPHeaderSubfields::spareMask) >> USLPHeaderSubfields::spareOffset);
        this->log_WARNING_LO_InvalidSpareBits(spares);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    if ((flags & USLPHeaderSubfields::ocfFlagMask) != 0) {
        // Operational Control Field is not supported
        this->log_WARNING_LO_OcfNotSupported();
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    const U8 vcfCountLength = static_cast<U8>(flags & USLPHeaderSubfields::vcfCountLengthMask);
    if (vcfCountLength != this->m_vcfCountLength) {
        this->log_WARNING_LO_InvalidVcfCountLength(vcfCountLength, this->m_vcfCountLength);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_HEADER);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    // Guard against underflow: the frame must be able to hold header + VCF count + TFDF header + trailer
    const FwSizeType headerAndTfdfSize =
        static_cast<FwSizeType>(USLPHeader::SERIALIZED_SIZE) + vcfCountLength + USLPTfdfHeader::SERIALIZED_SIZE;
    if ((headerAndTfdfSize + USLPTrailer::SERIALIZED_SIZE) > totalLength) {
        this->log_WARNING_HI_InvalidFrameLength(header.get_frameLength(), static_cast<FwSizeType>(data.getSize()));
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_LENGTH);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    // -------------------------------------------------
    // CRC Check
    // -------------------------------------------------
    // Compute CRC over the entire frame buffer minus the FECF trailer
    const U16 computedCrc =
        Ccsds::Utils::CRC16::compute(data.getData(), static_cast<U32>(totalLength - USLPTrailer::SERIALIZED_SIZE));
    USLPTrailer trailer;
    auto deserializer = data.getDeserializer();
    status = deserializer.moveDeserToOffset(totalLength - USLPTrailer::SERIALIZED_SIZE);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = deserializer.deserializeTo(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    const U16 transmittedCrc = trailer.get_fecf();
    if (transmittedCrc != computedCrc) {
        this->log_WARNING_HI_InvalidCrc(transmittedCrc, computedCrc);
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_CRC);
        this->m_crcErrorCount++;
        this->tlmWrite_CrcErrorCount(this->m_crcErrorCount);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    // -------------------------------------------------
    // TFDF Header Check
    // -------------------------------------------------
    USLPTfdfHeader tfdfHeader;
    auto tfdfDeserializer = data.getDeserializer();
    status = tfdfDeserializer.moveDeserToOffset(static_cast<FwSizeType>(USLPHeader::SERIALIZED_SIZE) + vcfCountLength);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = tfdfDeserializer.deserializeTo(tfdfHeader);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    const U8 rulesAndUpid = tfdfHeader.get_rulesAndUpid();
    const U8 rule = static_cast<U8>((rulesAndUpid & USLPTfdfSubfields::rulesMask) >> USLPTfdfSubfields::rulesOffset);
    if (rule != USLPTfdfSubfields::RULE_NO_SEGMENTATION) {
        this->log_WARNING_LO_InvalidTfdfRule(rule, static_cast<U8>(USLPTfdfSubfields::RULE_NO_SEGMENTATION));
        this->errorNotifyHelper(Ccsds::FrameError::USLP_INVALID_TFDF);
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    const U8 upid = static_cast<U8>(rulesAndUpid & USLPTfdfSubfields::upidMask);
    this->log_DIAGNOSTIC_UpidReceived(upid);

    // Point to the start of the data zone and set appropriate size
    data.advance(headerAndTfdfSize);
    // Shrink size to that of the encapsulated data zone ( header | vcf count | tfdf header | data | trailer )
    data.setSize(totalLength - headerAndTfdfSize - USLPTrailer::SERIALIZED_SIZE);

    ComCfg::FrameContext outContext = context;
    outContext.set_vcId(vcId);
    this->dataOut_out(0, data, outContext);

    this->m_framesProcessed++;
    this->tlmWrite_FramesProcessed(this->m_framesProcessed);
}

void UslpDeframer ::dataReturnIn_handler(FwIndexType portNum,
                                         Fw::Buffer& fwBuffer,
                                         const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, fwBuffer, context);
}

void UslpDeframer::errorNotifyHelper(Ccsds::FrameError error) {
    if (this->isConnected_errorNotify_OutputPort(0)) {
        this->errorNotify_out(0, error);
    }
}

}  // namespace Ccsds
}  // namespace Svc
