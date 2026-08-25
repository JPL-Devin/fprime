// ======================================================================
// \title  UslpDeframerTester.cpp
// \author thomas-bc
// \brief  cpp file for UslpDeframer component test harness implementation class
// ======================================================================

#include "UslpDeframerTester.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/Ccsds/Types/USLPHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTfdfHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"

namespace Svc {

namespace Ccsds {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

UslpDeframerTester ::UslpDeframerTester()
    : UslpDeframerGTestBase("UslpDeframerTester", UslpDeframerTester::MAX_HISTORY_SIZE), component("UslpDeframer") {
    this->initComponents();
    this->connectPorts();
}

UslpDeframerTester ::~UslpDeframerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void UslpDeframerTester::testDataReturn() {
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

void UslpDeframerTester::testNominalDeframing() {
    FrameParams params;
    params.scid = static_cast<U16>(STest::Random::lowerUpper(0, 0xFFFF));
    params.vcid = static_cast<U8>(STest::Random::lowerUpper(0, 0x3F));
    params.mapId = static_cast<U8>(STest::Random::lowerUpper(0, 0xF));
    params.upid = static_cast<U8>(STest::Random::lowerUpper(0, 0x1F));
    const U8 payloadLength = static_cast<U8>(STest::Random::lowerUpper(1, 200));
    U8 payload[payloadLength];
    for (FwIndexType i = 0; i < payloadLength; i++) {
        payload[i] = static_cast<U8>(i % std::numeric_limits<U8>::max());
    }

    Fw::Buffer buffer = this->assembleFrameBuffer(payload, payloadLength, params);
    ComCfg::FrameContext nullContext;

    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->invoke_to_dataIn(0, buffer, nullContext);

    ASSERT_from_dataOut_SIZE(1);
    ASSERT_FROM_PORT_HISTORY_SIZE(1);  // only one port call
    Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(0).data;
    ASSERT_EQ(outBuffer.getSize(), payloadLength);
    for (FwIndexType i = 0; i < payloadLength; i++) {
        ASSERT_EQ(outBuffer.getData()[i], payload[i]);
    }
    // Output context carries the received VCID
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_vcId(), params.vcid);
    ASSERT_TLM_FramesProcessed_SIZE(1);
    ASSERT_TLM_FramesProcessed(0, 1);
}

void UslpDeframerTester::testNominalVcfCountLengths() {
    // Exercise VCF Count Length variants 0, 4 and 7
    const U8 vcfCountLengths[3] = {0, 4, 7};
    U8 payload[16];
    for (FwIndexType i = 0; i < static_cast<FwIndexType>(sizeof(payload)); i++) {
        payload[i] = static_cast<U8>(i);
    }
    for (FwIndexType variant = 0; variant < 3; variant++) {
        FrameParams params;
        params.vcfCountLength = vcfCountLengths[variant];
        Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
        ComCfg::FrameContext nullContext;

        this->component.configure(params.vcid, params.scid, params.mapId, params.vcfCountLength, false);
        this->invoke_to_dataIn(0, buffer, nullContext);

        ASSERT_from_dataOut_SIZE(variant + 1);
        Fw::Buffer outBuffer = this->fromPortHistory_dataOut->at(variant).data;
        ASSERT_EQ(outBuffer.getSize(), sizeof(payload));
        for (FwIndexType i = 0; i < static_cast<FwIndexType>(sizeof(payload)); i++) {
            ASSERT_EQ(outBuffer.getData()[i], payload[i]);
        }
    }
    ASSERT_TLM_FramesProcessed_SIZE(3);
    ASSERT_TLM_FramesProcessed(2, 3);
}

void UslpDeframerTester::testShortFrame() {
    // 10 bytes is exactly header (7) + TFDF header (1) + trailer (2): a zero-payload frame must be rejected
    U8 data[10] = {0};
    Fw::Buffer buffer(data, sizeof(data));
    this->component.configure(0, 0x0044, 0, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_LENGTH);
    ASSERT_EVENTS_InvalidPacket_SIZE(1);
}

void UslpDeframerTester::testInvalidVersion() {
    FrameParams params;
    params.tfvn = 0x1;  // invalid TFVN (USLP is 0xC)
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_VERSION);
    ASSERT_EVENTS_InvalidFrameVersion_SIZE(1);
    ASSERT_EVENTS_InvalidFrameVersion(0, 0x1, 0xC);
}

void UslpDeframerTester::testTruncatedFrame() {
    FrameParams params;
    params.eofph = 1;  // truncated frame
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_TruncatedFrameNotSupported_SIZE(1);
}

void UslpDeframerTester::testInvalidScId() {
    FrameParams params;
    params.scid = 0x0045;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, 0x0044, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_SCID);
    ASSERT_EVENTS_InvalidSpacecraftId_SIZE(1);
    ASSERT_EVENTS_InvalidSpacecraftId(0, 0x0045, 0x0044);
}

