// ======================================================================
// \title  ApidPrepender.cpp
// \brief  cpp file for ApidPrepender component implementation class
// ======================================================================

#include "Svc/ApidPrepender/ApidPrepender.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Types/Assert.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ApidPrepender ::ApidPrepender(const char* const compName) : ApidPrependerComponentBase(compName) {}

ApidPrepender ::~ApidPrepender() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ApidPrepender ::dataIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const ComCfg::Apid& apid) {
    const FwSizeType outSize = fwBuffer.getSize() + sizeof(FwPacketDescriptorType);
    Fw::Buffer outBuffer = this->allocate_out(0, outSize);
    if (outBuffer.getSize() < outSize) {
        this->log_WARNING_HI_AllocationFailed(outSize);
        if (outBuffer.getData() != nullptr) {
            this->deallocate_out(0, outBuffer);
        }
        this->dataReturnOut_out(0, fwBuffer);
        return;
    }
    auto serializer = outBuffer.getSerializer();
    Fw::SerializeStatus status = serializer.serializeFrom(static_cast<FwPacketDescriptorType>(apid.e));
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    status = serializer.serializeFrom(fwBuffer.getData(), fwBuffer.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    outBuffer.setSize(static_cast<Fw::Buffer::SizeType>(outSize));
    this->dataOut_out(0, outBuffer);
    // Return ownership of the original buffer to its sender
    this->dataReturnOut_out(0, fwBuffer);
}

void ApidPrepender ::dataOutReturn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->deallocate_out(0, fwBuffer);
}

}  // namespace Svc
