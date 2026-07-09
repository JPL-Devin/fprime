// ======================================================================
// \title  ApidStripper.cpp
// \brief  cpp file for ApidStripper component implementation class
// ======================================================================

#include "Svc/ApidStripper/ApidStripper.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Types/Assert.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ApidStripper ::ApidStripper(const char* const compName) : ApidStripperComponentBase(compName) {}

ApidStripper ::~ApidStripper() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ApidStripper ::dataIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    if (fwBuffer.getSize() < sizeof(FwPacketDescriptorType)) {
        this->log_WARNING_HI_BufferTooSmall(fwBuffer.getSize());
        this->dataReturnOut_out(0, fwBuffer);
        return;
    }
    FwPacketDescriptorType packetDescriptor = 0;
    auto deserializer = fwBuffer.getDeserializer();
    Fw::SerializeStatus status = deserializer.deserializeTo(packetDescriptor);
    FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, status);
    // Invalid APID values map to INVALID_UNINITIALIZED and are left to downstream components
    ComCfg::Apid apid = ComCfg::Apid::INVALID_UNINITIALIZED;
    if (ComCfg::Apid::isValid(packetDescriptor)) {
        apid = static_cast<ComCfg::Apid::T>(packetDescriptor);
    }
    // Shift data pointer and shrink size to remove the APID field
    fwBuffer.setData(fwBuffer.getData() + sizeof(FwPacketDescriptorType));
    fwBuffer.setSize(fwBuffer.getSize() - sizeof(FwPacketDescriptorType));
    this->dataOut_out(0, fwBuffer, apid);
}

void ApidStripper ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->dataReturnOut_out(0, fwBuffer);
}

}  // namespace Svc
