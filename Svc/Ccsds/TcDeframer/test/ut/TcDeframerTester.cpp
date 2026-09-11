// ======================================================================
// \title  TcDeframerTester.cpp
// \author thomas-bc
// \brief  cpp file for TcDeframer component test harness implementation class
// ======================================================================

#include "TcDeframerTester.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/TCHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TCTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Types/TcSequenceFlagsEnumAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

TcDeframerTester ::TcDeframerTester()
    : TcDeframerGTestBase("TcDeframerTester", TcDeframerTester::MAX_HISTORY_SIZE), component("TcDeframer") {
    this->initComponents();
    this->connectPorts();
}

TcDeframerTester ::~TcDeframerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void TcDeframerTester::testDataReturn() {
    U8 data[1] = {0};
    Fw::Buffer buffer(data, sizeof(data));
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataReturnIn(0, buffer, nullContext);
    ASSERT_from_dataReturnOut_SIZE(1);  // incoming buffer should be deallocated
    ASSERT_FROM_PORT_HISTORY_SIZE(1);   // only port call
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), data);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), sizeof(data));
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context, nullContext);
}

void TcDeframerTester::testNominalDeframing() {
    // Frame: 5 bytes (header) + bytes (data) + 2 bytes (trailer)
    U16 scId = static_cast<U16>(STest::Random::lowerUpper(0, 0x3FF));       // random 10 bit Spacecraft ID
    U8 vcId = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));          // random 6 bit virtual channel ID
    U8 seqCount = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));      // random 8 bit sequence count
    U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));  // bytes of data, random length
    U8 payload[payloadLength];
    // Initialize payload with some data
    for (FwIndexType i = 0; i < payloadLength; i++) {
        payload[i] = static_cast<U8>(i % std::numeric_limits<U8>::max());
    }

    Fw::Buffer buffer = this->assembleFrameBuffer(payload, payloadLength, scId, vcId, seqCount);
    ComCfg::FrameContext nullContext;

    this->setComponentState(scId, vcId, seqCount);
    this->invoke_to_dataIn(0, buffer, nullContext);

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(1);  // only one port call
    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), payloadLength);
    for (FwIndexType i = 0; i < payloadLength; i++) {
        ASSERT_EQ(outBuffer.getData()[i], payload[i]);
    }
    // The frame's VCID is carried on the emitted context: Svc.Ccsds.AesGcmDecryptor builds its
    // AAD from this field, so a frame on VC != 0 fails its MAC check if it is not set here
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_vcId(), vcId);
}

void TcDeframerTester::testInvalidScId() {
    // Frame: 5 bytes (header) + 1 byte (data) + 2 bytes (trailer)
    U16 scId = static_cast<U16>(STest::Random::lowerUpper(1, 0x3FF));    // random 10 bit Spacecraft ID
    U8 dataLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));  // bytes of data, random length
    U8 data[dataLength];

    // Assemble frame with incorrect scId value
    Fw::Buffer buffer = this->assembleFrameBuffer(data, dataLength, static_cast<U16>(scId - 1));
    ComCfg::FrameContext nullContext;

    this->setComponentState(scId);
    this->invoke_to_dataIn(0, buffer, nullContext);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    ASSERT_FROM_PORT_HISTORY_SIZE(2);   // two port calls, one for dataReturn, one for errorNotify
    ASSERT_from_errorNotify(0, Svc::Ccsds::FrameError::TC_INVALID_SCID);  // errorNotify port called with invalid SCID
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), buffer.getSize());
    ASSERT_EVENTS_SIZE(1);                      // exactly 1 event emitted
    ASSERT_EVENTS_InvalidSpacecraftId_SIZE(1);  // event was emitted for invalid spacecraft ID
    ASSERT_EVENTS_InvalidSpacecraftId(0, static_cast<U16>(scId - 1),
                                      scId);  // event was emitted for invalid spacecraft ID
}

