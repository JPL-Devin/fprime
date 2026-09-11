// ======================================================================
// \title  CcsdsTestUtils.cpp
// \author bocchino
// \brief  Test utilities for CCSDS
// ======================================================================

#include "Svc/Ccsds/TestUtils/TestUtils.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/TCHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TCTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"

namespace Svc {

namespace CcsdsTestUtils {

using SerialType = ComCfg::Apid::SerialType;
using ApidOption = Fw::Optional<ComCfg::Apid::T>;

ApidOption getRandomApid() {
    const SerialType selectedIdx = static_cast<SerialType>(STest::Random::startLength(0, ComCfg::Apid::NUM_CONSTANTS));
    // Choose from within the bounds provided by CCSDS and the SerialType.
    // 1. We have to respect the CCSDS bound because we are testing CCSDS code
    // 2. We have to respect the SerialType bound because F Prime may not be configured
    // to use CCSDS, but we still want the test to be valid, if possible.
    constexpr auto ccsdsBound = static_cast<SerialType>((1 << 11) - 1);  // 11 bits
    constexpr auto serialTypeBound = std::numeric_limits<SerialType>::max();
    constexpr auto bound = std::min(ccsdsBound, serialTypeBound);
    // Search through the interval [0, maxApid] until we find a valid APID at the
    // selected index, or we run out of numbers
    SerialType idx = 0;
    SerialType apid = 0;
    // Break the loop checking into two operations so that g++ can't report a Wtype-limits warning
    // when bound is the type max (since neither individual check is always true, but <= would be)
    for (SerialType candidateApid = 0; candidateApid < bound || candidateApid == bound; candidateApid++) {
        if (ComCfg::Apid::isValid(candidateApid)) {
            // Found a valid APID: store it
            apid = candidateApid;
            if (idx == selectedIdx) {
                // We are at the selected index: done
                break;
            }
            // Not yet at the selected index: keep going
            // We'll either go onto the next valid APID or use the current one
            // if we run off the end of the 11-bit range
            idx++;
        }
    }
    // If the APID we found is not valid, then return NONE
    // This can happen if all of the configured APIDs are out of the 11-bit range
    // required by CCSDS
    const auto result = ComCfg::Apid::isValid(apid) ? ApidOption(static_cast<ComCfg::Apid::T>(apid)) : Fw::NONE;
    return result;
}

bool buildTcFrame(const TcFrameFields& fields, const Fw::Buffer& payload, Fw::Buffer& frame) {
    using Svc::Ccsds::TCHeader;
    using Svc::Ccsds::TCTrailer;
    namespace TCSegmentHeader = Svc::Ccsds::TCSegmentHeader;
    namespace TCSubfields = Svc::Ccsds::TCSubfields;
    // Frame Length is a 10-bit field holding (total octets - 1): frames are at most 1024 octets
    constexpr FwSizeType MAX_FRAME_LENGTH = static_cast<FwSizeType>(TCSubfields::FrameLengthMask) + 1;

    const FwSizeType segmentHeaderLength =
        fields.withSegmentHeader ? static_cast<FwSizeType>(TCSegmentHeader::Size) : 0;
    const FwSizeType frameLength = static_cast<FwSizeType>(TCHeader::SERIALIZED_SIZE) + segmentHeaderLength +
                                   payload.getSize() + static_cast<FwSizeType>(TCTrailer::SERIALIZED_SIZE);
    if ((frameLength > MAX_FRAME_LENGTH) || (frameLength > frame.getSize())) {
        return false;
    }

    U8* const out = frame.getData();
    const U16 lengthToken = static_cast<U16>(frameLength - 1);
    U16 flagsAndScId = static_cast<U16>(fields.scid & TCSubfields::SpacecraftIdMask);
    if (fields.bypass) {
        flagsAndScId = static_cast<U16>(flagsAndScId | TCSubfields::BypassFlagMask);
    }
    if (fields.control) {
        flagsAndScId = static_cast<U16>(flagsAndScId | TCSubfields::ControlFlagMask);
    }
    const U16 vcIdAndLength =
        static_cast<U16>(((static_cast<U16>(fields.vcid) << TCSubfields::VcIdOffset) & TCSubfields::VcIdMask) |
                         (lengthToken & TCSubfields::FrameLengthMask));

    FwSizeType offset = 0;
    out[offset++] = static_cast<U8>(flagsAndScId >> 8);
    out[offset++] = static_cast<U8>(flagsAndScId & 0xFF);
    out[offset++] = static_cast<U8>(vcIdAndLength >> 8);
    out[offset++] = static_cast<U8>(vcIdAndLength & 0xFF);
    out[offset++] = fields.sequence;
    if (fields.withSegmentHeader) {
        out[offset++] = fields.segmentHeader;
    }
    for (FwSizeType i = 0; i < payload.getSize(); i++) {
        out[offset++] = payload.getData()[i];
    }
    const U16 crc = Svc::Ccsds::Utils::CRC16::compute(out, static_cast<U32>(offset));
    out[offset++] = static_cast<U8>(crc >> 8);
    out[offset++] = static_cast<U8>(crc & 0xFF);
    FW_ASSERT(offset == frameLength, static_cast<FwAssertArgType>(offset), static_cast<FwAssertArgType>(frameLength));

    frame.setSize(frameLength);
    return true;
}

bool buildTcFrame(bool bypass,
                  bool control,
                  U16 scid,
                  U8 vcid,
                  U8 seq,
                  const Fw::Buffer& payload,
                  bool withSegmentHeader,
                  U8 sh,
                  Fw::Buffer& frame) {
    TcFrameFields fields;
    fields.bypass = bypass;
    fields.control = control;
    fields.scid = scid;
    fields.vcid = vcid;
    fields.sequence = seq;
    fields.withSegmentHeader = withSegmentHeader;
    fields.segmentHeader = sh;
    return buildTcFrame(fields, payload, frame);
}

U8 makeSegmentHeader(U8 sequenceFlags, U8 mapId) {
    namespace TCSegmentHeader = Svc::Ccsds::TCSegmentHeader;
    return static_cast<U8>(((static_cast<U8>(sequenceFlags << TCSegmentHeader::SequenceFlagsOffset)) &
                            TCSegmentHeader::SequenceFlagsMask) |
                           (mapId & TCSegmentHeader::MapIdMask));
}

}  // namespace CcsdsTestUtils

}  // namespace Svc
