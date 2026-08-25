// ======================================================================
// \title  CcsdsUslpFrameDetectorTestMain.cpp
// \author thomas-bc
// \brief  cpp file for CcsdsUslpFrameDetector test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/Types/USLPHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTfdfHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "Svc/FrameAccumulator/FrameDetector/CcsdsUslpFrameDetector.hpp"
#include "Utils/Types/test/ut/CircularBuffer/CircularBufferTester.hpp"
#include "gtest/gtest.h"

using namespace Svc::Ccsds;

constexpr U32 CIRCULAR_BUFFER_TEST_SIZE = 2048;
constexpr FwSizeType USLP_FRAME_OVERHEAD =
    USLPHeader::SERIALIZED_SIZE + USLPTfdfHeader::SERIALIZED_SIZE + USLPTrailer::SERIALIZED_SIZE;

// The first header word: TFVN 0xC, configured SCID, destination flag set, VCID 0, MAP 0, EOFPH 0
constexpr U32 EXPECTED_ID_WORD =
    (static_cast<U32>(USLPHeaderSubfields::frameVersionValue) << USLPHeaderSubfields::frameVersionOffset) |
    (static_cast<U32>(ComCfg::SpacecraftId) << USLPHeaderSubfields::spacecraftIdOffset) |
    (0x1 << USLPHeaderSubfields::sourceOrDestOffset);

// Test fixture to set up the detector under test and circular buffer
class CcsdsUslpFrameDetectorTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ::memset(this->m_buffer, 0, CIRCULAR_BUFFER_TEST_SIZE);
        this->circular_buffer = Types::CircularBuffer(this->m_buffer, CIRCULAR_BUFFER_TEST_SIZE);
    }

    U8 m_buffer[CIRCULAR_BUFFER_TEST_SIZE];
    Svc::FrameDetectors::CcsdsUslpFrameDetector detector;
    Types::CircularBuffer circular_buffer;
};

//! \brief Create a USLP frame and serialize it into the supplied circular buffer
//! \param circular_buffer The circular buffer to serialize the frame into
//! \param id_word Value of the first four header octets
//! \param length_override If nonzero, override the frame length field with this value
//! \return The size of the generated frame
FwSizeType generate_uslp_frame(Types::CircularBuffer& circular_buffer,
                               U32 id_word = EXPECTED_ID_WORD,
                               U16 length_override = 0) {
    U16 payload_size = static_cast<U16>(STest::Random::lowerUpper(1, 1024));
    FwSizeType total_frame_size = payload_size + USLP_FRAME_OVERHEAD;

    U16 length_field = static_cast<U16>(total_frame_size - 1);
    if (length_override != 0) {
        length_field = length_override;
    }
    USLPHeader uslpHeader(id_word, length_field,
                          0  // flags: no protocol command, no OCF, VCF count length 0
    );
    U8 frame_header[USLPHeader::SERIALIZED_SIZE];
    Fw::ExternalSerializeBuffer header_ser_buffer(frame_header, USLPHeader::SERIALIZED_SIZE);
    uslpHeader.serializeTo(header_ser_buffer);
    circular_buffer.serialize(frame_header, USLPHeader::SERIALIZED_SIZE);

    // TFDF header: construction rule 0b111 (no segmentation), UPID 0
    U8 tfdf_header[USLPTfdfHeader::SERIALIZED_SIZE] = {0xE0};
    circular_buffer.serialize(tfdf_header, USLPTfdfHeader::SERIALIZED_SIZE);

    U8 payload_data[payload_size];
    for (FwSizeType i = 0; i < payload_size; i++) {
        payload_data[i] = static_cast<U8>(STest::Random::lowerUpper(0, 255));
    }
    circular_buffer.serialize(payload_data, payload_size);

    // Calculate CRC over everything before the trailer
    Svc::Ccsds::Utils::CRC16 crc;
    for (FwSizeType i = 0; i < total_frame_size - USLPTrailer::SERIALIZED_SIZE; ++i) {
        U8 byte = 0;
        circular_buffer.peek(byte, i);
        crc.update(byte);
    }
    USLPTrailer uslpTrailer;
    uslpTrailer.set_fecf(crc.finalize());
    U8 frame_trailer[USLPTrailer::SERIALIZED_SIZE];
    Fw::ExternalSerializeBuffer trailer_ser_buffer(frame_trailer, USLPTrailer::SERIALIZED_SIZE);
    uslpTrailer.serializeTo(trailer_ser_buffer);
    circular_buffer.serialize(frame_trailer, USLPTrailer::SERIALIZED_SIZE);
    return total_frame_size;
}

