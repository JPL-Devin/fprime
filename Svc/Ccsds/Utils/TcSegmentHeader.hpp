// ======================================================================
// \title  TcSegmentHeader.hpp
// \brief  Field extraction for the one-octet CCSDS TC Segment Header
// ======================================================================

#ifndef Svc_Ccsds_Utils_TcSegmentHeader_HPP
#define Svc_Ccsds_Utils_TcSegmentHeader_HPP

#include "Fw/FPrimeBasicTypes.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/TcSequenceFlagsEnumAc.hpp"

namespace Svc {
namespace Ccsds {
namespace Utils {

//! Decoder for the TC Segment Header octet (CCSDS 232.0-B-4 4.1.3.2.2): two Sequence Flags bits
//! (MSB first) followed by a six-bit MAP ID. The octet travels raw in
//! ComCfg::FrameContext.tcSegmentHeader so that it can be re-inserted byte-for-byte into the SDLS
//! authenticated data; consumers decode it with these helpers.
struct TcSegmentHeader final {
    static constexpr U8 SEQUENCE_FLAGS_MASK = static_cast<U8>(TCSegmentHeader::SequenceFlagsMask);
    static constexpr U8 SEQUENCE_FLAGS_SHIFT = static_cast<U8>(TCSegmentHeader::SequenceFlagsOffset);
    static constexpr U8 MAP_ID_MASK = static_cast<U8>(TCSegmentHeader::MapIdMask);
    static constexpr U8 MAP_ID_MAX = static_cast<U8>(TCSegmentHeader::MapIdMax);

    //! Sequence Flags of the octet (4.1.3.2.2.2); every two-bit value is a defined flag
    static constexpr TcSequenceFlags::T sequenceFlags(U8 sh) {
        return static_cast<TcSequenceFlags::T>((sh & SEQUENCE_FLAGS_MASK) >> SEQUENCE_FLAGS_SHIFT);
    }

    //! MAP ID of the octet (4.1.3.2.2.3), always in 0..MAP_ID_MAX
    static constexpr U8 mapId(U8 sh) { return static_cast<U8>(sh & MAP_ID_MASK); }

    //! Build the octet from its fields; inverse of sequenceFlags()/mapId()
    static constexpr U8 encode(TcSequenceFlags::T flags, U8 mapId) {
        return static_cast<U8>(((static_cast<U8>(flags) << SEQUENCE_FLAGS_SHIFT) & SEQUENCE_FLAGS_MASK) |
                               (mapId & MAP_ID_MASK));
    }

    //! True when mapId is representable in the six-bit MAP ID field
    static constexpr bool isValidMapId(U8 mapId) { return mapId <= MAP_ID_MAX; }
};

static_assert(TcSegmentHeader::SEQUENCE_FLAGS_MASK == 0xC0, "TC Segment Header Sequence Flags occupy bits 0-1");
static_assert(TcSegmentHeader::MAP_ID_MASK == 0x3F, "TC Segment Header MAP ID occupies bits 2-7");
static_assert((TcSegmentHeader::SEQUENCE_FLAGS_MASK | TcSegmentHeader::MAP_ID_MASK) == 0xFF,
              "TC Segment Header fields cover the whole octet");
static_assert(TCSegmentHeader::Size == 1, "TC Segment Header is one octet");

}  // namespace Utils
}  // namespace Ccsds
}  // namespace Svc

#endif
