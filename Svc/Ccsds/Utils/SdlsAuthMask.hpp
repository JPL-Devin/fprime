// ======================================================================
// \title  SdlsAuthMask.hpp
// \brief  SDLS additional authenticated data (AAD) construction
// ======================================================================

#ifndef Svc_Ccsds_Utils_SdlsAuthMask_HPP
#define Svc_Ccsds_Utils_SdlsAuthMask_HPP

#include <cstring>
#include "Fw/FPrimeBasicTypes.hpp"

namespace Svc {
namespace Ccsds {
namespace Utils {

// AES-GCM authenticates, but does not encrypt, a block of frame fields supplied as AAD. SDLS
// uses this to bind a frame to its virtual channel and security association. The block is a
// masked copy of the frame header: fields that differ between the two ends are zeroed.

//! AAD for an SDLS-protected TC (uplink) transfer frame: the masked 5-byte primary header,
//! the received TC Segment Header octet when the frame carried one (CCSDS 232.0-B-4 4.1.3.2.2;
//! CCSDS 355.0-B-2 2.3.2.2 always authenticates it), the SPI verbatim, then a zeroed 12-byte
//! IV field. `size` holds the number of valid octets in `bytes`:
//!
//!   no SH (19):   [0..4] 00 00 (vcId<<2)&FC 00 00 | [5..6] SPI | [7..18] 00 x12
//!   with SH (20): [0..4] 00 00 (vcId<<2)&FC 00 00 | [5] SH  | [6..7] SPI | [8..19] 00 x12
//!
//! The Segment Header is copied in unchanged, so the AAD is bit-identical to the mask applied
//! to the frame as received; callers must pass `size`, never `sizeof(bytes)`, to the cipher.
struct SdlsTcAuthMask final {
    static constexpr FwSizeType TC_PRIMARY_HEADER_SIZE = 5;
    static constexpr FwSizeType TC_SEGMENT_HEADER_SIZE = 1;
    static constexpr FwSizeType SPI_SIZE = 2;
    static constexpr FwSizeType IV_SIZE = 12;
    static constexpr FwSizeType SIZE_NO_SEGMENT_HEADER = TC_PRIMARY_HEADER_SIZE + SPI_SIZE + IV_SIZE;
    static constexpr FwSizeType SIZE_WITH_SEGMENT_HEADER = SIZE_NO_SEGMENT_HEADER + TC_SEGMENT_HEADER_SIZE;
    static constexpr FwSizeType MAX_SIZE = SIZE_WITH_SEGMENT_HEADER;

    //! Byte 2 holds the 6-bit VCID in bits 7..2, with the top of the frame length below it
    static constexpr U8 VCID_MASK = 0xFC;
    static constexpr FwSizeType VCID_BYTE_INDEX = 2;
    static constexpr U8 VCID_SHIFT = 2;

    U8 bytes[MAX_SIZE];
    FwSizeType size;  //!< Number of valid octets in bytes: SIZE_NO_SEGMENT_HEADER or SIZE_WITH_SEGMENT_HEADER

    //! Frame without a TC Segment Header (19 octets)
    SdlsTcAuthMask(U8 vcId, U16 securityAssociationIndex) : SdlsTcAuthMask(vcId, securityAssociationIndex, false, 0) {}

    //! When segmentHeaderPresent, the received Segment Header octet is placed at index 5 and the SPI moves to 6..7
    SdlsTcAuthMask(U8 vcId, U16 securityAssociationIndex, bool segmentHeaderPresent, U8 segmentHeader)
        : size(segmentHeaderPresent ? SIZE_WITH_SEGMENT_HEADER : SIZE_NO_SEGMENT_HEADER) {
        (void)::memset(this->bytes, 0, MAX_SIZE);
        this->bytes[VCID_BYTE_INDEX] = static_cast<U8>((vcId << VCID_SHIFT) & VCID_MASK);
        FwSizeType index = TC_PRIMARY_HEADER_SIZE;
        if (segmentHeaderPresent) {
            this->bytes[index] = segmentHeader;
            index += TC_SEGMENT_HEADER_SIZE;
        }
        this->bytes[index] = static_cast<U8>(securityAssociationIndex >> 8);
        this->bytes[index + 1] = static_cast<U8>(securityAssociationIndex & 0xFF);
    }
};

//! AAD for an SDLS-protected TM (downlink) transfer frame: 20 bytes, laid out as the masked
//! 6-byte primary header, the SPI verbatim, then a zeroed 12-byte IV field.
//!
//! Assumes no transfer frame secondary header; a link using one inserts that many additional
//! zero bytes before the SPI.
struct SdlsTmAuthMask final {
    static constexpr FwSizeType TM_PRIMARY_HEADER_SIZE = 6;
    static constexpr FwSizeType SPI_SIZE = 2;
    static constexpr FwSizeType IV_SIZE = 12;
    static constexpr FwSizeType SIZE = TM_PRIMARY_HEADER_SIZE + SPI_SIZE + IV_SIZE;

    //! Byte 1 holds the 3-bit VCID in bits 3..1, between the low spacecraft ID bits and the OCF flag
    static constexpr U8 VCID_MASK = 0x0E;
    static constexpr FwSizeType VCID_BYTE_INDEX = 1;
    static constexpr U8 VCID_SHIFT = 1;

    U8 bytes[SIZE];

    SdlsTmAuthMask(U8 vcId, U16 securityAssociationIndex) {
        (void)::memset(this->bytes, 0, SIZE);
        this->bytes[VCID_BYTE_INDEX] = static_cast<U8>((vcId << VCID_SHIFT) & VCID_MASK);
        this->bytes[TM_PRIMARY_HEADER_SIZE] = static_cast<U8>(securityAssociationIndex >> 8);
        this->bytes[TM_PRIMARY_HEADER_SIZE + 1] = static_cast<U8>(securityAssociationIndex & 0xFF);
    }
};

}  // namespace Utils
}  // namespace Ccsds
}  // namespace Svc

#endif