TEST_F(CcsdsUslpFrameDetectorTest, TestBufferTooSmall) {
    // Anything smaller than the primary header cannot be evaluated
    U32 invalid_size = STest::Random::lowerUpper(1, USLPHeader::SERIALIZED_SIZE - 1);
    this->circular_buffer.serialize(this->m_buffer, invalid_size);

    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(circular_buffer, size_out);

    // Expect that the detector reports that more data is needed
    EXPECT_EQ(status, Svc::FrameDetector::Status::MORE_DATA_NEEDED);
    EXPECT_EQ(size_out, USLP_FRAME_OVERHEAD);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestFrameDetected) {
    FwSizeType frame_size = generate_uslp_frame(this->circular_buffer);

    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(circular_buffer, size_out);

    EXPECT_EQ(status, Svc::FrameDetector::Status::FRAME_DETECTED);
    EXPECT_EQ(size_out, frame_size);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestFrameDetectedAnyVcidAndMap) {
    // VCID and MAP ID are not part of the sync token: frames on any VC/MAP must be detected
    const U32 id_word = EXPECTED_ID_WORD | (0x2A << USLPHeaderSubfields::virtualChannelIdOffset) |
                        (0x5 << USLPHeaderSubfields::mapIdOffset);
    FwSizeType frame_size = generate_uslp_frame(this->circular_buffer, id_word);

    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(circular_buffer, size_out);

    EXPECT_EQ(status, Svc::FrameDetector::Status::FRAME_DETECTED);
    EXPECT_EQ(size_out, frame_size);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestGarbagePrefixResync) {
    (void)generate_uslp_frame(this->circular_buffer);
    // Remove 1 byte from the beginning of the frame, making it invalid at the current offset
    this->circular_buffer.rotate(1);
    Svc::FrameDetector::Status status;
    FwSizeType unused = 0;
    status = this->detector.detect(this->circular_buffer, unused);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestWrongScidRejected) {
    const U32 id_word = EXPECTED_ID_WORD ^ (0x1 << USLPHeaderSubfields::spacecraftIdOffset);  // flip an SCID bit
    (void)generate_uslp_frame(this->circular_buffer, id_word);
    Svc::FrameDetector::Status status;
    FwSizeType unused = 0;
    status = this->detector.detect(this->circular_buffer, unused);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestEofphRejected) {
    const U32 id_word = EXPECTED_ID_WORD | 0x1;  // set the EOFPH flag (truncated frame)
    (void)generate_uslp_frame(this->circular_buffer, id_word);
    Svc::FrameDetector::Status status;
    FwSizeType unused = 0;
    status = this->detector.detect(this->circular_buffer, unused);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestMoreDataNeeded) {
    FwSizeType frame_size = generate_uslp_frame(this->circular_buffer);
    // Remove 1 byte from the end of the frame to trigger "more data needed"
    Types::CircularBufferTester::tester_m_allocated_size_decrement(this->circular_buffer);
    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(this->circular_buffer, size_out);
    EXPECT_EQ(status, Svc::FrameDetector::Status::MORE_DATA_NEEDED);
    EXPECT_EQ(size_out, frame_size);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestCorruptedCrc) {
    FwSizeType frame_size = generate_uslp_frame(this->circular_buffer);
    this->m_buffer[frame_size - 2] ^= 0xFF;  // Corrupt the last 2 bytes to fail CRC check
    this->m_buffer[frame_size - 1] ^= 0xFF;  // Corrupt the last 2 bytes to fail CRC check

    Svc::FrameDetector::Status status;
    FwSizeType unused = 0;
    status = this->detector.detect(this->circular_buffer, unused);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestRejectsTooSmallExpectedFrameLength) {
    // length field = USLP_FRAME_OVERHEAD - 2 -> expected_frame_length = USLP_FRAME_OVERHEAD - 1.
    // Boundary just below the minimum valid size - must be rejected by the guard before the CRC span
    (void)generate_uslp_frame(this->circular_buffer, EXPECTED_ID_WORD, static_cast<U16>(USLP_FRAME_OVERHEAD - 2));
    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(this->circular_buffer, size_out);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

TEST_F(CcsdsUslpFrameDetectorTest, TestRejectsLengthAboveCapacity) {
    // length field = 0xFFFF -> expected_frame_length = 0x10000 (must widen, not wrap to 0),
    // which is larger than the circular buffer capacity and can never be accumulated
    (void)generate_uslp_frame(this->circular_buffer, EXPECTED_ID_WORD, 0xFFFF);
    Svc::FrameDetector::Status status;
    FwSizeType size_out = 0;
    status = this->detector.detect(this->circular_buffer, size_out);
    EXPECT_EQ(status, Svc::FrameDetector::Status::NO_FRAME_DETECTED);
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
