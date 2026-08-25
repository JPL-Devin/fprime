// ======================================================================
// \title  UslpFramerTester.cpp
// \author Devin
// \brief  cpp file for UslpFramer component test harness implementation class
// ======================================================================

#include "UslpFramerTester.hpp"
#include "Svc/Ccsds/Types/EppLengthOfLengthEnumAc.hpp"
#include "Svc/Ccsds/Types/EppProtocolIdEnumAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

UslpFramerTester ::UslpFramerTester()
    : UslpFramerGTestBase("UslpFramerTester", UslpFramerTester::MAX_HISTORY_SIZE),
      component("UslpFramer"),
      m_framesSent(0) {
    this->initComponents();
    this->connectPorts();
    ::memset(this->m_expectedFrame, 0, sizeof(this->m_expectedFrame));
    ::memset(this->m_payload, 0, sizeof(this->m_payload));
}

UslpFramerTester ::~UslpFramerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void UslpFramerTester ::testComStatusPassthrough() {
    Fw::Success inputStatus = Fw::Success::SUCCESS;
    this->invoke_to_comStatusIn(0, inputStatus);
    ASSERT_from_comStatusOut_SIZE(1);
    ASSERT_from_comStatusOut(0, inputStatus);  // at index 0, received SUCCESS
    inputStatus = Fw::Success::FAILURE;
    this->invoke_to_comStatusIn(0, inputStatus);
    ASSERT_from_comStatusOut_SIZE(2);
    ASSERT_from_comStatusOut(1, inputStatus);  // at index 1, received FAILURE
}

void UslpFramerTester ::testNominalFraming() {
    const U8 vcId = 5;
    const U8 mapId = 3;
    this->component.configure(vcId, mapId);
    this->frameAndVerify(100, vcId, mapId, 0);
}

void UslpFramerTester ::testVcfCountIncrementAndWrap() {
    const U8 vcId = 1;
    const U8 mapId = 0;
    this->component.configure(vcId, mapId);

    // VCF count increments per frame
    this->frameAndVerify(50, vcId, mapId, 0);
    this->frameAndVerify(50, vcId, mapId, 1);
    this->frameAndVerify(50, vcId, mapId, 2);

    // VCF count wraps around at 2^32 - 1
    this->component.m_vcFrameCount = 0xFFFFFFFFU;
    this->frameAndVerify(50, vcId, mapId, 0xFFFFFFFFU);
    this->frameAndVerify(50, vcId, mapId, 0);
}

void UslpFramerTester ::testIdleFillGaps() {
    const U8 vcId = 0;
    const U8 mapId = 0;
    this->component.configure(vcId, mapId);

    U32 expectedVcfCount = 0;
    // Gap sizes to exercise: 0 (no fill), 1 (single-octet idle packet),
    // 2 (two-octet header, no fill data), 5 (two-octet header + fill data),
    // and a large gap >= 256 (four-octet header with 2-octet length field)
    const FwSizeType gaps[5] = {0, 1, 2, 5, PAYLOAD_CAPACITY};
    for (FwSizeType i = 0; i < FW_NUM_ARRAY_ELEMENTS(gaps); i++) {
        ASSERT_GE(static_cast<FwSizeType>(PAYLOAD_CAPACITY), gaps[i]);
        this->frameAndVerify(PAYLOAD_CAPACITY - gaps[i], vcId, mapId, expectedVcfCount);
        expectedVcfCount++;
    }
}

void UslpFramerTester ::testConfigureInvalidArgs() {
    // Virtual Channel ID is 6 bits and MAP ID is 4 bits
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(0x40, 0), "UslpFramer.cpp");
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(0, 0x10), "UslpFramer.cpp");
}

void UslpFramerTester ::testInputBufferTooLarge() {
    const FwSizeType tooLargeSize = ComCfg::UslpFrameFixedSize;  // Larger than the payload capacity
    Fw::Buffer buffer(this->m_payload, tooLargeSize);
    ComCfg::FrameContext defaultContext;
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_dataIn(0, buffer, defaultContext), "UslpFramer.cpp");
}

