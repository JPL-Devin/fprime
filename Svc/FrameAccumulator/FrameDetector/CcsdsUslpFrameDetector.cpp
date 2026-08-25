// ======================================================================
// \title  CcsdsUslpFrameDetector.cpp
// \author thomas-bc
// \brief  cpp file for CCSDS USLP frame detector definitions
// ======================================================================

#include "Svc/FrameAccumulator/FrameDetector/CcsdsUslpFrameDetector.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/USLPHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTfdfHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/USLPTrailerSerializableAc.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "config/FppConstantsAc.hpp"

namespace Svc {
namespace FrameDetectors {

FrameDetector::Status CcsdsUslpFrameDetector::detect(const Types::CircularBuffer& data, FwSizeType& size_out) const {
    // Minimum size of a valid frame: primary header + TFDF header + trailer (FECF)
    const FwSizeType minimum_frame_size = static_cast<FwSizeType>(Ccsds::USLPHeader::SERIALIZED_SIZE) +
                                          Ccsds::USLPTfdfHeader::SERIALIZED_SIZE + Ccsds::USLPTrailer::SERIALIZED_SIZE;
    if (data.get_allocated_size() < Ccsds::USLPHeader::SERIALIZED_SIZE) {
        size_out = minimum_frame_size;
        return Status::MORE_DATA_NEEDED;
    }

    // ---------------- Frame Header ----------------
    // Copy CircularBuffer data into linear buffer, for serialization into FrameHeader object
    U8 header_data[Ccsds::USLPHeader::SERIALIZED_SIZE];
    Fw::SerializeStatus status = data.peek(header_data, Ccsds::USLPHeader::SERIALIZED_SIZE, 0);
    if (status != Fw::FW_SERIALIZE_OK) {
        return Status::NO_FRAME_DETECTED;
    }
    Fw::ExternalSerializeBuffer header_ser_buffer(header_data, Ccsds::USLPHeader::SERIALIZED_SIZE);
    status = header_ser_buffer.setBuffLen(Ccsds::USLPHeader::SERIALIZED_SIZE);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    // Attempt to deserialize data into the FrameHeader object
    Ccsds::USLPHeader header;
    status = header.deserializeFrom(header_ser_buffer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    const U32 id_word = header.get_tfvnScidVcidMap();
    if ((id_word & this->m_expectedTokenMask) != this->m_expectedHeaderToken) {
        // If the TFVN and spacecraft ID do not match the expected token, we don't have a valid frame
        return Status::NO_FRAME_DETECTED;
    }
    if ((id_word & Ccsds::USLPHeaderSubfields::eofphMask) != 0) {
        // Truncated frames (End of Frame Primary Header flag set) are not supported
        return Status::NO_FRAME_DETECTED;
    }

    // USLP defines the Frame Length as total octets minus 1; widen before adding 1 back to avoid U16 wrap
    const FwSizeType expected_frame_length = static_cast<FwSizeType>(header.get_frameLength()) + 1;

    // Validate frame length bounds BEFORE computing data_to_crc_length.
    // If expected_frame_length were smaller than the trailer size the subtraction below would
    // underflow to a near-maximum value, causing the CRC loop to read far beyond the valid frame data.
    if (expected_frame_length < minimum_frame_size) {
        return Status::NO_FRAME_DETECTED;
    }
    // A frame larger than the circular buffer capacity can never be fully accumulated
    if (expected_frame_length > data.get_capacity()) {
        return Status::NO_FRAME_DETECTED;
    }

    // If the full frame is not yet available, report that more data is needed
    if (data.get_allocated_size() < expected_frame_length) {
        size_out = expected_frame_length;
        return Status::MORE_DATA_NEEDED;
    }
    // Safe: the guard above guarantees expected_frame_length >= TRAILER_SIZE, so this
    // subtraction cannot underflow.
    const FwSizeType data_to_crc_length = expected_frame_length - Ccsds::USLPTrailer::SERIALIZED_SIZE;

    // ---------------- Frame Trailer ----------------
    // Compute CRC on the received data
    Ccsds::Utils::CRC16 crc;
    for (FwSizeType i = 0; i < data_to_crc_length; ++i) {
        U8 byte = 0;
        status = data.peek(byte, i);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        crc.update(byte);
    }
    U16 computed_fecf = crc.finalize();
    // Retrieve CRC field from the trailer
    U8 trailer_data[Ccsds::USLPTrailer::SERIALIZED_SIZE];
    status = data.peek(trailer_data, Ccsds::USLPTrailer::SERIALIZED_SIZE, data_to_crc_length);
    if (status != Fw::FW_SERIALIZE_OK) {
        return Status::NO_FRAME_DETECTED;
    }
    Fw::ExternalSerializeBuffer trailer_ser_buffer(trailer_data, Ccsds::USLPTrailer::SERIALIZED_SIZE);
    status = trailer_ser_buffer.setBuffLen(Ccsds::USLPTrailer::SERIALIZED_SIZE);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    // Attempt to deserialize data into the FrameTrailer object
    Ccsds::USLPTrailer trailer;
    status = trailer.deserializeFrom(trailer_ser_buffer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    U16 transmitted_fecf = trailer.get_fecf();
    if (transmitted_fecf != computed_fecf) {
        // If the computed CRC does not match the transmitted CRC, we don't have a valid frame
        return Status::NO_FRAME_DETECTED;
    }
    // At this point, we have validated the header and CRC - we report a valid frame detected
    size_out = expected_frame_length;
    return Status::FRAME_DETECTED;
}

}  // namespace FrameDetectors
}  // namespace Svc
