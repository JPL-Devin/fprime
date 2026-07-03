// ======================================================================
// \title  FprimeDeframer.cpp
// \author thomas-bc
// \brief  cpp file for FprimeDeframer component implementation class
// ======================================================================

#include "Svc/FprimeDeframer/FprimeDeframer.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Types/Assert.hpp"

#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"

#include <limits>

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FprimeDeframer ::FprimeDeframer(const char* const compName) : FprimeDeframerComponentBase(compName) {}

FprimeDeframer ::~FprimeDeframer() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void FprimeDeframer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    if (data.getSize() < FprimeProtocol::FrameHeader::SERIALIZED_SIZE + FprimeProtocol::FrameTrailer::SERIALIZED_SIZE) {
        // Incoming buffer is not long enough to contain a valid frame (header+trailer)
        this->log_WARNING_HI_InvalidBufferReceived();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    // Trailer object to hold the deserialized data (type is autocoded by FPP)
    FprimeProtocol::FrameTrailer trailer;

    // ---------------- Validate Frame Header ----------------
    // Deserialize the transmitted header field by field. The apid field is read as a raw
    // FwPacketDescriptorType so that values outside the ComCfg::Apid enumeration are
    // tolerated (they map to FW_PACKET_UNKNOWN) instead of being rejected
    auto deserializer = data.getDeserializer();
    FprimeProtocol::TokenType startWord = 0;
    FprimeProtocol::TokenType lengthField = 0;
    FwPacketDescriptorType packetDescriptor = 0;
    Fw::SerializeStatus status = deserializer.deserializeTo(startWord);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    status = deserializer.deserializeTo(lengthField);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    status = deserializer.deserializeTo(packetDescriptor);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    // Check that deserialized start_word token matches expected value (default start_word value in the FPP object)
    const FprimeProtocol::FrameHeader defaultValue;
    if (startWord != defaultValue.get_startWord()) {
        this->log_WARNING_HI_InvalidStartWord();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    // The length field accounts for the apid field and the payload
    if (lengthField < sizeof(FwPacketDescriptorType)) {
        this->log_WARNING_HI_InvalidLengthReceived();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    static_assert(FprimeProtocol::FrameHeader::SERIALIZED_SIZE <=
                      std::numeric_limits<FwSizeType>::max() - FprimeProtocol::FrameTrailer::SERIALIZED_SIZE,
                  "FrameHeader::SERIALIZED_SIZE + FrameTrailer::SERIALIZED_SIZE overflows FwSizeType");
    constexpr FwSizeType headerTrailerOverhead =
        FprimeProtocol::FrameHeader::SERIALIZED_SIZE + FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    // Guard: reject frames whose declared length would overflow FwSizeType when added to the fixed overhead
    if (lengthField > std::numeric_limits<FwSizeType>::max() - headerTrailerOverhead) {
        this->log_WARNING_HI_InvalidLengthReceived();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    // We expect the frame size to be size of header (which accounts for the apid field) + payload + trailer
    const FwSizeType expectedFrameSize =
        (static_cast<FwSizeType>(lengthField) - sizeof(FwPacketDescriptorType)) + headerTrailerOverhead;
    // Reject packets whose data does not match the header
    if (data.getSize() != expectedFrameSize) {
        this->log_WARNING_HI_InvalidLengthReceived();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }
    // -------- Extract APID from header --------
    ComCfg::FrameContext contextCopy = context;
    // If the APID is not a valid ComCfg::Apid value, let it default to FW_PACKET_UNKNOWN
    // and let downstream components (e.g. custom router) handle it
    if (packetDescriptor < ComCfg::Apid::INVALID_UNINITIALIZED) {
        contextCopy.set_apid(static_cast<ComCfg::Apid::T>(packetDescriptor));
    }

    // ---------------- Validate Frame Trailer ----------------
    // Deserialize transmitted trailer: trailer is at offset = frame size - len(trailer)
    status = deserializer.moveDeserToOffset(expectedFrameSize - FprimeProtocol::FrameTrailer::SERIALIZED_SIZE);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    status = trailer.deserializeFrom(deserializer);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    // Compute CRC over the transmitted data (header + payload)
    Utils::Hash hash;
    Utils::HashBuffer computedCrc;
    FwSizeType fieldToHashSize = expectedFrameSize - FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    hash.init();
    // Add byte by byte to the hash
    for (FwSizeType i = 0; i < fieldToHashSize; i++) {
        hash.update(data.getData() + i, 1);
    }
    hash.finalize(computedCrc);
    // Check that the CRC in the trailer of the frame matches the computed CRC
    if (trailer.get_crcField() != computedCrc.asBigEndianU32()) {
        this->log_WARNING_HI_InvalidChecksum();
        this->dataReturnOut_out(0, data, context);  // drop the frame
        return;
    }

    // ---------------- Extract payload from frame ----------------
    // Shift data pointer to effectively remove the header
    data.setData(data.getData() + FprimeProtocol::FrameHeader::SERIALIZED_SIZE);
    // Shrink size to effectively remove the trailer (also removes the header)
    data.setSize(data.getSize() - FprimeProtocol::FrameHeader::SERIALIZED_SIZE -
                 FprimeProtocol::FrameTrailer::SERIALIZED_SIZE);
    // Emit the deframed data
    this->dataOut_out(0, data, contextCopy);
}

void FprimeDeframer ::dataReturnIn_handler(FwIndexType portNum,
                                           Fw::Buffer& fwBuffer,
                                           const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, fwBuffer, context);
}

}  // namespace Svc