void TcDeframerTester::testInvalidVcId() {
    U8 vcId = static_cast<U8>(STest::Random::lowerUpper(1, 0x3F));       // random 6 bit VCID
    U8 dataLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));  // bytes of data, random length
    U8 data[dataLength];

    // Assemble frame with incorrect vcId value
    Fw::Buffer buffer = this->assembleFrameBuffer(data, dataLength, 0, static_cast<U8>(vcId - 1));
    ComCfg::FrameContext nullContext;

    this->setComponentState(0, vcId, 0, false);  // set the component in mode where only one VCID is accepted
    this->invoke_to_dataIn(0, buffer, nullContext);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    ASSERT_FROM_PORT_HISTORY_SIZE(2);   // two port calls, one for dataReturn, one for errorNotify
    ASSERT_from_errorNotify(0, Svc::Ccsds::FrameError::TC_INVALID_VCID);  // errorNotify port called with invalid VCID
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), buffer.getSize());
    ASSERT_EVENTS_SIZE(1);                                           // exactly 1 event emitted
    ASSERT_EVENTS_InvalidVcId_SIZE(1);                               // event was emitted for invalid VCID
    ASSERT_EVENTS_InvalidVcId(0, static_cast<U16>(vcId - 1), vcId);  // event was emitted for invalid VCID
}

void TcDeframerTester::testInvalidLengthToken() {
    U8 dataLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));  // bytes of data, random length
    U8 data[dataLength];
    U8 incorrectLengthToken = static_cast<U8>(dataLength + TCHeader::SERIALIZED_SIZE + TCTrailer::SERIALIZED_SIZE + 1);

    Fw::Buffer buffer = this->assembleFrameBuffer(data, dataLength);
    buffer.getData()[3] = incorrectLengthToken;  // Override length token to invalid value
    ComCfg::FrameContext nullContext;

    this->setComponentState();
    this->invoke_to_dataIn(0, buffer, nullContext);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was deallocated
    ASSERT_FROM_PORT_HISTORY_SIZE(2);   // two port calls, one for dataReturn, one for errorNotify
    ASSERT_from_errorNotify(0, Svc::Ccsds::FrameError::TC_INVALID_LENGTH);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), buffer.getSize());
    ASSERT_EVENTS_SIZE(1);                     // exactly 1 event emitted
    ASSERT_EVENTS_InvalidFrameLength_SIZE(1);  // event was emitted for invalid frame length
    // event logs size in bytes which is length token + 1
    ASSERT_EVENTS_InvalidFrameLength(
        0, static_cast<U16>(incorrectLengthToken + 1),
        static_cast<FwSizeType>(dataLength + TCHeader::SERIALIZED_SIZE + TCTrailer::SERIALIZED_SIZE));
}

void TcDeframerTester::testInvalidCrc() {
    U8 dataLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));  // bytes of data, random length
    U8 data[dataLength];

    Fw::Buffer buffer = this->assembleFrameBuffer(data, dataLength);
    // Increment CRC to corrupt its value
    buffer.getData()[TCHeader::SERIALIZED_SIZE + dataLength + 1]++;
    ComCfg::FrameContext nullContext;

    this->setComponentState();
    this->invoke_to_dataIn(0, buffer, nullContext);

    // Invalid CRC drops the frame
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(2);  // two port calls, one for dataReturn, one for errorNotify
    ASSERT_from_errorNotify(0, Svc::Ccsds::FrameError::TC_INVALID_CRC);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), buffer.getSize());
    ASSERT_EVENTS_SIZE(1);  // exactly 1 event emitted
    ASSERT_EVENTS_InvalidCrc_SIZE(1);
}

// ----------------------------------------------------------------------
// Segment Header mode tests (CCSDS 232.0-B-4 4.1.3.2.2)
// ----------------------------------------------------------------------