void UslpDeframerTester::testInvalidSourceOrDest() {
    FrameParams params;
    params.sourceOrDest = 0;  // spacecraft must be the destination on uplink
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_InvalidSourceOrDest_SIZE(1);
    ASSERT_EVENTS_InvalidSourceOrDest(0, 0);
}

void UslpDeframerTester::testInvalidLength() {
    FrameParams params;
    params.lengthDelta = 1;  // length field does not match the buffer size
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_LENGTH);
    ASSERT_EVENTS_InvalidFrameLength_SIZE(1);
}

void UslpDeframerTester::testLengthWrap() {
    // A frame length field of 0xFFFF must not wrap to 0 when converted to total length
    FrameParams params;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    // Override the length field with 0xFFFF; totalLen must widen to 0x10000, not wrap to 0
    buffer.getData()[4] = 0xFF;
    buffer.getData()[5] = 0xFF;
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_LENGTH);
    ASSERT_EVENTS_InvalidFrameLength_SIZE(1);
    ASSERT_EVENTS_InvalidFrameLength(0, 0xFFFF, buffer.getSize());
}

void UslpDeframerTester::testInvalidVcId() {
    FrameParams params;
    params.vcid = 5;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(4, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_VCID);
    ASSERT_EVENTS_InvalidVcId_SIZE(1);
    ASSERT_EVENTS_InvalidVcId(0, 5, 4);
}

void UslpDeframerTester::testAcceptAllVcid() {
    FrameParams params;
    params.vcid = 5;
    U8 payload[4] = {0, 1, 2, 3};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    ComCfg::FrameContext nullContext;
    // Configure a different VCID but accept all VCIDs
    this->component.configure(4, params.scid, params.mapId, 0, true);
    this->invoke_to_dataIn(0, buffer, nullContext);
    ASSERT_from_dataOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dataOut->at(0).context.get_vcId(), params.vcid);
}

void UslpDeframerTester::testInvalidMapId() {
    FrameParams params;
    params.mapId = 3;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, 2, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_MAP);
    ASSERT_EVENTS_InvalidMapId_SIZE(1);
    ASSERT_EVENTS_InvalidMapId(0, 3, 2);
}

void UslpDeframerTester::testProtocolCommand() {
    FrameParams params;
    params.protCmd = 1;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_ProtocolCommandNotSupported_SIZE(1);
}

void UslpDeframerTester::testInvalidSpares() {
    FrameParams params;
    params.spares = 0x3;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_InvalidSpareBits_SIZE(1);
    ASSERT_EVENTS_InvalidSpareBits(0, 0x3);
}

void UslpDeframerTester::testOcfFlag() {
    FrameParams params;
    params.ocf = 1;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_OcfNotSupported_SIZE(1);
}

void UslpDeframerTester::testInvalidVcfCountLength() {
    FrameParams params;
    params.vcfCountLength = 2;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_HEADER);
    ASSERT_EVENTS_InvalidVcfCountLength_SIZE(1);
    ASSERT_EVENTS_InvalidVcfCountLength(0, 2, 0);
}

void UslpDeframerTester::testInvalidCrc() {
    FrameParams params;
    params.corruptCrc = true;
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_CRC);
    ASSERT_EVENTS_InvalidCrc_SIZE(1);
    ASSERT_TLM_CrcErrorCount_SIZE(1);
    ASSERT_TLM_CrcErrorCount(0, 1);
}

