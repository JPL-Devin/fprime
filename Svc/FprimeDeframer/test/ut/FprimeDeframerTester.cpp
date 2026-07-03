// ======================================================================
// \title  FprimeDeframerTester.cpp
// \author thomas-bc
// \brief  cpp file for FprimeDeframer component test harness implementation class
// ======================================================================

#include "FprimeDeframerTester.hpp"
#include "STest/Random/Random.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FprimeDeframerTester ::FprimeDeframerTester()
    : FprimeDeframerGTestBase("FprimeDeframerTester", FprimeDeframerTester::MAX_HISTORY_SIZE),
      component("FprimeDeframer") {
    this->initComponents();
    this->connectPorts();
}

FprimeDeframerTester ::~FprimeDeframerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FprimeDeframerTester ::testNominalFrame() {
    // Nominal frame with 1 byte of payload. The header carries the APID; the
    // lengthField accounts for the apid field and the payload (= 2 + 1 = 3)

    // Get random byte of data
    U8 randomByte = static_cast<U8>(STest::Random::lowerUpper(1, 255));
    //           |  F´ start word        |     Length (= 3)      | APID (FILE)|   Data     |   Checksum (4 bytes)   |
    U8 data[15] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, randomByte, 0x00, 0x00, 0x00, 0x00};
    // Inject the checksum into the data and send it to the component under test
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(1);        // something emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(0);  // nothing emitted on dataReturnOut
    // Payload is descriptor-free: exactly the 1 data byte
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getSize(), 1);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getData()[0], randomByte);
    // APID comes from the frame header
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_apid(), ComCfg::Apid::FW_PACKET_FILE);
    ASSERT_EVENTS_SIZE(0);  // no events emitted
}

void FprimeDeframerTester ::testNominalFrameApid() {
    // Nominal frame with no payload: lengthField only accounts for the apid field (= 2).
    // The APID is a random value below INVALID_UNINITIALIZED: the deframer accepts any
    // numerically in-range value (not just defined enum constants) and reports it in the context
    U8 randomByte = static_cast<U8>(STest::Random::lowerUpper(0, 255));
    //           |  F´ start word        |     Length (= 2)      | APID            | Checksum (4 bytes)    |
    U8 data[14] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x02, 0x00, randomByte, 0x00, 0x00, 0x00, 0x00};
    // Inject the checksum into the data and send it to the component under test
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(1);                                                     // something emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(0);                                               // nothing emitted on dataReturnOut
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getSize(), 0);               // payload is empty
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_apid(), randomByte);  // APID should be set in context
    ASSERT_EVENTS_SIZE(0);                                                           // no events emitted
}

void FprimeDeframerTester ::testOutOfRangeApid() {
    // A frame whose APID is not a valid ComCfg::Apid enum value (>= INVALID_UNINITIALIZED)
    // should still be deframed, with the context APID defaulting to FW_PACKET_UNKNOWN
    //           |  F´ start word        |     Length (= 2)      | APID (0x0900)   | Checksum (4 bytes)    |
    U8 data[14] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x02, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00};
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(1);        // frame is still emitted
    ASSERT_from_dataReturnOut_SIZE(0);  // nothing emitted on dataReturnOut
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_apid(), ComCfg::Apid::FW_PACKET_UNKNOWN);
    ASSERT_EVENTS_SIZE(0);  // no events emitted
}

void FprimeDeframerTester ::testLengthTooSmall() {
    // lengthField must at least account for the apid field (2 bytes); a length of 0 is invalid
    //           |  F´ start word        |     Length (= 0)      | APID       | Checksum (4 bytes)    |
    U8 data[14] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(0);                  // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);            // invalid buffer was deallocated
    ASSERT_EVENTS_SIZE(1);                        // exactly 1 event emitted
    ASSERT_EVENTS_InvalidLengthReceived_SIZE(1);  // event was emitted for invalid length
}