void TcDeframerTester::testFeatureOffIdentity() {
    // Default-constructed component: configureSegmentHeader() is never called. Every frame of the corpus must come
    // out exactly as the baseline deframer emits it: payload = octets [5, length - 2), context untouched but for vcId
    const U16 scId = static_cast<U16>(STest::Random::lowerUpper(0, 0x3FF));
    this->setComponentState(scId);

    // (a) baseline frames from the existing helper (Bypass=0, random payload)
    for (U32 iteration = 0; iteration < 10; iteration++) {
        const U8 vcId = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));
        const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, MAX_PAYLOAD_LENGTH));
        TcDeframerTester::fillPayload(this->m_payload, payloadLength);
        Fw::Buffer frame = this->assembleFrameBuffer(this->m_payload, payloadLength, scId, vcId);
        this->assertForwarded(frame, this->m_payload, payloadLength, vcId, false, 0);
    }

    // (b) Type-BD frames from the shared builder whose first data octet looks like a Segment Header: it is payload
    for (U32 iteration = 0; iteration < 10; iteration++) {
        CcsdsTestUtils::TcFrameFields fields;
        fields.scid = scId;
        fields.vcid = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));
        fields.sequence = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
        const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, MAX_PAYLOAD_LENGTH));
        TcDeframerTester::fillPayload(this->m_payload, payloadLength);
        this->m_payload[0] = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
        Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));
        this->assertForwarded(frame, this->m_payload, payloadLength, fields.vcid, false, 0);
    }

    // (c) 7-octet frame (header | FECF) inside a 16-octet buffer: baseline forwards an empty payload
    CcsdsTestUtils::TcFrameFields emptyFields;
    emptyFields.scid = scId;
    emptyFields.vcid = 1;
    Fw::Buffer emptyFrame = this->buildFrame(emptyFields, Fw::Buffer(this->m_payload, 0), 16);
    this->assertForwarded(emptyFrame, this->m_payload, 0, 1, false, 0);

    // (d) Type-BC control frame (Unlock): baseline forwards its single data octet
    CcsdsTestUtils::TcFrameFields controlFields;
    controlFields.control = true;
    controlFields.scid = scId;
    controlFields.vcid = 1;
    this->m_payload[0] = 0x00;
    Fw::Buffer controlFrame = this->buildFrame(controlFields, Fw::Buffer(this->m_payload, 1));
    this->assertForwarded(controlFrame, this->m_payload, 1, 1, false, 0);
}

void TcDeframerTester::testShModeUnsegmented() {
    CcsdsTestUtils::TcFrameFields fields;
    fields.scid = static_cast<U16>(STest::Random::lowerUpper(0, 0x3FF));
    fields.vcid = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));
    fields.sequence = static_cast<U8>(STest::Random::lowerUpper(0, 0xFF));
    fields.withSegmentHeader = true;
    fields.segmentHeader = 0xC0;  // UNSEGMENTED, MAP 0
    const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, MAX_PAYLOAD_LENGTH));
    TcDeframerTester::fillPayload(this->m_payload, payloadLength);
    Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));

    this->setComponentState(fields.scid);
    this->component.configureSegmentHeader(true);
    this->assertForwarded(frame, this->m_payload, payloadLength, fields.vcid, true, 0xC0);
}

void TcDeframerTester::testShModeAllFlags() {
    const U8 mapIds[] = {0, 5, 63};
    this->setComponentState();
    this->component.configureSegmentHeader(true);
    for (U8 flags = 0; flags <= 3; flags++) {
        for (FwSizeType m = 0; m < FW_NUM_ARRAY_ELEMENTS(mapIds); m++) {
            CcsdsTestUtils::TcFrameFields fields;
            fields.vcid = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));
            fields.withSegmentHeader = true;
            fields.segmentHeader = CcsdsTestUtils::makeSegmentHeader(flags, mapIds[m]);
            ASSERT_EQ(fields.segmentHeader,
                      static_cast<U8>((flags << TCSegmentHeader::SequenceFlagsOffset) | mapIds[m]));
            const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, MAX_PAYLOAD_LENGTH));
            TcDeframerTester::fillPayload(this->m_payload, payloadLength);
            Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));
            // The raw octet is carried verbatim: the deframer neither decodes nor validates it
            this->assertForwarded(frame, this->m_payload, payloadLength, fields.vcid, true, fields.segmentHeader);
        }
    }
}

void TcDeframerTester::testShModeMinLength() {
    // header (5) | Segment Header (1) | FECF (2): the smallest frame Segment Header mode accepts. The empty segment
    // is forwarded as a zero-length payload; rejecting it is the reassembler's job
    CcsdsTestUtils::TcFrameFields fields;
    fields.scid = 0x44;
    fields.vcid = 1;
    fields.withSegmentHeader = true;
    fields.segmentHeader = CcsdsTestUtils::makeSegmentHeader(TcSequenceFlags::FIRST, 0);
    Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, 0));
    ASSERT_EQ(frame.getSize(),
              static_cast<FwSizeType>(TCHeader::SERIALIZED_SIZE + TCSegmentHeader::Size + TCTrailer::SERIALIZED_SIZE));

    this->setComponentState(0x44);
    this->component.configureSegmentHeader(true);
    this->assertForwarded(frame, this->m_payload, 0, 1, true, fields.segmentHeader);
}

