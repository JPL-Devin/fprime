// ======================================================================
// \title  ApidPrependerTester.cpp
// \brief  cpp file for ApidPrepender component test harness implementation class
// ======================================================================

#include "ApidPrependerTester.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ApidPrependerTester ::ApidPrependerTester()
    : ApidPrependerGTestBase("ApidPrependerTester", ApidPrependerTester::MAX_HISTORY_SIZE), component("ApidPrepender") {
    this->initComponents();
    this->connectPorts();
}

ApidPrependerTester ::~ApidPrependerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ApidPrependerTester ::testNominalPrepend() {
    U8 bufferData[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    ComCfg::Apid apid = ComCfg::Apid::FW_PACKET_FILE;

    this->invoke_to_dataIn(0, buffer, apid);

    ASSERT_from_allocate_SIZE(1);
    ASSERT_from_dataOut_SIZE(1);        // APID-prepended buffer emitted
    ASSERT_from_dataReturnOut_SIZE(1);  // original buffer ownership returned
    ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).fwBuffer.getData(), bufferData);
    ASSERT_EVENTS_SIZE(0);

    Fw::Buffer outputBuffer = this->fromPortHistory_dataOut->at(0).fwBuffer;
    ASSERT_EQ(outputBuffer.getSize(), sizeof(bufferData) + sizeof(FwPacketDescriptorType));
    FwPacketDescriptorType packetDescriptor = 0;
    auto deserializer = outputBuffer.getDeserializer();
    ASSERT_EQ(deserializer.deserializeTo(packetDescriptor), Fw::SerializeStatus::FW_SERIALIZE_OK);
    ASSERT_EQ(packetDescriptor, static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_FILE));
    for (U32 i = 0; i < sizeof(bufferData); ++i) {
        ASSERT_EQ(outputBuffer.getData()[i + sizeof(FwPacketDescriptorType)], bufferData[i]);
    }
}

void ApidPrependerTester ::testAllocationFailure() {
    U8 bufferData[8] = {0};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    ComCfg::Apid apid = ComCfg::Apid::FW_PACKET_FILE;

    this->m_failAllocation = true;
    this->invoke_to_dataIn(0, buffer, apid);
    this->m_failAllocation = false;

    ASSERT_from_dataOut_SIZE(0);        // nothing emitted
    ASSERT_from_dataReturnOut_SIZE(1);  // original buffer ownership returned
    ASSERT_from_deallocate_SIZE(0);     // empty allocation is not deallocated
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_AllocationFailed_SIZE(1);
}

void ApidPrependerTester ::testDataOutReturn() {
    U8 bufferData[8] = {0};
    Fw::Buffer buffer(bufferData, sizeof(bufferData));
    this->invoke_to_dataOutReturn(0, buffer);
    ASSERT_from_deallocate_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_deallocate->at(0).fwBuffer.getData(), bufferData);
}

// ----------------------------------------------------------------------
// Test Harness: Handler implementations for output ports
// ----------------------------------------------------------------------

Fw::Buffer ApidPrependerTester ::from_allocate_handler(FwIndexType portNum, FwSizeType size) {
    this->pushFromPortEntry_allocate(size);
    if (this->m_failAllocation) {
        return Fw::Buffer();
    }
    this->m_buffer.setData(this->m_buffer_slot);
    this->m_buffer.setSize(static_cast<Fw::Buffer::SizeType>(size));
    ::memset(this->m_buffer.getData(), 0, size);
    return this->m_buffer;
}

}  // namespace Svc
