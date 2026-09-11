// ======================================================================
// \title  TcSegmentHeaderTest.cpp
// \brief  Unit tests for the TC Segment Header types, decoder, and FrameContext fields
// ======================================================================

#include <gtest/gtest.h>
#include "Fw/Types/SerialBuffer.hpp"
#include "Svc/Ccsds/Types/FrameErrorEnumAc.hpp"
#include "Svc/Ccsds/Types/TcSequenceFlagsEnumAc.hpp"
#include "Svc/Ccsds/Utils/TcSegmentHeader.hpp"
#include "config/FrameContextSerializableAc.hpp"

namespace Svc {
namespace Ccsds {
namespace Utils {

// Serialized size of ComCfg::FrameContext before the two TC Segment Header fields were appended
static constexpr FwSizeType FRAME_CONTEXT_BASELINE_SERIALIZED_SIZE =
    sizeof(FwIndexType)              // comQueueIndex
    + ComCfg::Apid::SERIALIZED_SIZE  // apid
    + sizeof(U8)                     // hasSecHdr (bool serializes as one octet)
    + sizeof(U8)                     // sequenceFlags
    + sizeof(U16)                    // sequenceCount
    + sizeof(U8)                     // vcId
    + ComCfg::Pvn::SERIALIZED_SIZE   // pvn
    + sizeof(U8)                     // sendNow
    + sizeof(U16)                    // saIndex
    + sizeof(U16);                   // firstHeaderPointer

TEST(TcSegmentHeader, testSerializedSize) {
    ASSERT_EQ(ComCfg::FrameContext::SERIALIZED_SIZE, FRAME_CONTEXT_BASELINE_SERIALIZED_SIZE + 2);

    const ComCfg::FrameContext defaults;
    ASSERT_FALSE(defaults.get_tcSegmentHeaderPresent());
    ASSERT_EQ(defaults.get_tcSegmentHeader(), 0);

    U8 storage[ComCfg::FrameContext::SERIALIZED_SIZE];
    Fw::SerialBuffer serialBuffer(storage, sizeof(storage));
    ASSERT_EQ(serialBuffer.serializeFrom(defaults), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(serialBuffer.getSize(), ComCfg::FrameContext::SERIALIZED_SIZE);

    ComCfg::FrameContext roundTrip;
    roundTrip.set_tcSegmentHeaderPresent(true);
    roundTrip.set_tcSegmentHeader(0xFF);
    ASSERT_EQ(serialBuffer.deserializeTo(roundTrip), Fw::FW_SERIALIZE_OK);
    ASSERT_FALSE(roundTrip.get_tcSegmentHeaderPresent());
    ASSERT_EQ(roundTrip.get_tcSegmentHeader(), 0);
    ASSERT_EQ(roundTrip, defaults);
}

TEST(TcSegmentHeader, testFrameErrorValuesUnchanged) {
    ASSERT_EQ(static_cast<U8>(FrameError::SP_INVALID_PACKET), 0);
    ASSERT_EQ(static_cast<U8>(FrameError::SP_INVALID_LENGTH), 1);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_INVALID_SCID), 2);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_INVALID_LENGTH), 3);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_INVALID_VCID), 4);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_INVALID_CRC), 5);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_SCID), 6);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_LENGTH), 7);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_VCID), 8);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_CRC), 9);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_VERSION), 10);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_INVALID_EPP), 11);
    ASSERT_EQ(static_cast<U8>(FrameError::AOS_VC_FRAME_COUNT_GAP), 12);
    ASSERT_EQ(static_cast<U8>(FrameError::SDLS_DECRYPTION_FAILURE), 13);

    ASSERT_EQ(static_cast<U8>(FrameError::TC_MISSING_SEGMENT_HEADER), 14);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_HEADER_ABSENT), 15);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_INVALID_MAP_ID), 16);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_EMPTY), 17);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_UNEXPECTED_FIRST), 18);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_UNEXPECTED_UNSEGMENTED), 19);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_ORPHAN), 20);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_OVERFLOW), 21);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_ALLOC_FAILED), 22);
    ASSERT_EQ(static_cast<U8>(FrameError::TC_SEGMENT_LENGTH_MISMATCH), 23);
    ASSERT_EQ(FrameError::NUM_CONSTANTS, 24);
}

TEST(TcSegmentHeader, testSequenceFlagsValues) {
    ASSERT_EQ(static_cast<U8>(TcSequenceFlags::CONTINUING), 0);
    ASSERT_EQ(static_cast<U8>(TcSequenceFlags::FIRST), 1);
    ASSERT_EQ(static_cast<U8>(TcSequenceFlags::LAST), 2);
    ASSERT_EQ(static_cast<U8>(TcSequenceFlags::UNSEGMENTED), 3);
    ASSERT_EQ(TcSequenceFlags::NUM_CONSTANTS, 4);
}

TEST(TcSegmentHeader, testDecode) {
    // Vectors from the design: FIRST MAP 1 = 0x41, UNSEGMENTED MAP 1 = 0xC1
    ASSERT_EQ(TcSegmentHeader::sequenceFlags(0x41), TcSequenceFlags::FIRST);
    ASSERT_EQ(TcSegmentHeader::mapId(0x41), 1);
    ASSERT_EQ(TcSegmentHeader::sequenceFlags(0xC1), TcSequenceFlags::UNSEGMENTED);
    ASSERT_EQ(TcSegmentHeader::mapId(0xC1), 1);
    ASSERT_EQ(TcSegmentHeader::sequenceFlags(0x00), TcSequenceFlags::CONTINUING);
    ASSERT_EQ(TcSegmentHeader::mapId(0x00), 0);
    ASSERT_EQ(TcSegmentHeader::sequenceFlags(0xBF), TcSequenceFlags::LAST);
    ASSERT_EQ(TcSegmentHeader::mapId(0xBF), static_cast<U8>(TcSegmentHeader::MAP_ID_MAX));

    for (U16 octet = 0; octet <= 0xFF; octet++) {
        const U8 sh = static_cast<U8>(octet);
        const TcSequenceFlags::T flags = TcSegmentHeader::sequenceFlags(sh);
        const U8 mapId = TcSegmentHeader::mapId(sh);
        ASSERT_LE(static_cast<U8>(flags), static_cast<U8>(TcSequenceFlags::UNSEGMENTED));
        ASSERT_TRUE(TcSegmentHeader::isValidMapId(mapId));
        ASSERT_EQ(TcSegmentHeader::encode(flags, mapId), sh);
    }
    ASSERT_TRUE(TcSegmentHeader::isValidMapId(TcSegmentHeader::MAP_ID_MAX));
    ASSERT_FALSE(TcSegmentHeader::isValidMapId(static_cast<U8>(TcSegmentHeader::MAP_ID_MAX + 1)));
}

}  // namespace Utils
}  // namespace Ccsds
}  // namespace Svc

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