void TcDeframerTester::testShModeMissingSh() {
    // 7-octet frame (length field 6, valid FECF) with nothing after the primary header, inside a 16-octet buffer so
    // that the buffer-size guard does not fire first. Without the check, the first FECF octet would be read as SH
    const U8 expectedFrame[] = {0x20, 0x44, 0x04, 0x06, 0x00, 0xCB, 0xB3};
    CcsdsTestUtils::TcFrameFields fields;
    fields.scid = 0x44;
    fields.vcid = 1;
    Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, 0), 16);
    ASSERT_EQ(frame.getSize(), static_cast<FwSizeType>(16));
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(expectedFrame); i++) {
        ASSERT_EQ(frame.getData()[i], expectedFrame[i]) << "octet " << i;
    }

    this->setComponentState(0x44);
    this->component.configureSegmentHeader(true);
    this->assertDroppedWithError(frame, FrameError::TC_MISSING_SEGMENT_HEADER);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_MissingSegmentHeader_SIZE(1);
    ASSERT_EVENTS_MissingSegmentHeader(0, 7);

    // The same 7 octets in an exactly-sized buffer never reach the Segment Header check: baseline InvalidPacket
    this->clearHistory();
    frame.setSize(7);
    this->assertDropped(frame);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_InvalidPacket_SIZE(1);
}

void TcDeframerTester::testShModeTypeBc() {
    this->setComponentState(0x44);
    this->component.configureSegmentHeader(true);

    // Unlock control frame: 30 44 04 07 00 00 F6 93 (Bypass=1, Control=1, SCID 0x044, VC 1, FDU 00)
    const U8 expectedFrame[] = {0x30, 0x44, 0x04, 0x07, 0x00, 0x00, 0xF6, 0x93};
    CcsdsTestUtils::TcFrameFields fields;
    fields.control = true;
    fields.scid = 0x44;
    fields.vcid = 1;
    this->m_payload[0] = 0x00;
    Fw::Buffer unlock = this->buildFrame(fields, Fw::Buffer(this->m_payload, 1));
    ASSERT_EQ(unlock.getSize(), FW_NUM_ARRAY_ELEMENTS(expectedFrame));
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(expectedFrame); i++) {
        ASSERT_EQ(unlock.getData()[i], expectedFrame[i]) << "octet " << i;
    }
    this->assertDropped(unlock);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ControlFrameDropped_SIZE(1);
    ASSERT_EVENTS_ControlFrameDropped(0, 0x3044);
    ASSERT_EVENTS_MissingSegmentHeader_SIZE(0);

    // SetV(R) control frame: FDU 82 00
    this->clearHistory();
    this->m_payload[0] = 0x82;
    this->m_payload[1] = 0x00;
    Fw::Buffer setVr = this->buildFrame(fields, Fw::Buffer(this->m_payload, 2));
    this->assertDropped(setVr);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ControlFrameDropped(0, 0x3044);

    // Control frame with no data octet at all, in a larger buffer: dropped as a control frame, octet 5 is never
    // interpreted so MissingSegmentHeader is not reached
    this->clearHistory();
    Fw::Buffer emptyControl = this->buildFrame(fields, Fw::Buffer(this->m_payload, 0), 16);
    this->assertDropped(emptyControl);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ControlFrameDropped(0, 0x3044);
    ASSERT_EVENTS_MissingSegmentHeader_SIZE(0);
}

void TcDeframerTester::testShModeTypeBcOff() {
    // Segment Header mode off: the Type-BC frame is forwarded unchanged, as the baseline deframer does
    CcsdsTestUtils::TcFrameFields fields;
    fields.control = true;
    fields.scid = 0x44;
    fields.vcid = 1;
    this->m_payload[0] = 0x00;
    Fw::Buffer unlock = this->buildFrame(fields, Fw::Buffer(this->m_payload, 1));
    ASSERT_EQ(unlock.getData()[0], 0x30);

    this->setComponentState(0x44);
    this->assertForwarded(unlock, this->m_payload, 1, 1, false, 0);
}

