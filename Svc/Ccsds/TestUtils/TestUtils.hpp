// ======================================================================
// \title  CcsdsTestUtils.hpp
// \author bocchino
// \brief  Test utilities for CCSDS
// ======================================================================

#ifndef Svc_Ccsds_TestUtils_TestUtils_HPP
#define Svc_Ccsds_TestUtils_TestUtils_HPP

#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/Optional.hpp"
#include "config/ApidEnumAc.hpp"

namespace Svc {

namespace CcsdsTestUtils {

//! Get a random APID for CCSDS testing
//! If possible, randomly choose from among the set of configured constants for
//! ComCfg::APID that fit in 11 bits.
//! If not possible, return NONE.
//! \return SOME(apid) or NONE
Fw::Optional<ComCfg::Apid::T> getRandomApid();

//! Primary header fields and optional Segment Header of a CCSDS TC Transfer Frame (CCSDS 232.0-B-4 4.1.2)
struct TcFrameFields {
    bool bypass = true;              //!< Bypass Flag (1 = Type-B, FARM checks bypassed)
    bool control = false;            //!< Control Command Flag (1 = Type-C control frame)
    U16 scid = 0;                    //!< 10-bit Spacecraft ID
    U8 vcid = 0;                     //!< 6-bit Virtual Channel ID
    U8 sequence = 0;                 //!< Frame Sequence Number
    bool withSegmentHeader = false;  //!< Insert a Segment Header octet after the primary header
    U8 segmentHeader = 0;            //!< Raw Segment Header octet (Sequence Flags << 6 | MAP ID)
};

//! Build a CCSDS TC Transfer Frame: primary header | [Segment Header] | payload | FECF (CRC-16)
//! The Frame Length field is set to (total octets - 1) and the FECF is computed with Svc::Ccsds::Utils::CRC16
//! over every preceding octet, exactly as Svc::Ccsds::TcDeframer verifies it.
//! \param fields header fields and Segment Header selection
//! \param payload data octets placed after the primary header (and Segment Header, when present)
//! \param frame on input, backing storage whose size is the capacity; on output, its size is set to the frame length
//! \return true when the frame fit in `frame`, false (frame untouched) when the capacity is too small or the frame
//! would exceed the 1024-octet CCSDS maximum
bool buildTcFrame(const TcFrameFields& fields, const Fw::Buffer& payload, Fw::Buffer& frame);

//! Build a CCSDS TC Transfer Frame from individual header fields (see the TcFrameFields overload)
//! \param bypass Bypass Flag
//! \param control Control Command Flag
//! \param scid 10-bit Spacecraft ID
//! \param vcid 6-bit Virtual Channel ID
//! \param seq Frame Sequence Number
//! \param payload data octets placed after the primary header (and Segment Header, when present)
//! \param withSegmentHeader insert a Segment Header octet after the primary header
//! \param sh raw Segment Header octet (ignored when withSegmentHeader is false)
//! \param frame backing storage on input; on output, its size is set to the frame length
//! \return true when the frame fit in `frame`, false otherwise (frame untouched)
bool buildTcFrame(bool bypass,
                  bool control,
                  U16 scid,
                  U8 vcid,
                  U8 seq,
                  const Fw::Buffer& payload,
                  bool withSegmentHeader,
                  U8 sh,
                  Fw::Buffer& frame);

//! Build a Segment Header octet (CCSDS 232.0-B-4 4.1.3.2.2) from its Sequence Flags and MAP ID
//! \param sequenceFlags 2-bit Sequence Flags (0 continuing, 1 first, 2 last, 3 unsegmented)
//! \param mapId 6-bit MAP ID
//! \return the raw octet
U8 makeSegmentHeader(U8 sequenceFlags, U8 mapId);

}  // namespace CcsdsTestUtils

}  // namespace Svc

#endif