void UslpDeframerTester::testInvalidTfdfRule() {
    FrameParams params;
    params.rule = 0x0;  // packets spanning multiple frames: not supported
    U8 payload[4] = {0};
    Fw::Buffer buffer = this->assembleFrameBuffer(payload, sizeof(payload), params);
    this->component.configure(params.vcid, params.scid, params.mapId, 0, false);
    this->assertReject(buffer, Svc::Ccsds::FrameError::USLP_INVALID_TFDF);
    ASSERT_EVENTS_InvalidTfdfRule_SIZE(1);
    ASSERT_EVENTS_InvalidTfdfRule(0, 0x0, 0x7);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void UslpDeframerTester::assertReject(Fw::Buffer& buffer, Svc::Ccsds::FrameError expectedError) {
    ComCfg::FrameContext nullContext;
    this->invoke_to_dataIn(0, buffer, nullContext);
    ASSERT_from_dataOut_SIZE(0);
    ASSERT_from_dataReturnOut_SIZE(1);  // invalid buffer was returned
    ASSERT_from_errorNotify_SIZE(1);
    ASSERT_from_errorNotify(0, expectedError);
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getData(), buffer.getData());
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data.getSize(), buffer.getSize());
    ASSERT_EVENTS_SIZE(1);  // exactly 1 event emitted
}

Fw::Buffer UslpDeframerTester::assembleFrameBuffer(const U8* payload,
                                                   FwSizeType payloadLength,
                                                   const FrameParams& params) {
    ::memset(this->m_frameData, 0, sizeof(this->m_frameData));
    const FwSizeType totalLength = USLPHeader::SERIALIZED_SIZE + params.vcfCountLength +
                                   USLPTfdfHeader::SERIALIZED_SIZE + payloadLength + USLPTrailer::SERIALIZED_SIZE;
    FW_ASSERT(totalLength <= sizeof(this->m_frameData), static_cast<FwAssertArgType>(totalLength));
    const U16 lengthField = static_cast<U16>(totalLength - 1 + params.lengthDelta);

    const U32 idWord = (static_cast<U32>(params.tfvn) << 28) | (static_cast<U32>(params.scid) << 12) |
                       (static_cast<U32>(params.sourceOrDest) << 11) | (static_cast<U32>(params.vcid) << 5) |
                       (static_cast<U32>(params.mapId) << 1) | static_cast<U32>(params.eofph);
    this->m_frameData[0] = static_cast<U8>(idWord >> 24);
    this->m_frameData[1] = static_cast<U8>(idWord >> 16);
    this->m_frameData[2] = static_cast<U8>(idWord >> 8);
    this->m_frameData[3] = static_cast<U8>(idWord & 0xFF);
    this->m_frameData[4] = static_cast<U8>(lengthField >> 8);
    this->m_frameData[5] = static_cast<U8>(lengthField & 0xFF);
    this->m_frameData[6] = static_cast<U8>((params.bypass << 7) | (params.protCmd << 6) | (params.spares << 4) |
                                           (params.ocf << 3) | params.vcfCountLength);
    // VCF count octets are left zeroed
    const FwSizeType tfdfOffset = static_cast<FwSizeType>(USLPHeader::SERIALIZED_SIZE) + params.vcfCountLength;
    this->m_frameData[tfdfOffset] = static_cast<U8>((params.rule << 5) | params.upid);

    // Payload
    memcpy(&this->m_frameData[tfdfOffset + USLPTfdfHeader::SERIALIZED_SIZE], payload, payloadLength);

    // CRC trailer
    U16 crc =
        Ccsds::Utils::CRC16::compute(this->m_frameData, static_cast<U32>(totalLength - USLPTrailer::SERIALIZED_SIZE));
    if (params.corruptCrc) {
        crc = static_cast<U16>(crc + 1);
    }
    this->m_frameData[totalLength - 2] = static_cast<U8>(crc >> 8);
    this->m_frameData[totalLength - 1] = static_cast<U8>(crc & 0xFF);

    return Fw::Buffer(this->m_frameData, static_cast<Fw::Buffer::SizeType>(totalLength));
}

}  // namespace Ccsds
}  // namespace Svc