void TcDeframerTester::testShModeOrderOfChecks() {
    this->setComponentState(0x44);
    this->component.configureSegmentHeader(true);

    // Bad CRC + missing Segment Header: the existing CRC check fires and nothing Segment-Header-related runs
    CcsdsTestUtils::TcFrameFields fields;
    fields.scid = 0x44;
    fields.vcid = 1;
    Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, 0), 16);
    frame.getData()[6]++;  // corrupt the FECF
    this->assertDroppedWithError(frame, FrameError::TC_INVALID_CRC);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_InvalidCrc_SIZE(1);
    ASSERT_EVENTS_MissingSegmentHeader_SIZE(0);
    ASSERT_EVENTS_ControlFrameDropped_SIZE(0);

    // Bad CRC + Type-BC: InvalidCrc only
    this->clearHistory();
    fields.control = true;
    this->m_payload[0] = 0x00;
    Fw::Buffer control = this->buildFrame(fields, Fw::Buffer(this->m_payload, 1));
    control.getData()[7]++;  // corrupt the FECF
    this->assertDroppedWithError(control, FrameError::TC_INVALID_CRC);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_InvalidCrc_SIZE(1);
    ASSERT_EVENTS_ControlFrameDropped_SIZE(0);

    // Wrong SCID + Type-BC + missing Segment Header: InvalidSpacecraftId only
    this->clearHistory();
    fields.scid = 0x45;
    Fw::Buffer wrongScid = this->buildFrame(fields, Fw::Buffer(this->m_payload, 0), 16);
    this->assertDroppedWithError(wrongScid, FrameError::TC_INVALID_SCID);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_InvalidSpacecraftId_SIZE(1);
    ASSERT_EVENTS_MissingSegmentHeader_SIZE(0);
    ASSERT_EVENTS_ControlFrameDropped_SIZE(0);
}

void TcDeframerTester::testConfigureIdempotent() {
    CcsdsTestUtils::TcFrameFields fields;
    fields.vcid = 2;
    fields.withSegmentHeader = true;
    fields.segmentHeader = 0xC5;  // UNSEGMENTED, MAP 5
    const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, MAX_PAYLOAD_LENGTH));
    TcDeframerTester::fillPayload(this->m_payload, payloadLength);
    this->setComponentState();

    // Enabling twice behaves as enabling once
    this->component.configureSegmentHeader(true);
    this->component.configureSegmentHeader(true);
    Fw::Buffer frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));
    this->assertForwarded(frame, this->m_payload, payloadLength, 2, true, 0xC5);

    // Last call wins: disabling restores the baseline behaviour, the Segment Header octet is payload again
    this->component.configureSegmentHeader(false);
    frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));
    this->assertForwarded(frame, &frame.getData()[TCHeader::SERIALIZED_SIZE],
                          static_cast<FwSizeType>(payloadLength) + TCSegmentHeader::Size, 2, false, 0);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).data.getData()[0], 0xC5);

    // And enabling again strips it again
    this->component.configureSegmentHeader(true);
    frame = this->buildFrame(fields, Fw::Buffer(this->m_payload, payloadLength));
    this->assertForwarded(frame, this->m_payload, payloadLength, 2, true, 0xC5);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void TcDeframerTester::fillPayload(U8* payload, FwSizeType length) {
    for (FwSizeType i = 0; i < length; i++) {
        payload[i] = static_cast<U8>(i % std::numeric_limits<U8>::max());
    }
}

Fw::Buffer TcDeframerTester::buildFrame(const CcsdsTestUtils::TcFrameFields& fields,
                                        const Fw::Buffer& payload,
                                        FwSizeType bufferSize) {
    ::memset(this->m_frameData, 0, sizeof(this->m_frameData));
    Fw::Buffer frame(this->m_frameData, sizeof(this->m_frameData));
    const bool built = CcsdsTestUtils::buildTcFrame(fields, payload, frame);
    EXPECT_TRUE(built);
    if (bufferSize > frame.getSize()) {
        EXPECT_LE(bufferSize, sizeof(this->m_frameData));
        frame.setSize(bufferSize);
    }
    return frame;
}