void UslpFramerTester ::testDataReturn() {
    U8 bufferData[10];
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    ComCfg::FrameContext defaultContext;
    // Send a buffer that is not the internal buffer of the component, and expect an assertion
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_dataReturnIn(0, buffer, defaultContext), "UslpFramer.cpp");

    // Now send the expected buffer and expect state to go back to OWNED
    this->component.m_bufferState = UslpFramer::BufferOwnershipState::NOT_OWNED;
    Fw::Buffer internalBuffer(this->component.m_frameBuffer, sizeof(this->component.m_frameBuffer));
    this->invoke_to_dataReturnIn(0, internalBuffer, defaultContext);
    ASSERT_EQ(this->component.m_bufferState, UslpFramer::BufferOwnershipState::OWNED);
}

void UslpFramerTester ::testBufferOwnershipState() {
    U8 bufferData[10];
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    ComCfg::FrameContext context;
    // force state to be NOT_OWNED and test that assertion is triggered
    this->component.m_bufferState = UslpFramer::BufferOwnershipState::NOT_OWNED;
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_dataIn(0, buffer, context), "UslpFramer.cpp");
    this->component.m_bufferState = UslpFramer::BufferOwnershipState::OWNED;
    this->invoke_to_dataIn(0, buffer, context);  // this should work now
    ASSERT_EQ(this->component.m_bufferState, UslpFramer::BufferOwnershipState::NOT_OWNED);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void UslpFramerTester ::buildExpectedFrame(const U8* payload, FwSizeType payloadSize, U8 vcId, U8 mapId, U32 vcfCount) {
    ::memset(this->m_expectedFrame, 0, sizeof(this->m_expectedFrame));
    U8* frame = this->m_expectedFrame;

    // Primary header octets 0-3: 4b TFVN | 16b SCID | 1b source-or-dest | 6b VCID | 4b MAP ID | 1b EOFPH
    const U32 tfvnScidVcidMap =
        (static_cast<U32>(USLPHeaderSubfields::frameVersionValue) << USLPHeaderSubfields::frameVersionOffset) |
        (static_cast<U32>(ComCfg::SpacecraftId) << USLPHeaderSubfields::spacecraftIdOffset) |
        (static_cast<U32>(vcId) << USLPHeaderSubfields::virtualChannelIdOffset) |
        (static_cast<U32>(mapId) << USLPHeaderSubfields::mapIdOffset);
    frame[0] = static_cast<U8>(tfvnScidVcidMap >> 24);
    frame[1] = static_cast<U8>(tfvnScidVcidMap >> 16);
    frame[2] = static_cast<U8>(tfvnScidVcidMap >> 8);
    frame[3] = static_cast<U8>(tfvnScidVcidMap);

    // Octets 4-5: Frame Length = total frame octets minus 1
    const U16 frameLength = static_cast<U16>(ComCfg::UslpFrameFixedSize - 1);
    frame[4] = static_cast<U8>(frameLength >> 8);
    frame[5] = static_cast<U8>(frameLength);

    // Octet 6: flags - bypass=1 (Expedited), protCmd=0, spares=00, OCF=0, vcfCountLen=4
    frame[6] = static_cast<U8>((0x1 << USLPHeaderSubfields::bypassFlagOffset) |
                               (0x4 << USLPHeaderSubfields::vcfCountLengthOffset));

    // Octets 7-10: VCF count, big-endian
    frame[7] = static_cast<U8>(vcfCount >> 24);
    frame[8] = static_cast<U8>(vcfCount >> 16);
    frame[9] = static_cast<U8>(vcfCount >> 8);
    frame[10] = static_cast<U8>(vcfCount);

    // Octet 11: TFDF header - construction rule 000, UPID 0b00000
    frame[11] = static_cast<U8>((USLPTfdfSubfields::RULE_PACKETS_SPANNING << USLPTfdfSubfields::rulesOffset) |
                                USLPTfdfSubfields::UPID_SPACE_PACKETS);

    // Octets 12-13: First Header Pointer = 0
    frame[12] = 0;
    frame[13] = 0;

    // TFDZ: payload
    const FwSizeType tfdzStart = 14;
    ::memcpy(&frame[tfdzStart], payload, payloadSize);

    // TFDZ: Encapsulation Idle Packet fill
    const FwSizeType fecfStart = ComCfg::UslpFrameFixedSize - 2;
    FwSizeType index = tfdzStart + payloadSize;
    const FwSizeType gap = fecfStart - index;
    const U8 idleFirstOctet =
        static_cast<U8>((ComCfg::Pvn::ENCAPSULATION_PACKET_PROTOCOL << EPPSubfields::packetVersionOffset) |
                        (EppProtocolId::Idle << EPPSubfields::protocolIdOffset));
    if (gap == 1) {
        frame[index++] = idleFirstOctet | static_cast<U8>(EppLengthOfLength::Zero);
    } else if ((gap >= 2) && (gap <= 0xFF)) {
        frame[index++] = idleFirstOctet | static_cast<U8>(EppLengthOfLength::One);
        frame[index++] = static_cast<U8>(gap);
    } else if (gap > 0xFF) {
        frame[index++] = idleFirstOctet | static_cast<U8>(EppLengthOfLength::Two);
        frame[index++] = 0x00;
        frame[index++] = static_cast<U8>(gap >> 8);
        frame[index++] = static_cast<U8>(gap);
    }
    while (index < fecfStart) {
        frame[index++] = UslpFramer::IDLE_DATA_PATTERN;
    }

    // FECF: CRC16 over everything before it
    const U16 crc = Ccsds::Utils::CRC16::compute(frame, static_cast<U32>(fecfStart));
    frame[fecfStart] = static_cast<U8>(crc >> 8);
    frame[fecfStart + 1] = static_cast<U8>(crc);
}