void FprimeDeframerTester ::testIncorrectLengthToken() {
    // Frame:     |  F´ start word       |  INCORRECT Length=5   | APID       |   Checksum (4 bytes)   |
    U8 data[14] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    // Inject the checksum into the data and send it to the component under test
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(0);        // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    // Check which event was emitted
    ASSERT_EVENTS_SIZE(1);                        // exactly 1 event emitted
    ASSERT_EVENTS_InvalidLengthReceived_SIZE(1);  // event was emitted for invalid length
}

void FprimeDeframerTester ::testIncorrectStartWord() {
    // Frame:     |  INCORRECT start word |      Length = 2      | APID       |   Checksum (4 bytes)   |
    U8 data[14] = {0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    // Inject the checksum into the data and send it to the component under test
    this->injectChecksum(data, sizeof(data));
    this->mockReceiveData(data, sizeof(data));

    ASSERT_from_dataOut_SIZE(0);        // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    // Check which event was emitted
    ASSERT_EVENTS_SIZE(1);                   // exactly 1 event emitted
    ASSERT_EVENTS_InvalidStartWord_SIZE(1);  // event was emitted for invalid start word
}

void FprimeDeframerTester ::testIncorrectCrc() {
    // Frame:     |   F´ start word      |      Length = 2       | APID       | INCORRECT Checksum    |
    U8 data[14] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    this->mockReceiveData(data, sizeof(data));
    ASSERT_from_dataOut_SIZE(0);        // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    // Check which event was emitted
    ASSERT_EVENTS_SIZE(1);                  // exactly 1 event emitted
    ASSERT_EVENTS_InvalidChecksum_SIZE(1);  // event was emitted for invalid checksum
}

void FprimeDeframerTester::testTruncatedFrame() {
    // Send a truncated frame, too short to be valid (minimum is header (10) + trailer (4))
    U8 data[13] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    this->mockReceiveData(data, sizeof(data));
    ASSERT_from_dataOut_SIZE(0);        // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    // Check which event was emitted
    ASSERT_EVENTS_SIZE(1);                        // exactly 1 event emitted
    ASSERT_EVENTS_InvalidBufferReceived_SIZE(1);  // event was emitted for invalid buffer
}

void FprimeDeframerTester::testZeroSizeFrame() {
    // Send an empty frame, too short to be valid
    this->mockReceiveData(nullptr, 0);
    ASSERT_from_dataOut_SIZE(0);        // nothing emitted on dataOut
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    // Check which event was emitted
    ASSERT_EVENTS_SIZE(1);                        // exactly 1 event emitted
    ASSERT_EVENTS_InvalidBufferReceived_SIZE(1);  // event was emitted for invalid buffer
}

void FprimeDeframerTester::testDataReturn() {
    U8 data[1];
    Fw::Buffer buffer(data, sizeof(data));
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataReturnIn(0, buffer, nullContext);
    ASSERT_from_dataReturnOut_SIZE(1);  // incoming buffer should be deallocated
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), data);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), sizeof(data));
}

// ----------------------------------------------------------------------
// Test Helpers
// ----------------------------------------------------------------------

void FprimeDeframerTester::injectChecksum(U8* data, FwSizeType size) {
    // Needs 4 bytes for the checksum field and at least 1 byte of data to checksum
    if (size < 5) {
        return;
    }
    // Compute the checksum
    Utils::Hash crc_calculator;
    Utils::HashBuffer crc_result;
    crc_calculator.update(data, size - 4);
    crc_calculator.finalize(crc_result);
    // Inject the checksum into the data
    for (FwSizeType i = 0; i < 4; i++) {
        data[size - 4 + i] = static_cast<U8>(crc_result.asBigEndianU32() >> (8 * (3 - i)) & 0xFF);
    }
}

void FprimeDeframerTester::mockReceiveData(U8* data, FwSizeType size) {
    ComCfg::FrameContext nullContext;
    Fw::Buffer buffer(data, static_cast<Fw::Buffer::SizeType>(size));
    this->invoke_to_dataIn(0, buffer, nullContext);
}

}  // namespace Svc