void TcDeframerTester::assertForwarded(Fw::Buffer& frame,
                                       const U8* expectedPayload,
                                       FwSizeType expectedLength,
                                       U8 expectedVcId,
                                       bool expectedShPresent,
                                       U8 expectedSh) {
    this->clearHistory();
    // The component advances the buffer it is handed: remember where the frame was
    const U8* const frameStart = frame.getData();
    const FwSizeType frameSize = frame.getSize();
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataIn(0, frame, nullContext);

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(1);  // only one port call: no dataReturnOut, no errorNotify
    ASSERT_EVENTS_SIZE(0);
    const Fw::Buffer& outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), expectedLength);
    // The emitted buffer is a view into the received frame, trailer excluded
    ASSERT_GE(outBuffer.getData(), frameStart + TCHeader::SERIALIZED_SIZE);
    ASSERT_LE(outBuffer.getData() + outBuffer.getSize(), frameStart + frameSize - TCTrailer::SERIALIZED_SIZE);
    for (FwSizeType i = 0; i < expectedLength; i++) {
        ASSERT_EQ(outBuffer.getData()[i], expectedPayload[i]) << "payload octet " << i;
    }
    const ComCfg::FrameContext& outContext = this->fromPortHistory_dataOut->at(0).context;
    ASSERT_EQ(outContext.get_vcId(), expectedVcId);
    ASSERT_EQ(outContext.get_tcSegmentHeaderPresent(), expectedShPresent);
    ASSERT_EQ(outContext.get_tcSegmentHeader(), expectedSh);
    // Every other context field is untouched
    ComCfg::FrameContext expectedContext;
    expectedContext.set_vcId(expectedVcId);
    expectedContext.set_tcSegmentHeaderPresent(expectedShPresent);
    expectedContext.set_tcSegmentHeader(expectedSh);
    ASSERT_EQ(outContext, expectedContext);
}

void TcDeframerTester::assertDropped(Fw::Buffer& frame) {
    this->clearHistory();
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataIn(0, frame, nullContext);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(1);  // dataReturnOut only
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), frame.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), frame.getSize());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context, nullContext);
}

void TcDeframerTester::assertDroppedWithError(Fw::Buffer& frame, FrameError expectedError) {
    this->clearHistory();
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataIn(0, frame, nullContext);

    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(2);  // dataReturnOut and errorNotify
    ASSERT_from_errorNotify_SIZE(1);
    ASSERT_from_errorNotify(0, expectedError);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), frame.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), frame.getSize());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).context, nullContext);
}

void TcDeframerTester::setComponentState(U16 scid, U8 vcid, U8 sequenceNumber, bool acceptAllVcid) {
    this->component.configure(vcid, scid, acceptAllVcid);
}

Fw::Buffer TcDeframerTester::assembleFrameBuffer(U8* data, U8 dataLength, U16 scid, U8 vcid, U8 seqNumber) {
    ::memset(this->m_frameData, 0, sizeof(this->m_frameData));
    U16 frameLength = static_cast<U16>(TCHeader::SERIALIZED_SIZE + dataLength + TCTrailer::SERIALIZED_SIZE);
    U16 frameLengthToken = static_cast<U16>(frameLength - 1);  // length token is length - 1
    // Header
    this->m_frameData[0] = static_cast<U8>(scid >> 8);
    this->m_frameData[1] = static_cast<U8>(scid & 0xFF);
    this->m_frameData[2] = static_cast<U8>((vcid << 2) | static_cast<U8>((frameLengthToken >> 8) & 0x03));
    this->m_frameData[3] = static_cast<U8>(frameLengthToken & 0xFF);
    this->m_frameData[4] = seqNumber;

    // Data
    memcpy(&this->m_frameData[TCHeader::SERIALIZED_SIZE], data, dataLength);

    // CRC trailer
    U16 crc = Ccsds::Utils::CRC16::compute(this->m_frameData, TCHeader::SERIALIZED_SIZE + dataLength);
    this->m_frameData[TCHeader::SERIALIZED_SIZE + dataLength] = static_cast<U8>(crc >> 8);
    this->m_frameData[TCHeader::SERIALIZED_SIZE + dataLength + 1] = static_cast<U8>(crc & 0xFF);

    return Fw::Buffer(this->m_frameData, frameLength);
}

}  // namespace Ccsds
}  // namespace Svc