void UslpFramerTester ::frameAndVerify(FwSizeType payloadSize, U8 vcId, U8 mapId, U32 expectedVcfCount) {
    ASSERT_LE(payloadSize, static_cast<FwSizeType>(PAYLOAD_CAPACITY));
    for (FwSizeType i = 0; i < payloadSize; i++) {
        this->m_payload[i] = static_cast<U8>(i & 0xFF);
    }
    Fw::Buffer buffer(this->m_payload, payloadSize);
    ComCfg::FrameContext defaultContext;

    this->invoke_to_dataIn(0, buffer, defaultContext);

    const U32 frameIndex = this->m_framesSent;
    this->m_framesSent++;

    // The frame was emitted and the input buffer was returned to the sender
    ASSERT_from_dataOut_SIZE(frameIndex + 1);
    ASSERT_from_dataReturnOut_SIZE(frameIndex + 1);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(frameIndex).data.getData(), this->m_payload);

    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(frameIndex).data;
    ASSERT_EQ(outBuffer.getSize(), ComCfg::UslpFrameFixedSize);

    // Byte-exact comparison against the hand-built expected frame
    this->buildExpectedFrame(this->m_payload, payloadSize, vcId, mapId, expectedVcfCount);
    for (FwSizeType i = 0; i < ComCfg::UslpFrameFixedSize; i++) {
        ASSERT_EQ(outBuffer.getData()[i], this->m_expectedFrame[i]) << "Frame mismatch at octet " << i;
    }

    // Verify the frame parses back: FECF matches a CRC16 recomputed over the received frame
    const FwSizeType fecfStart = ComCfg::UslpFrameFixedSize - 2;
    const U16 recomputedCrc = Ccsds::Utils::CRC16::compute(outBuffer.getData(), static_cast<U32>(fecfStart));
    const U16 receivedCrc =
        static_cast<U16>((outBuffer.getData()[fecfStart] << 8) | outBuffer.getData()[fecfStart + 1]);
    ASSERT_EQ(receivedCrc, recomputedCrc);

    // Return the frame buffer for the next iteration (buffer ownership round trip)
    ASSERT_EQ(this->component.m_bufferState, UslpFramer::BufferOwnershipState::NOT_OWNED);
    this->invoke_to_dataReturnIn(0, outBuffer, defaultContext);
    ASSERT_EQ(this->component.m_bufferState, UslpFramer::BufferOwnershipState::OWNED);
}

}  // namespace Ccsds

}  // namespace Svc
